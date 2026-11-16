// BS2 SSAO (SEPPMA Phase 3, Claude for Isa, 15 Nov 2026).
// Pass 1: normal-free crease AO from linear depth into _rt_BS2_SSAO (R = AO, GB = 16-bit depth), optional half resolution.
// Pass 2: 4x4 depth-aware blur (cancels the 4x4 interleaved rotation pattern) + multiply into the scene.
#include "cbase.h"
#include "bs2_ssao.h"
#include "view_scene.h"
#include "viewrender.h"
#include "rendertexture.h"
#include "viewpostprocess.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier0/memdbgon.h"

static ConVar bs2_ssao( "bs2_ssao", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "BS2 screen-space ambient occlusion (0 off, 1 on)", true, 0, true, 1 );
static ConVar bs2_ssao_strength( "bs2_ssao_strength", "1.2", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO darkening strength", true, 0, true, 2 );
static ConVar bs2_ssao_radius( "bs2_ssao_radius", "24", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO radius in world units", true, 2, true, 256 );
static ConVar bs2_ssao_falloff( "bs2_ssao_falloff", "6", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crease depth (units) that gives full occlusion per pair", true, 0.5f, true, 256 );
static ConVar bs2_ssao_maxdist( "bs2_ssao_maxdist", "2500", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO fades out by this distance (units)", true, 64, true, 65536 );
static ConVar bs2_ssao_half( "bs2_ssao_half", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Compute AO at half resolution", true, 0, true, 1 );
static ConVar bs2_ssao_blur( "bs2_ssao_blur", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Depth-aware 4x4 blur of the AO term", true, 0, true, 1 );
static ConVar bs2_ssao_debug( "bs2_ssao_debug", "0", FCVAR_CLIENTDLL, "1 = show the AO term, 2 = show the bounce light, 3 = show depth bands, 4 = print depth samples, 5 = AO pass diagnostics (R occlusion x4, G fade, B radius), 7 = depth as the AO pass sees it (R z/4096, G z/512, B 32/z)", true, 0, true, 7 );
// v3 (Claude for Isa, 16 Nov 2026)
static ConVar bs2_ssao_legacy_depth( "bs2_ssao_legacy_depth", "0", FCVAR_CLIENTDLL, "Comparison only: 1 = read depth the pre-v3 way (sRGB-decoded, wrong)", true, 0, true, 1 );
static ConVar bs2_ssao_depth_half( "bs2_ssao_depth_half", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Render the SSAO depth pre-pass at half resolution (only used with bs2_ssao_half 1)", true, 0, true, 1 );
static ConVar bs2_ssao_depth_cull( "bs2_ssao_depth_cull", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Depth pre-pass skips props/entities beyond bs2_ssao_maxdist (or opaque fog end) and small props beyond 35% of it", true, 0, true, 1 );
static ConVar bs2_ssao_bench_repeat( "bs2_ssao_bench_repeat", "8", FCVAR_CLIENTDLL, "bs2_ssao_bench: draw each SSAO pass this many times per frame to amplify its cost", true, 1, true, 32 );
static float s_flOpaqueFogEnd = 0; // last frame's fog end when the fog is (nearly) opaque, else 0

int BS2_SSAODepthScale() { return ( bs2_ssao_half.GetBool() && bs2_ssao_depth_half.GetBool() ) ? 2 : 1; }
float BS2_SSAODepthCullDist()
{
	if ( !bs2_ssao_depth_cull.GetBool() ) return 0;
	float d = bs2_ssao_maxdist.GetFloat();
	if ( s_flOpaqueFogEnd > 0 ) d = MIN( d, s_flOpaqueFogEnd );
	return d + 2.0f * bs2_ssao_radius.GetFloat();
}
static ConVar bs2_ssao_depthfar( "bs2_ssao_depth_scale", "0", FCVAR_CLIENTDLL, "Depth scale: 0 = view zFar (the depth pass stores depth / zFar)", true, 0, true, 100000 );
static ConVar bs2_ssgi( "bs2_ssgi", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "One-bounce screen-space indirect light (needs bs2_ssao 1)", true, 0, true, 1 );
static ConVar bs2_ssgi_strength( "bs2_ssgi_strength", "0.35", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Bounce light strength", true, 0, true, 2 );
static ConVar bs2_ssgi_radius( "bs2_ssgi_radius", "64", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Bounce gather radius in world units", true, 4, true, 512 );

bool BS2_SSAOEnabled()
{
	return bs2_ssao.GetBool() && g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() && !engine->IsHammerRunning();
}

static void SSAOFail( const char *why )
{
	static const char *s_last = NULL;
	if ( s_last != why ) { Warning( "bs2_ssao: skipped (%s)\n", why ); s_last = why; }
}

static void SetC( IMaterial *m, int reg, int comp, float v )
{
	char name[16];
	Q_snprintf( name, sizeof(name), "$c%d_%c", reg, "xyzw"[comp] );
	bool found = false;
	IMaterialVar *var = m->FindVar( name, &found, false );
	if ( found ) var->SetFloatValue( v );
}

extern float BS2_GetWorldFogMaxDensity(); // viewrender.cpp
// bs2_ssao_bench v3: no GPU sync at all (the old ReadPixels sync could hang the engine). Measures wall-clock frame time over
// phases: SSAO off | depth pre-pass x R only | depth x R + AO x R (R = bs2_ssao_bench_repeat), then (dT / R) per pass.
// Needs fps_max 0, mat_vsync 0, engine_no_focus_sleep 0. The image is over-darkened while it runs.
enum { BENCH_IDLE = 0, BENCH_OFF, BENCH_DEPTH, BENCH_FULL, BENCH_OFF2, BENCH_PHASES };
static int s_nBenchPhase = BENCH_IDLE, s_nBenchFrame = 0, s_nBenchFrames = 200;
static double s_flBenchLast = 0, s_flBenchSum[BENCH_PHASES];
static void CC_BS2_SSAO_Bench( const CCommand &args )
{
	s_nBenchFrames = args.ArgC() > 1 ? clamp( atoi( args[1] ), 20, 5000 ) : 300;
	s_nBenchPhase = BENCH_OFF; s_nBenchFrame = 0; s_flBenchLast = 0;
	for ( int i = 0; i < BENCH_PHASES; ++i ) s_flBenchSum[i] = 0;
}
static ConCommand bs2_ssao_bench( "bs2_ssao_bench", CC_BS2_SSAO_Bench, "Time the SSAO depth pre-pass and AO passes from frame times: bs2_ssao_bench [frames per phase] (no GPU sync; image over-darkened while running)" );
bool BS2_SSAOBenchPending() { return false; } // legacy hook in viewrender.cpp, unused
// called once per main view, before the depth pass
void BS2_SSAOBenchFrame()
{
	if ( s_nBenchPhase == BENCH_IDLE ) return;
	const double now = Plat_FloatTime();
	const int warm = 30;
	if ( s_flBenchLast > 0 && s_nBenchFrame >= warm ) s_flBenchSum[s_nBenchPhase] += now - s_flBenchLast;
	s_flBenchLast = now;
	if ( ++s_nBenchFrame >= warm + s_nBenchFrames )
	{
		s_nBenchFrame = 0; s_flBenchLast = 0;
		if ( ++s_nBenchPhase >= BENCH_PHASES )
		{
			const double n = s_nBenchFrames, R = bs2_ssao_bench_repeat.GetInt();
			const double off = 0.5 * ( s_flBenchSum[BENCH_OFF] + s_flBenchSum[BENCH_OFF2] ) / n * 1000.0;
			const double dep = s_flBenchSum[BENCH_DEPTH] / n * 1000.0, full = s_flBenchSum[BENCH_FULL] / n * 1000.0;
			Msg( "bs2_ssao_bench: off %.3f ms (off2 %.3f) | depth x%d %.3f | full x%d %.3f ms/frame -> depth pre-pass %.3f ms, AO+blur %.3f ms, total %.3f ms per frame [half %d depth_half %d cull %d, %d frames/phase]\n",
				s_flBenchSum[BENCH_OFF] / n * 1000.0, s_flBenchSum[BENCH_OFF2] / n * 1000.0, int( R ), dep, int( R ), full,
				( dep - off ) / R, ( full - dep ) / R, ( full - off ) / R,
				bs2_ssao_half.GetInt(), BS2_SSAODepthScale() > 1 ? 1 : 0, bs2_ssao_depth_cull.GetInt(), s_nBenchFrames );
			s_nBenchPhase = BENCH_IDLE;
		}
	}
}
// passes to draw this frame (bench phases override)
int BS2_SSAODepthRepeat()
{
	if ( s_nBenchPhase == BENCH_OFF || s_nBenchPhase == BENCH_OFF2 ) return 0;
	if ( s_nBenchPhase == BENCH_DEPTH || s_nBenchPhase == BENCH_FULL ) return bs2_ssao_bench_repeat.GetInt();
	return 1;
}
static int SSAOAORepeat()
{
	if ( s_nBenchPhase == BENCH_OFF || s_nBenchPhase == BENCH_OFF2 || s_nBenchPhase == BENCH_DEPTH ) return 0;
	if ( s_nBenchPhase == BENCH_FULL ) return bs2_ssao_bench_repeat.GetInt();
	return 1;
}

static void DrawSSAOOnce( const CViewSetup &view )
{
	IMaterial *aoMat = materials->FindMaterial( bs2_ssao_legacy_depth.GetBool() ? "effects/bs2_ssao_legacydepth" : "effects/bs2_ssao", TEXTURE_GROUP_CLIENT_EFFECTS );
	IMaterial *blurMat = materials->FindMaterial( "effects/bs2_ssao_blur", TEXTURE_GROUP_CLIENT_EFFECTS );
	if ( !aoMat || aoMat->IsErrorMaterial() ) { SSAOFail( "effects/bs2_ssao material missing" ); return; }
	if ( !blurMat || blurMat->IsErrorMaterial() ) { SSAOFail( "effects/bs2_ssao_blur material missing" ); return; }
	CMaterialReference aoRef, blurRef; // keep them referenced while bound
	aoRef.Init( aoMat );
	blurRef.Init( blurMat );
	ITexture *depth = materials->FindTexture( "_rt_ResolvedFullFrameDepth", TEXTURE_GROUP_RENDER_TARGET );
	// SP 2013 never calls the client's InitClientRenderTargets, so reuse engine targets:
	// AO + packed depth -> _rt_FullFrameFB1 (frame-buffer size, RGBA8), bounce colour -> _rt_SmallFB1 (quarter size).
	ITexture *aoRT = GetFullFrameFrameBufferTexture( 1 );
	ITexture *giRT = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
	if ( !depth || depth->IsError() ) { SSAOFail( "_rt_ResolvedFullFrameDepth missing" ); return; }
	if ( !aoRT || aoRT->IsError() ) { SSAOFail( "_rt_FullFrameFB1 missing" ); return; }
	ITexture *frame = GetFullFrameFrameBufferTexture( 0 );
	if ( !frame || frame->IsError() ) return;

	const float tanX = tanf( DEG2RAD( view.fov * 0.5f ) );
	const float aspect = view.m_flAspectRatio > 0 ? view.m_flAspectRatio : float( view.width ) / view.height;
	const float tanY = tanX / aspect;
	const int dw = depth->GetActualWidth(), dh = depth->GetActualHeight();
	const int scale = bs2_ssao_half.GetBool() ? 2 : 1;
	const int aw = MAX( 1, view.width / scale ), ah = MAX( 1, view.height / scale );
	const int ds = BS2_SSAODepthScale(); // depth pre-pass resolution divisor (v3)
	const int dx0 = view.x / ds, dy0 = view.y / ds, dx1 = dx0 + MAX( 1, view.width / ds ) - 1, dy1 = dy0 + MAX( 1, view.height / ds ) - 1;

	// Snapshot the lit scene into _rt_FullFrameFB once, before anything touches the copy targets
	// (later UpdateScreenEffectTexture calls can reuse _rt_FullFrameFB1, which holds our AO).
	UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height );

	// Pass 1: AO + packed depth into the AO target (top-left aw x ah)
	SetC( aoMat, 0, 0, 1.0f / dw );
	SetC( aoMat, 0, 1, 1.0f / dh );
	SetC( aoMat, 0, 2, bs2_ssao_strength.GetFloat() );
	SetC( aoMat, 0, 3, bs2_ssao_radius.GetFloat() );
	const float depthFar = bs2_ssao_depthfar.GetFloat() > 0 ? bs2_ssao_depthfar.GetFloat() : view.zFar;
	SetC( aoMat, 1, 0, bs2_ssao_legacy_depth.GetBool() ? -depthFar : depthFar );   // v3: negative = legacy 8-bit R-only depth read
	SetC( aoMat, 1, 1, tanX );
	SetC( aoMat, 1, 2, tanY );
	SetC( aoMat, 1, 3, bs2_ssao_falloff.GetFloat() );
	SetC( aoMat, 2, 0, bs2_ssao_maxdist.GetFloat() );
	const int dbg = bs2_ssao_debug.GetInt();
	SetC( aoMat, 2, 1, dbg == 5 ? 1.0f : ( dbg == 6 ? 2.0f : ( dbg == 7 ? 3.0f : 0.0f ) ) );
	SetC( aoMat, 2, 2, 1 );            // mode 1: write AO + depth
	SetC( aoMat, 2, 3, float( scale ) / ds );  // output pixel = scale/ds depth pixels (noise pattern in output pixels)
	if ( dbg == 7 )
	{
		static float s_nextMsg = 0;
		if ( gpGlobals->realtime > s_nextMsg ) { s_nextMsg = gpGlobals->realtime + 2.0f; Msg( "bs2_ssao depthdbg: depth RT format %d (%dx%d), zFar %.1f fov %.2f view %dx%d legacy %d ds %d\n", int( depth->GetImageFormat() ), dw, dh, depthFar, view.fov, view.width, view.height, bs2_ssao_legacy_depth.GetInt(), ds ); }
	}
	{
		// Polish (16 Nov): fade AO with world fog. The composite runs after fogged opaque shading,
		// so unfaded AO would darken fogged distance (the fog colour itself) with crease shadows.
		CMatRenderContextPtr ctx( materials );
		float fs = 0, fe = 0, fz = 0;
		ctx->GetFogDistances( &fs, &fe, &fz );
		const bool bFog = ctx->GetFogMode() != MATERIAL_FOG_NONE && fe > fs;
		s_flOpaqueFogEnd = ( bFog && BS2_GetWorldFogMaxDensity() >= 0.98f ) ? fe : 0.0f;
		SetC( aoMat, 3, 0, fs );
		SetC( aoMat, 3, 1, bFog ? 1.0f / ( fe - fs ) : 0.0f );
		SetC( aoMat, 3, 2, bFog ? clamp( BS2_GetWorldFogMaxDensity(), 0.0f, 1.0f ) : 0.0f );
		SetC( aoMat, 3, 3, float( dx1 - dx0 + 1 ) / dw );   // v3: tap radius is in screen fractions; the depth rectangle covers this much of the texture
	}
	{
		CMatRenderContextPtr ctx( materials );
		ctx->PushRenderTargetAndViewport( aoRT, 0, 0, aw, ah );
		ctx->DrawScreenSpaceRectangle( aoMat, 0, 0, aw, ah, dx0, dy0, dx1, dy1, dw, dh );
		ctx->PopRenderTargetAndViewport();
	}

	// Pass 1b (optional): one-bounce indirect light gathered from the lit scene into _rt_BS2_SSGI
	int gw = 1, gh = 1;
	const bool bGI = bs2_ssgi.GetBool() && giRT && !giRT->IsError();
	IMaterial *giMat = bGI ? materials->FindMaterial( "effects/bs2_ssgi", TEXTURE_GROUP_CLIENT_EFFECTS ) : NULL;
	CMaterialReference giRef;
	if ( giMat && !giMat->IsErrorMaterial() )
	{
		giRef.Init( giMat );
		SetC( giMat, 0, 0, 1.0f / dw );
		SetC( giMat, 0, 1, 1.0f / dh );
		SetC( giMat, 0, 3, bs2_ssgi_radius.GetFloat() );
		SetC( giMat, 1, 0, depthFar );
		SetC( giMat, 1, 1, tanX );
		SetC( giMat, 1, 2, tanY );
		SetC( giMat, 2, 0, bs2_ssao_maxdist.GetFloat() );
		SetC( giMat, 2, 3, float( scale ) );
		gw = MIN( giRT->GetActualWidth(), view.width / 4 ); gh = MIN( giRT->GetActualHeight(), view.height / 4 );
		SetC( giMat, 2, 3, float( view.width ) / gw );
		CMatRenderContextPtr ctx( materials );
		ctx->PushRenderTargetAndViewport( giRT, 0, 0, gw, gh );
		ctx->DrawScreenSpaceRectangle( giMat, 0, 0, gw, gh, dx0, dy0, dx1, dy1, dw, dh );
		ctx->PopRenderTargetAndViewport();
	}

	if ( bs2_ssao_debug.GetInt() == 4 )
	{
		// print the AO target just before the composite (R = AO, G+B = packed depth = saturate(depth*scale/8192)) at 5 points, once per second
		static float s_next = 0;
		if ( gpGlobals->realtime > s_next )
		{
			s_next = gpGlobals->realtime + 1.0f;
			CMatRenderContextPtr ctx( materials );
			ctx->PushRenderTargetAndViewport( aoRT );
			const int px[5][2] = { { 1, 1 }, { 1, 3 }, { 3, 1 }, { 3, 3 }, { 2, 2 } };
			Msg( "bs2_ssao aoRT (zFar %.0f, fov %.1f, ao %dx%d):", view.zFar, view.fov, aw, ah );
			for ( int i = 0; i < 5; ++i )
			{
				unsigned char v[4] = { 0, 0, 0, 0 };
				ctx->ReadPixels( aw * px[i][0] / 4, ah * px[i][1] / 4, 1, 1, v, IMAGE_FORMAT_RGBA8888 );
				Msg( " [ao %d d %.1f]", v[0], ( v[1] / 255.0f + v[2] / 65025.0f ) * 8192.0f );
			}
			Msg( "\n" );
			ctx->PopRenderTargetAndViewport();
		}
	}

	// Pass 2: blur + composite (snapshots the frame buffer into _rt_FullFrameFB first)
	SetC( blurMat, 0, 0, 1.0f / aoRT->GetActualWidth() );
	SetC( blurMat, 0, 1, 1.0f / aoRT->GetActualHeight() );
	SetC( blurMat, 0, 2, bs2_ssao_blur.GetFloat() );
	SetC( blurMat, 0, 3, bs2_ssao_debug.GetFloat() );
	// AO texture rectangle: full-frame uv -> AO uv
	SetC( blurMat, 1, 0, float( aw ) / aoRT->GetActualWidth() * float( frame->GetActualWidth() ) / view.width );
	SetC( blurMat, 1, 1, float( ah ) / aoRT->GetActualHeight() * float( frame->GetActualHeight() ) / view.height );
	SetC( blurMat, 1, 2, float( view.x ) / frame->GetActualWidth() );
	SetC( blurMat, 1, 3, float( view.y ) / frame->GetActualHeight() );
	SetC( blurMat, 2, 0, ( aw - 0.5f ) / aoRT->GetActualWidth() );
	SetC( blurMat, 2, 1, ( ah - 0.5f ) / aoRT->GetActualHeight() );
	SetC( blurMat, 2, 2, bs2_ssgi_strength.GetFloat() );
	if ( giRT && !giRT->IsError() )
	{
		SetC( blurMat, 3, 0, float( gw ) / giRT->GetActualWidth() * float( frame->GetActualWidth() ) / view.width );
		SetC( blurMat, 3, 1, float( gh ) / giRT->GetActualHeight() * float( frame->GetActualHeight() ) / view.height );
		SetC( blurMat, 3, 2, 1.0f / giRT->GetActualWidth() );
		SetC( blurMat, 3, 3, 1.0f / giRT->GetActualHeight() );
	}
	SetC( blurMat, 2, 3, ( giMat && !giMat->IsErrorMaterial() ) ? 1.0f : 0.0f );
	{
		CMatRenderContextPtr ctx( materials );
		ctx->DrawScreenSpaceRectangle( blurMat, view.x, view.y, view.width, view.height,
			view.x, view.y, view.x + view.width - 1, view.y + view.height - 1,
			frame->GetActualWidth(), frame->GetActualHeight() );
	}
}

void BS2_DrawSSAO( const CViewSetup &view )
{
	if ( !BS2_SSAOEnabled() || view.width <= 0 || view.height <= 0 )
		return;
	const int n = SSAOAORepeat();
	for ( int i = 0; i < n; ++i )
		DrawSSAOOnce( view );
}