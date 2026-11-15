//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// BS2 (Claude, 15 Nov 2026): projected-texture governor (FPS idea #16).
//   Every shadowed env_projectedtexture costs a shadow-depth render plus an extra lighting pass over everything
//   it touches. Once per frame the governor scores the active shadowed lights (brightness, distance to the view,
//   whether the light or its cone is on screen) and keeps only the best r_bs2_ptex_max_shadowed of them.
//   The others fade their brightness to zero over r_bs2_ptex_fadetime and are then switched off (handle destroyed);
//   a light that wins its slot back fades in again. A held light keeps its slot unless another one beats it by
//   r_bs2_ptex_hysteresis, so lights do not flicker between slots. r_bs2_ptex_governor 0 = stock behaviour.
//=============================================================================

#include "cbase.h"
#include "shareddefs.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterial.h"
#include "view.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "texture_group_names.h"
#include "tier0/icommandline.h"
#include "igamesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar mat_slopescaledepthbias_shadowmap( "mat_slopescaledepthbias_shadowmap", "16", FCVAR_CHEAT );
static ConVar mat_depthbias_shadowmap(	"mat_depthbias_shadowmap", "0.0005", FCVAR_CHEAT  );

static ConVar r_bs2_ptex_governor( "r_bs2_ptex_governor", "0", FCVAR_ARCHIVE, "BS2 (default off, measured below noise on shipped maps): limit how many shadowed env_projectedtextures render at once (fade, no popping). 0 = stock" );
static ConVar r_bs2_ptex_max_shadowed( "r_bs2_ptex_max_shadowed", "2", FCVAR_ARCHIVE, "BS2: shadowed projected textures kept by the governor" );
static ConVar r_bs2_ptex_fadetime( "r_bs2_ptex_fadetime", "0.6", FCVAR_ARCHIVE, "BS2: seconds a governed projected texture takes to fade in/out" );
static ConVar r_bs2_ptex_hysteresis( "r_bs2_ptex_hysteresis", "1.3", FCVAR_ARCHIVE, "BS2: a held light keeps its slot unless another scores this many times higher" );
static ConVar r_bs2_ptex_debug( "r_bs2_ptex_debug", "0", 0, "BS2: print governor decisions" );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_EnvProjectedTexture : public C_BaseEntity
{
	DECLARE_CLASS( C_EnvProjectedTexture, C_BaseEntity );
public:
	DECLARE_CLIENTCLASS();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	void	ShutDownLightHandle( void );

	virtual void Simulate();

	void	UpdateLight( bool bForceUpdate );

	C_EnvProjectedTexture();
	~C_EnvProjectedTexture();

	static void GovernorThink();

private:
	float	GovernorScore() const;
	bool	IsGoverned() const { return r_bs2_ptex_governor.GetBool() && m_bState && m_bEnableShadows; }

	ClientShadowHandle_t m_LightHandle;

	EHANDLE	m_hTargetEntity;

	bool	m_bState;
	float	m_flLightFOV;
	bool	m_bEnableShadows;
	bool	m_bLightOnlyTarget;
	bool	m_bLightWorld;
	bool	m_bCameraSpace;
	Vector	m_LinearFloatLightColor;
	float	m_flAmbient;
	float	m_flNearZ;
	float	m_flFarZ;
	char	m_SpotlightTextureName[ MAX_PATH ];
	int		m_nSpotlightTextureFrame;
	int		m_nShadowQuality;

	// governor
	bool	m_bGovKeep;		// holds a shadow slot this frame
	float	m_flGovFade;	// 0..1 brightness scale
	float	m_flGovScore;

	static CUtlVector< C_EnvProjectedTexture * > s_All;
	static int s_nGovFrame;
};

CUtlVector< C_EnvProjectedTexture * > C_EnvProjectedTexture::s_All;
int C_EnvProjectedTexture::s_nGovFrame = -1;

