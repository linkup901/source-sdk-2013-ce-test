//========= Black Stasis 2, Phase 2 A5: the bs2_veins HUD element =========//
//
// Purpose: thin, nerve-like veins crawling in from the screen edges.
//   - damage: every drop of the local player's health pushes the vein level up (scaled by the damage), it decays in about 5 seconds. Together with a weaker
//     red flash (hud_damageindicator.cpp scales its alphas by bs2_damage_flash_scale)
//   - low health keeps a faint level
//   - env_bs2_veins (BS2Veins user message): a pulse for scripted collapses, or a steady level that ramps
//   - death: two eyelids close over bs2_death_lid_time seconds (3), the veins pulse hard until the game reloads
//
// The veins are pre-drawn art (tools\gen_bs2_veins.py): one branching black network grown inward from the screen border (Murray-law radii, hairline
// capillaries, wet sheen), saved as BS2_VEIN_STAGES snapshots of its growth (materials/vgui/bs2/veins/vein_00..07). At vein level g the two snapshots around g
// are cross-faded, so raising g makes the veins crawl from the edge to the centre. The canvas is 2:1 and covers the screen (crops the sides on 16:9, the top and
// bottom on 21:9, never stretched); each new episode of veins mirrors it at random so it does not always look the same.
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include "bs2/bs2_ui_messages.h"
#include "bs2_hud.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar bs2_veins( "bs2_veins", "1", FCVAR_ARCHIVE, "Black Stasis 2: vein overlay on damage, pulses and death (0 = off)" );
static ConVar bs2_death_lid_time( "bs2_death_lid_time", "3.0", FCVAR_ARCHIVE, "Seconds the eyelids take to close when the player dies" );
static ConVar bs2_veins_debug( "bs2_veins_debug", "0", FCVAR_CHEAT, "Hold the vein level at this value (0 = normal)" );

#define BS2_VEIN_STAGES		8			// growth snapshots, vein_00 (first 1/8 of the growth) .. vein_07 (all of it)
#define BS2_VEIN_ASPECT		2.0f		// width / height of the vein canvas (2048 x 1024)

//-----------------------------------------------------------------------------
class CHudBS2Veins : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBS2Veins, vgui::Panel );

public:
	CHudBS2Veins( const char *pElementName );

	virtual void	Init( void );
	virtual void	LevelInit( void );
	virtual void	LevelShutdown( void );
	virtual bool	ShouldDraw( void );

	void			MsgFunc_BS2Veins( bf_read &msg );
	void			Pulse( float flStrength, float flDuration );

protected:
	virtual void	Paint( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	void			Reset_( void );
	void			Update( void );
	void			LoadStages( void );
	void			DrawStage( int nStage, int nAlpha, int nScreenW, int nScreenH );
	void			DrawVeins( float flLevel, float flThrob, int nScreenW, int nScreenH );
	void			DrawLids( float flProgress, int nScreenW, int nScreenH );

	int				m_nStage[BS2_VEIN_STAGES];		// vgui texture ids (0 = not loaded yet)
	int				m_nFlip;						// bit 0 mirrors x, bit 1 mirrors y; new at the start of every episode
	bool			m_bWasActive;

	// sources of the level
	float			m_flDamageLevel;
	float			m_flSteadyLevel;
	float			m_flSteadyFrom;
	float			m_flSteadyTarget;
	float			m_flSteadyStart;
	float			m_flSteadyTime;
	float			m_flPulseStrength;
	float			m_flPulseStart;
	float			m_flPulseDuration;

	// the player
	int				m_nLastHealth;
	bool			m_bDead;
	float			m_flDeathStart;

	// this frame
	float			m_flLastUpdate;
	float			m_flLevel;		// 0..1, what the veins show
	float			m_flThrob;		// 0..1
	float			m_flLids;		// 0..1 how far the eyelids are closed
};

DECLARE_HUDELEMENT( CHudBS2Veins );
DECLARE_HUD_MESSAGE( CHudBS2Veins, BS2Veins );

static CHudBS2Veins *s_pVeins = NULL;

//-----------------------------------------------------------------------------
CHudBS2Veins::CHudBS2Veins( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudBS2Veins" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	SetZPos( 100 );		// above the other HUD panels: the veins creep over the title cards (prelude), the eyelids close over everything

	for ( int i = 0; i < BS2_VEIN_STAGES; i++ )
		m_nStage[i] = 0;
	m_nFlip = 0;
	m_bWasActive = false;
	Reset_();

	s_pVeins = this;
}

