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

static ConVar bs2_ssao( "bs2_ssao", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "BS2 screen-space ambient occlusion (0 off, 1 on)", true, 0, true, 1 );
static ConVar bs2_ssao_strength( "bs2_ssao_strength", "0.8", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO darkening strength", true, 0, true, 2 );
static ConVar bs2_ssao_radius( "bs2_ssao_radius", "32", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO radius in world units", true, 2, true, 256 );
static ConVar bs2_ssao_falloff( "bs2_ssao_falloff", "6", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crease depth (units) that gives full occlusion per pair", true, 0.5f, true, 256 );
static ConVar bs2_ssao_maxdist( "bs2_ssao_maxdist", "2500", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO fades out by this distance (units)", true, 64, true, 65536 );
static ConVar bs2_ssao_half( "bs2_ssao_half", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Compute AO at half resolution", true, 0, true, 1 );
static ConVar bs2_ssao_blur( "bs2_ssao_blur", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Depth-aware 4x4 blur of the AO term", true, 0, true, 1 );
static ConVar bs2_ssao_debug( "bs2_ssao_debug", "0", FCVAR_CLIENTDLL, "1 = show the AO term, 2 = show the bounce light", true, 0, true, 2 );
static ConVar bs2_ssgi( "bs2_ssgi", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "One-bounce screen-space indirect light (needs bs2_ssao 1)", true, 0, true, 1 );
static ConVar bs2_ssgi_strength( "bs2_ssgi_strength", "0.35", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Bounce light strength", true, 0, true, 2 );
static ConVar bs2_ssgi_radius( "bs2_ssgi_radius", "64", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Bounce gather radius in world units", true, 4, true, 512 );

bool BS2_SSAOEnabled()
{
	return bs2_ssao.GetBool() && g_pMaterialSystemHardwareConfig->SupportsPixelShaders_2_b() && !engine->IsHammerRunning();
}

static void SetC( IMaterial *m, int reg, int comp, float v )
{
	char name[16];
	Q_snprintf( name, sizeof(name), "$c%d_%c", reg, "xyzw"[comp] );
	bool found = false;
	IMaterialVar *var = m->FindVar( name, &found, false );
	if ( found ) var->SetFloatValue( v );
}

void BS2_DrawSSAO( const CViewSetup &view )
{
	if ( !BS2_SSAOEnabled() || view.width <= 0 || view.height <= 0 )
		return;
	IMaterial *aoMat = materials->FindMaterial( "effects/bs2_ssao", TEXTURE_GROUP_CLIENT_EFFECTS );
	IMaterial *blurMat = materials->FindMaterial( "effects/bs2_ssao_blur", TEXTURE_GROUP_CLIENT_EFFECTS );
	if ( !aoMat || aoMat->IsErrorMaterial() || !blurMat || blurMat->IsErrorMaterial() ) return;
	CMaterialReference aoRef, blurRef; // keep them referenced while bound
	aoRef.Init( aoMat );
	blurRef.Init( blurMat );
	ITexture *depth = materials->FindTexture( "_rt_ResolvedFullFrameDepth", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *aoRT = materials->FindTexture( "_rt_BS2_SSAO", TEXTURE_GROUP_RENDER_TARGET );
	ITexture *giRT = materials->FindTexture( "_rt_BS2_SSGI", TEXTURE_GROUP_RENDER_TARGET );
	if ( !depth || depth->IsError() || !aoRT || aoRT->IsError() ) return;
	ITexture *frame = GetFullFrameFrameBufferTexture( 0 );
	if ( !frame || frame->IsError() ) return;

	const float tanX = tanf( DEG2RAD( view.fov * 0.5f ) );
	const float aspect = view.m_flAspectRatio > 0 ? view.m_flAspectRatio : float( view.width ) / view.height;
	const float tanY = tanX / aspect;
	const int dw = depth->GetActualWidth(), dh = depth->GetActualHeight();
	const int scale = bs2_ssao_half.GetBool() ? 2 : 1;
	const int aw = MAX( 1, view.width / scale ), ah = MAX( 1, view.height / scale );

	// Pass 1: AO + packed depth into the AO target (top-left aw x ah)
	SetC( aoMat, 0, 0, 1.0f / dw );
	SetC( aoMat, 0, 1, 1.0f / dh );
	SetC( aoMat, 0, 2, bs2_ssao_strength.GetFloat() );
	SetC( aoMat, 0, 3, bs2_ssao_radius.GetFloat() );
	SetC( aoMat, 1, 0, view.zFar );
	SetC( aoMat, 1, 1, tanX );
	SetC( aoMat, 1, 2, tanY );
	SetC( aoMat, 1, 3, bs2_ssao_falloff.GetFloat() );
	SetC( aoMat, 2, 0, bs2_ssao_maxdist.GetFloat() );
	SetC( aoMat, 2, 1, 0 );
	SetC( aoMat, 2, 2, 1 );            // mode 1: write AO + depth
	SetC( aoMat, 2, 3, float( scale ) );  // output pixel = scale depth pixels (noise pattern in output pixels)
	{
		CMatRenderContextPtr ctx( materials );
		ctx->PushRenderTargetAndViewport( aoRT, 0, 0, aw, ah );
		ctx->DrawScreenSpaceRectangle( aoMat, 0, 0, aw, ah,
			view.x, view.y, view.x + view.width - 1, view.y + view.height - 1, dw, dh );
		ctx->PopRenderTargetAndViewport();
	}

	// Pass 1b (optional): one-bounce indirect light gathered from the lit scene into _rt_BS2_SSGI
	const bool bGI = bs2_ssgi.GetBool() && giRT && !giRT->IsError();
	IMaterial *giMat = bGI ? materials->FindMaterial( "effects/bs2_ssgi", TEXTURE_GROUP_CLIENT_EFFECTS ) : NULL;
	CMaterialReference giRef;
	if ( giMat && !giMat->IsErrorMaterial() )
	{
		giRef.Init( giMat );
		UpdateScreenEffectTexture( 0, view.x, view.y, view.width, view.height ); // scene -> _rt_FullFrameFB
		SetC( giMat, 0, 0, 1.0f / dw );
		SetC( giMat, 0, 1, 1.0f / dh );
		SetC( giMat, 0, 3, bs2_ssgi_radius.GetFloat() );
		SetC( giMat, 1, 0, view.zFar );
		SetC( giMat, 1, 1, tanX );
		SetC( giMat, 1, 2, tanY );
		SetC( giMat, 2, 0, bs2_ssao_maxdist.GetFloat() );
		SetC( giMat, 2, 3, float( scale ) );
		CMatRenderContextPtr ctx( materials );
		ctx->PushRenderTargetAndViewport( giRT, 0, 0, aw, ah );
		ctx->DrawScreenSpaceRectangle( giMat, 0, 0, aw, ah,
			view.x, view.y, view.x + view.width - 1, view.y + view.height - 1, dw, dh );
		ctx->PopRenderTargetAndViewport();
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
	SetC( blurMat, 2, 3, ( giMat && !giMat->IsErrorMaterial() ) ? 1.0f : 0.0f );
	DrawScreenEffectMaterial( blurMat, view.x, view.y, view.width, view.height );
}