IMPLEMENT_CLIENTCLASS_DT( C_EnvProjectedTexture, DT_EnvProjectedTexture, CEnvProjectedTexture )
	RecvPropEHandle( RECVINFO( m_hTargetEntity )	),
	RecvPropBool(	 RECVINFO( m_bState )			),
	RecvPropFloat(	 RECVINFO( m_flLightFOV )		),
	RecvPropBool(	 RECVINFO( m_bEnableShadows )	),
	RecvPropBool(	 RECVINFO( m_bLightOnlyTarget ) ),
	RecvPropBool(	 RECVINFO( m_bLightWorld )		),
	RecvPropBool(	 RECVINFO( m_bCameraSpace )		),
	RecvPropVector(	 RECVINFO( m_LinearFloatLightColor )		),
	RecvPropFloat(	 RECVINFO( m_flAmbient )		),
	RecvPropString(  RECVINFO( m_SpotlightTextureName ) ),
	RecvPropInt(	 RECVINFO( m_nSpotlightTextureFrame ) ),
	RecvPropFloat(	 RECVINFO( m_flNearZ )	),
	RecvPropFloat(	 RECVINFO( m_flFarZ )	),
	RecvPropInt(	 RECVINFO( m_nShadowQuality )	),
END_RECV_TABLE()

C_EnvProjectedTexture::C_EnvProjectedTexture( void )
{
	m_LightHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_bGovKeep = true;
	m_flGovFade = 1.0f;
	m_flGovScore = 0.0f;
	s_All.AddToTail( this );
}

C_EnvProjectedTexture::~C_EnvProjectedTexture( void )
{
	s_All.FindAndRemove( this );
	ShutDownLightHandle();
}