void CHudBS2Veins::Reset_( void )
{
	m_flDamageLevel = 0.0f;
	m_flSteadyLevel = 0.0f;
	m_flSteadyFrom = 0.0f;
	m_flSteadyTarget = 0.0f;
	m_flSteadyStart = 0.0f;
	m_flSteadyTime = 0.0f;
	m_flPulseStrength = 0.0f;
	m_flPulseStart = 0.0f;
	m_flPulseDuration = 0.0f;
	m_nLastHealth = -1;
	m_bDead = false;
	m_flDeathStart = 0.0f;
	m_flLastUpdate = -1.0f;
	m_flLevel = 0.0f;
	m_flThrob = 0.0f;
	m_flLids = 0.0f;
}

void CHudBS2Veins::Init( void )
{
	HOOK_HUD_MESSAGE( CHudBS2Veins, BS2Veins );
	Reset_();
}

void CHudBS2Veins::LevelInit( void )
{
	Reset_();
	m_bWasActive = false;
}

void CHudBS2Veins::LevelShutdown( void )
{
	Reset_();
}

void CHudBS2Veins::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetVisible( false );
	SetBgColor( Color( 0, 0, 0, 0 ) );
}

//-----------------------------------------------------------------------------
// Purpose: a pulse for code (and for the BS2Veins message)
//-----------------------------------------------------------------------------
void CHudBS2Veins::Pulse( float flStrength, float flDuration )
{
	m_flPulseStrength = clamp( flStrength, 0.0f, 1.0f );
	m_flPulseStart = gpGlobals->curtime;
	m_flPulseDuration = MAX( flDuration, 0.2f );
}

void BS2_Veins_Pulse( float flStrength, float flDuration )
{
	if ( s_pVeins )
		s_pVeins->Pulse( flStrength, flDuration );
}

// console test hook: bs2_veinpulse [strength 0..1] [seconds]
CON_COMMAND( bs2_veinpulse, "bs2_veinpulse [strength 0..1] [seconds]: one vein pulse (test)" )
{
	const float flStrength = ( args.ArgC() > 1 ) ? (float)atof( args[1] ) : 0.9f;
	const float flDuration = ( args.ArgC() > 2 ) ? (float)atof( args[2] ) : 3.0f;
	BS2_Veins_Pulse( flStrength, flDuration );
}

