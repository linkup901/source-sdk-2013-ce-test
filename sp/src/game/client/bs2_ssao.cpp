// BS2 SSAO prototype (SEPPMA Phase 3, Claude for Isa, 15 Nov 2026).
// Normal-free crease AO from linear depth: pairs of opposite taps, harmonic-mean plane test (exactly 0 on planes).
#include "cbase.h"
#include "bs2_ssao.h"
#include "view_scene.h"
#include "viewrender.h"
#include "rendertexture.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier0/memdbgon.h"

static ConVar bs2_ssao( "bs2_ssao", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "BS2 screen-space ambient occlusion prototype (0 off, 1 on)", true, 0, true, 1 );
static ConVar bs2_ssao_strength( "bs2_ssao_strength", "0.6", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO darkening strength", true, 0, true, 2 );
static ConVar bs2_ssao_radius( "bs2_ssao_radius", "40", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO radius in world units", true, 2, true, 256 );
static ConVar bs2_ssao_falloff( "bs2_ssao_falloff", "12", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Crease depth (units) that gives full occlusion per pair", true, 0.5f, true, 256 );
static ConVar bs2_ssao_maxdist( "bs2_ssao_maxdist", "3000", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "AO fades out by this distance (units)", true, 64, true, 65536 );
static ConVar bs2_ssao_debug( "bs2_ssao_debug", "0", FCVAR_CLIENTDLL, "Show the AO term only", true, 0, true, 1 );

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
	IMaterial *material = materials->FindMaterial( "effects/bs2_ssao", TEXTURE_GROUP_CLIENT_EFFECTS );
	if ( !material || material->IsErrorMaterial() ) return;
	CMaterialReference materialReference; // keep it referenced while bound (fixes bad reference count 0)
	materialReference.Init( material );
	ITexture *depth = materials->FindTexture( "_rt_ResolvedFullFrameDepth", TEXTURE_GROUP_RENDER_TARGET );
	if ( !depth || depth->IsError() ) return;
	ITexture *frame = GetFullFrameFrameBufferTexture( 0 );
	if ( !frame || frame->IsError() ) return;

	const float tanX = tanf( DEG2RAD( view.fov * 0.5f ) );
	const float aspect = view.m_flAspectRatio > 0 ? view.m_flAspectRatio : float( view.width ) / view.height;
	const float tanY = tanX / aspect;

	SetC( material, 0, 0, 1.0f / depth->GetActualWidth() );
	SetC( material, 0, 1, 1.0f / depth->GetActualHeight() );
	SetC( material, 0, 2, bs2_ssao_strength.GetFloat() );
	SetC( material, 0, 3, bs2_ssao_radius.GetFloat() );
	SetC( material, 1, 0, view.zFar );
	SetC( material, 1, 1, tanX );
	SetC( material, 1, 2, tanY );
	SetC( material, 1, 3, bs2_ssao_falloff.GetFloat() );
	SetC( material, 2, 0, bs2_ssao_maxdist.GetFloat() );
	SetC( material, 2, 1, bs2_ssao_debug.GetFloat() );
	SetC( material, 2, 2, 0 );
	SetC( material, 2, 3, 0 );

	DrawScreenEffectMaterial( material, view.x, view.y, view.width, view.height );
}