void C_EnvProjectedTexture::ShutDownLightHandle( void )
{
	// Clear out the light
	if( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( m_LightHandle );
		m_LightHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
}

//-----------------------------------------------------------------------------
// Governor: how much this light matters to the current view
//-----------------------------------------------------------------------------
float C_EnvProjectedTexture::GovernorScore() const
{
	const Vector &eye = MainViewOrigin();
	const Vector &fwd = MainViewForward();
	Vector pos = GetAbsOrigin();
	Vector lfwd;
	AngleVectors( GetAbsAngles(), &lfwd );

	float lum = 0.3f * m_LinearFloatLightColor.x + 0.59f * m_LinearFloatLightColor.y + 0.11f * m_LinearFloatLightColor.z;
	lum = MAX( lum, 0.01f );

	// where the light lands: a point down its cone (clamped to its range)
	float reach = clamp( m_flFarZ * 0.35f, 64.0f, 512.0f );
	Vector lit = pos + lfwd * reach;

	float vis = 0.15f;
	Vector toLit = lit - eye; float dLit = VectorNormalize( toLit );
	Vector toPos = pos - eye; float dPos = VectorNormalize( toPos );
	if ( DotProduct( toLit, fwd ) > 0.35f || DotProduct( toPos, fwd ) > 0.35f )
		vis = 1.0f;
	// standing inside the cone: always matters
	Vector fromLight = eye - pos; float dEye = VectorNormalize( fromLight );
	if ( dEye < m_flFarZ && DotProduct( fromLight, lfwd ) > cosf( DEG2RAD( m_flLightFOV * 0.5f ) ) )
		vis = 1.5f;

	float d = MIN( dLit, dPos );
	if ( d > m_flFarZ + 1024.0f )
		vis *= 0.25f;
	return lum * vis * ( m_flLightFOV / 90.0f + 0.5f ) / ( 1.0f + ( d / 512.0f ) * ( d / 512.0f ) );
}

void C_EnvProjectedTexture::GovernorThink()
{
	if ( s_nGovFrame == gpGlobals->framecount )
		return;
	s_nGovFrame = gpGlobals->framecount;

	CUtlVector< C_EnvProjectedTexture * > live;
	for ( int i = 0; i < s_All.Count(); i++ )
	{
		C_EnvProjectedTexture *p = s_All[i];
		if ( !p->IsGoverned() || p->IsDormant() )
		{
			p->m_bGovKeep = true;	// not governed: always on
			continue;
		}
		float s = p->GovernorScore();
		if ( p->m_bGovKeep )
			s *= MAX( 1.0f, r_bs2_ptex_hysteresis.GetFloat() );
		p->m_flGovScore = s;
		live.AddToTail( p );
	}

	int nMax = MAX( 0, r_bs2_ptex_max_shadowed.GetInt() );
	// partial selection sort: highest scores first
	for ( int i = 0; i < live.Count(); i++ )
	{
		int best = i;
		for ( int j = i + 1; j < live.Count(); j++ )
			if ( live[j]->m_flGovScore > live[best]->m_flGovScore ) best = j;
		if ( best != i ) { C_EnvProjectedTexture *t = live[i]; live[i] = live[best]; live[best] = t; }
		bool keep = i < nMax;
		if ( r_bs2_ptex_debug.GetBool() && keep != live[i]->m_bGovKeep )
			Msg( "ptex governor: %s light %d (score %.3f)\n", keep ? "KEEP" : "FADE", live[i]->entindex(), live[i]->m_flGovScore );
		live[i]->m_bGovKeep = keep;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_EnvProjectedTexture::OnDataChanged( DataUpdateType_t updateType )
{
	UpdateLight( true );
	BaseClass::OnDataChanged( updateType );
}

void C_EnvProjectedTexture::UpdateLight( bool bForceUpdate )
{
	if ( m_bState == false || ( IsGoverned() && !m_bGovKeep && m_flGovFade <= 0.0f ) )
	{
		if ( m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			ShutDownLightHandle();
		}

		return;
	}

	Vector vForward, vRight, vUp, vPos = GetAbsOrigin();
	FlashlightState_t state;

	if ( m_hTargetEntity != NULL )
	{
		if ( m_bCameraSpace )
		{
			const QAngle &angles = GetLocalAngles();

			C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
			if( pPlayer )
			{
				const QAngle playerAngles = pPlayer->GetAbsAngles();
				
				Vector vPlayerForward, vPlayerRight, vPlayerUp;
				AngleVectors( playerAngles, &vPlayerForward, &vPlayerRight, &vPlayerUp );

            	matrix3x4_t	mRotMatrix;
				AngleMatrix( angles, mRotMatrix );

				VectorITransform( vPlayerForward, mRotMatrix, vForward );
				VectorITransform( vPlayerRight, mRotMatrix, vRight );
				VectorITransform( vPlayerUp, mRotMatrix, vUp );

				float dist = (m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin()).Length();
				vPos = m_hTargetEntity->GetAbsOrigin() - vForward*dist;

				VectorNormalize( vForward );
				VectorNormalize( vRight );
				VectorNormalize( vUp );
			}
		}
		else
		{
			vForward = m_hTargetEntity->GetAbsOrigin() - GetAbsOrigin();
			VectorNormalize( vForward );

			// JasonM - unimplemented
			Assert (0);

			//Quaternion q = DirectionToOrientation( dir );


			//
			// JasonM - set up vRight, vUp
			//

//			VectorNormalize( vRight );
//			VectorNormalize( vUp );
		}
	}
	else
	{
		AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
	}

	state.m_fHorizontalFOVDegrees = m_flLightFOV;
	state.m_fVerticalFOVDegrees = m_flLightFOV;

	state.m_vecLightOrigin = vPos;
	BasisToQuaternion( vForward, vRight, vUp, state.m_quatOrientation );

	float flFade = IsGoverned() ? clamp( m_flGovFade, 0.0f, 1.0f ) : 1.0f;
	state.m_fQuadraticAtten = 0.0;
	state.m_fLinearAtten = 100;
	state.m_fConstantAtten = 0.0f;
	state.m_Color[0] = m_LinearFloatLightColor.x * flFade;
	state.m_Color[1] = m_LinearFloatLightColor.y * flFade;
	state.m_Color[2] = m_LinearFloatLightColor.z * flFade;
	state.m_Color[3] = 0.0f; // fixme: need to make ambient work m_flAmbient;
	state.m_NearZ = m_flNearZ;
	state.m_FarZ = m_flFarZ;
	state.m_flShadowSlopeScaleDepthBias = mat_slopescaledepthbias_shadowmap.GetFloat();
	state.m_flShadowDepthBias = mat_depthbias_shadowmap.GetFloat();
	state.m_bEnableShadows = m_bEnableShadows;
	state.m_pSpotlightTexture = materials->FindTexture( m_SpotlightTextureName, TEXTURE_GROUP_OTHER, false );
	state.m_nSpotlightTextureFrame = m_nSpotlightTextureFrame;

	state.m_nShadowQuality = m_nShadowQuality; // Allow entity to affect shadow quality

	if( m_LightHandle == CLIENTSHADOW_INVALID_HANDLE )
	{
		m_LightHandle = g_pClientShadowMgr->CreateFlashlight( state );
	}
	else
	{
		if ( m_hTargetEntity != NULL || bForceUpdate == true )
		{
			g_pClientShadowMgr->UpdateFlashlightState( m_LightHandle, state );
		}
	}

	if( m_bLightOnlyTarget )
	{
		g_pClientShadowMgr->SetFlashlightTarget( m_LightHandle, m_hTargetEntity );
	}
	else
	{
		g_pClientShadowMgr->SetFlashlightTarget( m_LightHandle, NULL );
	}

	g_pClientShadowMgr->SetFlashlightLightWorld( m_LightHandle, m_bLightWorld );

	if ( bForceUpdate == false )
	{
		g_pClientShadowMgr->UpdateProjectedTexture( m_LightHandle, true );
	}
}

//-----------------------------------------------------------------------------
// BS2 (Claude, 15 Nov 2026): bs2_fpsmark <label> <seconds> - measures REAL frames (unlike timerefresh, which renders
// 128 views inside one frame and runs out of render data on prop-dense maps). Prints in timerefresh's format so the
// fps-heatmap tools parse it unchanged:  BS2FPS <label>: <avg frame s> seconds (<fps> fps) frames <n> p95 <ms> ms
//-----------------------------------------------------------------------------
class CBS2FpsMark : public CAutoGameSystemPerFrame
{
public:
	CBS2FpsMark() : CAutoGameSystemPerFrame( "CBS2FpsMark" ), m_bActive( false ) {}
	void Start( const char *label, float seconds )
	{
		Q_strncpy( m_szLabel, label, sizeof( m_szLabel ) );
		m_flEnd = Plat_FloatTime() + seconds; m_flLast = -1; m_Samples.RemoveAll(); m_bActive = true;
	}
	virtual void Update( float frametime )
	{
		if ( !m_bActive ) return;
		double now = Plat_FloatTime();
		if ( m_flLast > 0 ) m_Samples.AddToTail( (float)( now - m_flLast ) );
		m_flLast = now;
		if ( now < m_flEnd ) return;
		m_bActive = false;
		int n = m_Samples.Count(); if ( n < 2 ) { Msg( "BS2FPS %s: no frames\n", m_szLabel ); return; }
		double sum = 0; for ( int i = 0; i < n; i++ ) sum += m_Samples[i];
		m_Samples.Sort( []( const float *a, const float *b ) { return ( *a < *b ) ? -1 : ( *a > *b ? 1 : 0 ); } );
		double avg = sum / n;
		Msg( "BS2FPS %s: %f seconds (%f fps) frames %d p95 %.2f ms\n", m_szLabel, avg, 1.0 / avg, n, 1000.0 * m_Samples[ MIN( n - 1, (int)( n * 0.95f ) ) ] );
	}
private:
	bool m_bActive; double m_flEnd, m_flLast; char m_szLabel[64]; CUtlVector< float > m_Samples;
};
static CBS2FpsMark s_BS2FpsMark;
CON_COMMAND( bs2_fpsmark, "BS2: bs2_fpsmark <label> <seconds> - average real frame time over the next N seconds" )
{
	s_BS2FpsMark.Start( args.ArgC() > 1 ? args[1] : "mark", args.ArgC() > 2 ? atof( args[2] ) : 3.0f );
}

void C_EnvProjectedTexture::Simulate( void )
{
	bool bForce = false;
	if ( IsGoverned() )
	{
		GovernorThink();
		float flTarget = m_bGovKeep ? 1.0f : 0.0f;
		if ( m_flGovFade != flTarget )
		{
			float flStep = gpGlobals->frametime / MAX( 0.01f, r_bs2_ptex_fadetime.GetFloat() );
			m_flGovFade = ( flTarget > m_flGovFade ) ? MIN( flTarget, m_flGovFade + flStep ) : MAX( flTarget, m_flGovFade - flStep );
			bForce = true;	// push the new brightness (or create / destroy the handle)
		}
	}
	else if ( m_flGovFade != 1.0f )
	{
		m_flGovFade = 1.0f;
		bForce = true;
	}

	UpdateLight( bForce );
	if ( bForce && m_LightHandle != CLIENTSHADOW_INVALID_HANDLE )
		g_pClientShadowMgr->UpdateProjectedTexture( m_LightHandle, true );

	BaseClass::Simulate();
}