void CHudBS2Veins::MsgFunc_BS2Veins( bf_read &msg )
{
	int nCmd = msg.ReadByte();

	if ( nCmd == BS2VEINS_PULSE )
	{
		float flStrength = msg.ReadFloat();
		float flDuration = msg.ReadFloat();
		Pulse( flStrength, flDuration );
	}
	else if ( nCmd == BS2VEINS_LEVEL )
	{
		float flLevel = msg.ReadFloat();
		float flTime = msg.ReadFloat();
		m_flSteadyFrom = m_flSteadyLevel;
		m_flSteadyTarget = clamp( flLevel, 0.0f, 1.0f );
		m_flSteadyStart = gpGlobals->curtime;
		m_flSteadyTime = MAX( flTime, 0.0f );
	}
	else
	{
		m_flDamageLevel = 0.0f;
		m_flSteadyLevel = m_flSteadyFrom = m_flSteadyTarget = 0.0f;
		m_flPulseStrength = 0.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: the levels for this frame: from health, the entity and the death
//-----------------------------------------------------------------------------
void CHudBS2Veins::Update( void )
{
	const float flNow = gpGlobals->curtime;
	if ( flNow == m_flLastUpdate )
		return;

	float flDt = ( m_flLastUpdate < 0.0f ) ? 0.0f : clamp( flNow - m_flLastUpdate, 0.0f, 0.25f );
	m_flLastUpdate = flNow;

	// the player: health drops, death
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	float flLowHealth = 0.0f;

	if ( pPlayer )
	{
		const int nHealth = pPlayer->GetHealth();

		if ( m_nLastHealth >= 0 && nHealth < m_nLastHealth && nHealth > 0 )
		{
			const float flDrop = (float)( m_nLastHealth - nHealth );
			m_flDamageLevel = MIN( 1.0f, m_flDamageLevel + 0.18f + flDrop * 0.015f );
		}
		m_nLastHealth = nHealth;

		if ( nHealth > 0 && nHealth < 30 )
		{
			flLowHealth = ( 30 - nHealth ) / 30.0f * 0.38f;
		}

		const bool bDead = ( nHealth <= 0 ) || !pPlayer->IsAlive();
		if ( bDead && !m_bDead )
		{
			m_flDeathStart = flNow;
		}
		m_bDead = bDead;
	}

	m_flDamageLevel = MAX( 0.0f, m_flDamageLevel - flDt * 0.12f );

	// the steady level of the entity
	if ( m_flSteadyTime > 0.0f )
	{
		const float f = clamp( ( flNow - m_flSteadyStart ) / m_flSteadyTime, 0.0f, 1.0f );
		m_flSteadyLevel = m_flSteadyFrom + ( m_flSteadyTarget - m_flSteadyFrom ) * f;
	}
	else
	{
		m_flSteadyLevel = m_flSteadyTarget;
	}

	// a pulse: up quickly, down slowly
	float flPulse = 0.0f;
	if ( m_flPulseStrength > 0.0f && m_flPulseDuration > 0.0f )
	{
		const float t = ( flNow - m_flPulseStart ) / m_flPulseDuration;
		if ( t >= 0.0f && t < 1.0f )
		{
			const float flUp = 0.2f;
			float e = ( t < flUp ) ? ( t / flUp ) : ( 1.0f - ( t - flUp ) / ( 1.0f - flUp ) );
			e = e * e * ( 3.0f - 2.0f * e );
			flPulse = m_flPulseStrength * e;
		}
		else if ( t >= 1.0f )
		{
			m_flPulseStrength = 0.0f;
		}
	}

	float flLevel = MAX( MAX( m_flDamageLevel, m_flSteadyLevel ), MAX( flPulse, flLowHealth ) );

	// the heart: faster with the level
	float flThrob = 0.5f + 0.5f * sinf( flNow * ( 5.0f + 5.0f * flLevel ) );

	// death
	m_flLids = 0.0f;
	if ( m_bDead )
	{
		const float flLidTime = MAX( bs2_death_lid_time.GetFloat(), 0.2f );
		const float p = clamp( ( flNow - m_flDeathStart ) / flLidTime, 0.0f, 1.0f );
		m_flLids = p;
		flLevel = MAX( flLevel, 0.55f + 0.45f * p );
		flThrob = 0.5f + 0.5f * sinf( flNow * 8.0f );
	}

	if ( bs2_veins_debug.GetFloat() > 0.0f )
	{
		flLevel = clamp( bs2_veins_debug.GetFloat(), 0.0f, 1.0f );
	}

	m_flLevel = flLevel;
	m_flThrob = flThrob;
}

bool CHudBS2Veins::ShouldDraw( void )
{
	if ( !bs2_veins.GetBool() )
		return false;

	Update();

	return ( m_flLevel > 0.002f || m_flLids > 0.0f );
}

//-----------------------------------------------------------------------------
// Purpose: the vein art (see the top of the file)
//-----------------------------------------------------------------------------
void CHudBS2Veins::LoadStages( void )
{
	for ( int i = 0; i < BS2_VEIN_STAGES; i++ )
	{
		if ( m_nStage[i] != 0 )
			continue;

		char szName[64];
		Q_snprintf( szName, sizeof( szName ), "vgui/bs2/veins/vein_%02d", i );
		m_nStage[i] = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_nStage[i], szName, true, false );
	}
}

// one snapshot over the whole screen, "cover" fit of the 2:1 canvas
void CHudBS2Veins::DrawStage( int nStage, int nAlpha, int nScreenW, int nScreenH )
{
	if ( nAlpha <= 0 || nStage < 0 || nStage >= BS2_VEIN_STAGES || m_nStage[nStage] == 0 )
		return;

	const float flScreenAspect = (float)nScreenW / (float)MAX( nScreenH, 1 );
	float s0 = 0.0f, s1 = 1.0f, t0 = 0.0f, t1 = 1.0f;

	if ( flScreenAspect >= BS2_VEIN_ASPECT )
	{
		// wider than the canvas: crop the top and bottom
		const float flVisible = BS2_VEIN_ASPECT / flScreenAspect;
		t0 = 0.5f - 0.5f * flVisible;
		t1 = 0.5f + 0.5f * flVisible;
	}
	else
	{
		// narrower: crop the sides
		const float flVisible = flScreenAspect / BS2_VEIN_ASPECT;
		s0 = 0.5f - 0.5f * flVisible;
		s1 = 0.5f + 0.5f * flVisible;
	}

	if ( m_nFlip & 1 )
		{ const float flTmp = s0; s0 = s1; s1 = flTmp; }
	if ( m_nFlip & 2 )
		{ const float flTmp = t0; t0 = t1; t1 = flTmp; }

	vgui::Vertex_t verts[4];
	verts[0].Init( Vector2D( 0, 0 ),								Vector2D( s0, t0 ) );
	verts[1].Init( Vector2D( (float)nScreenW, 0 ),					Vector2D( s1, t0 ) );
	verts[2].Init( Vector2D( (float)nScreenW, (float)nScreenH ),	Vector2D( s1, t1 ) );
	verts[3].Init( Vector2D( 0, (float)nScreenH ),					Vector2D( s0, t1 ) );

	surface()->DrawSetColor( 255, 255, 255, nAlpha );
	surface()->DrawSetTexture( m_nStage[nStage] );
	surface()->DrawTexturedPolygon( 4, verts );
}

void CHudBS2Veins::DrawVeins( float flLevel, float flThrob, int nScreenW, int nScreenH )
{
	LoadStages();

	// the heart: a slight alpha pulse
	const float flOverall = clamp( ( 0.62f + flLevel * 0.5f ) * ( 0.92f + 0.08f * flThrob ), 0.0f, 1.0f );

	// stage k holds the growth up to (k + 1) / N: below the first stage the first one fades in, above it the stage below is solid and the next one fades in
	const float flPos = clamp( flLevel, 0.0f, 1.0f ) * BS2_VEIN_STAGES;
	const int nBase = MIN( (int)flPos, BS2_VEIN_STAGES );
	const float flFrac = flPos - (float)nBase;

	if ( nBase == 0 )
	{
		DrawStage( 0, (int)( 255.0f * flOverall * flFrac ), nScreenW, nScreenH );
		return;
	}

	DrawStage( nBase - 1, (int)( 255.0f * flOverall ), nScreenW, nScreenH );
	if ( nBase < BS2_VEIN_STAGES )
	{
		DrawStage( nBase, (int)( 255.0f * flOverall * flFrac ), nScreenW, nScreenH );
	}
}

//-----------------------------------------------------------------------------
// Purpose: two eyelids closing from the top and the bottom with a soft edge; p = 0..1
//-----------------------------------------------------------------------------
void CHudBS2Veins::DrawLids( float flProgress, int nScreenW, int nScreenH )
{
	if ( flProgress <= 0.0f )
		return;

	// slow start, a small flutter, then shut
	float e = flProgress * flProgress * ( 3.0f - 2.0f * flProgress );
	e = clamp( e + 0.035f * sinf( flProgress * 20.0f ) * ( 1.0f - flProgress ), 0.0f, 1.0f );

	const float flSoft = nScreenH * 0.14f;
	const float flCovered = e * ( nScreenH * 0.5f + flSoft );
	const int nSolid = (int)MAX( 0.0f, flCovered - flSoft );

	// top
	surface()->DrawSetColor( 0, 0, 0, 255 );
	if ( nSolid > 0 )
	{
		surface()->DrawFilledRect( 0, 0, nScreenW, nSolid );
	}
	{
		const int nFadeTop = nSolid;
		const int nFadeBottom = (int)flCovered;
		const unsigned int nAlpha0 = (unsigned int)( 255.0f * clamp( flCovered / flSoft, 0.0f, 1.0f ) );
		if ( nFadeBottom > nFadeTop )
		{
			surface()->DrawFilledRectFade( 0, nFadeTop, nScreenW, nFadeBottom, nAlpha0, 0, false );
		}
	}

	// bottom
	if ( nSolid > 0 )
	{
		surface()->DrawFilledRect( 0, nScreenH - nSolid, nScreenW, nScreenH );
	}
	{
		const int nFadeBottom = nScreenH - nSolid;
		const int nFadeTop = nScreenH - (int)flCovered;
		const unsigned int nAlpha1 = (unsigned int)( 255.0f * clamp( flCovered / flSoft, 0.0f, 1.0f ) );
		if ( nFadeBottom > nFadeTop )
		{
			surface()->DrawFilledRectFade( 0, nFadeTop, nScreenW, nFadeBottom, 0, nAlpha1, false );
		}
	}
}

void CHudBS2Veins::Paint( void )
{
	int nScreenW, nScreenH;
	GetHudSize( nScreenW, nScreenH );
	SetSize( nScreenW, nScreenH );
	SetPos( 0, 0 );

	Update();

	// a dark vignette under the veins
	if ( m_flLevel > 0.002f )
	{
		const int nBand = (int)( nScreenH * 0.20f );
		const unsigned int nAlpha = (unsigned int)( 150.0f * clamp( m_flLevel * 1.2f, 0.0f, 1.0f ) );

		surface()->DrawSetColor( 8, 0, 3, 255 );
		surface()->DrawFilledRectFade( 0, 0, nScreenW, nBand, nAlpha, 0, false );
		surface()->DrawFilledRectFade( 0, nScreenH - nBand, nScreenW, nScreenH, 0, nAlpha, false );
		surface()->DrawFilledRectFade( 0, 0, nBand, nScreenH, nAlpha, 0, true );
		surface()->DrawFilledRectFade( nScreenW - nBand, 0, nScreenW, nScreenH, 0, nAlpha, true );

		if ( !m_bWasActive )
		{
			m_nFlip = RandomInt( 0, 3 );
		}
		m_bWasActive = true;

		DrawVeins( m_flLevel, m_flThrob, nScreenW, nScreenH );
	}
	else
	{
		m_bWasActive = false;
	}

	DrawLids( m_flLids, nScreenW, nScreenH );
}
