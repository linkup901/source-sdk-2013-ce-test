//========= Black Stasis 2, Phase 2 A5: the bs2_veins HUD element =========//
//
// Purpose: thin, nerve-like veins crawling in from the screen edges.
//   - damage: every drop of the local player's health pushes the vein level up (scaled by the damage), it decays in about 5 seconds. Together with a weaker
//     red flash (hud_damageindicator.cpp scales its alphas by bs2_damage_flash_scale)
//   - low health keeps a faint level
//   - env_bs2_veins (BS2Veins user message): a pulse for scripted collapses, or a steady level that ramps
//   - death: two eyelids close over bs2_death_lid_time seconds (3), the veins pulse hard until the game reloads
//
// The veins are drawn from a little tree grown once with a fixed seed (no texture to make): a few roots on the screen border, each walking inward with a
// drifting heading and side branches. A segment has a birth value 0..1 (how far along its path); at vein level g every segment with birth < g is there, the one
// at the front is only partly drawn, so raising g makes the veins crawl.
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

#define VEIN_SPACE_W		1.7778f		// the veins are grown in a 16:9 space measured in screen heights: x 0..1.7778, y 0..1
#define VEIN_MAX_SEGMENTS	3000

struct BS2VeinSeg_t
{
	float	x0, y0, x1, y1;		// in the grown space
	float	width;				// pixels at 1080 lines
	float	birth;				// 0..1
};

class CBS2VeinRand
{
public:
	CBS2VeinRand( unsigned nSeed ) : m_nState( nSeed ) {}
	float Next( void )
	{
		m_nState = m_nState * 1664525u + 1013904223u;
		return ( m_nState >> 8 ) * ( 1.0f / 16777216.0f );
	}
private:
	unsigned m_nState;
};

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
	void			BuildVeins( void );
	void			Grow( float x, float y, float flAngle, float flLength, float flWidth, float flBirth, int nGeneration, CBS2VeinRand &rnd );
	void			AddSegment( float x0, float y0, float x1, float y1, float flWidth, float flBirth );
	void			DrawQuad( float x0, float y0, float x1, float y1, float flHalfWidth, int r, int g, int b, int a );
	void			DrawVeins( float flLevel, float flThrob, int nScreenW, int nScreenH );
	void			DrawLids( float flProgress, int nScreenW, int nScreenH );

	CUtlVector<BS2VeinSeg_t>	m_Segments;
	bool			m_bBuilt;
	int				m_nWhiteTexture;

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

	m_bBuilt = false;
	m_nWhiteTexture = 0;
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
			m_flDamageLevel = MIN( 1.0f, m_flDamageLevel + 0.10f + flDrop * 0.012f );
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

	m_flDamageLevel = MAX( 0.0f, m_flDamageLevel - flDt * 0.22f );

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
// Purpose: the vein tree (grown once, fixed seed)
//-----------------------------------------------------------------------------
void CHudBS2Veins::AddSegment( float x0, float y0, float x1, float y1, float flWidth, float flBirth )
{
	if ( m_Segments.Count() >= VEIN_MAX_SEGMENTS )
		return;

	BS2VeinSeg_t seg;
	seg.x0 = x0;
	seg.y0 = y0;
	seg.x1 = x1;
	seg.y1 = y1;
	seg.width = flWidth;
	seg.birth = clamp( flBirth, 0.0f, 1.0f );
	m_Segments.AddToTail( seg );
}

void CHudBS2Veins::Grow( float x, float y, float flAngle, float flLength, float flWidth, float flBirth, int nGeneration, CBS2VeinRand &rnd )
{
	const float flStep = 0.016f;
	const int nSteps = (int)( flLength / flStep );

	for ( int i = 0; i < nSteps; i++ )
	{
		flAngle += ( rnd.Next() - 0.5f ) * 0.7f;

		const float nx = x + cosf( flAngle ) * flStep;
		const float ny = y + sinf( flAngle ) * flStep;
		const float f = (float)i / (float)MAX( nSteps, 1 );
		const float w = flWidth * ( 1.0f - 0.65f * f );

		flBirth += flStep * 0.95f * ( 0.9f + 0.2f * rnd.Next() );
		AddSegment( x, y, nx, ny, w, flBirth );

		// side branches, thinner, shorter, born a little after
		if ( nGeneration < 3 && rnd.Next() < ( nGeneration == 0 ? 0.14f : 0.08f ) )
		{
			const float flSide = ( rnd.Next() < 0.5f ) ? -1.0f : 1.0f;
			Grow( nx, ny, flAngle + flSide * ( 0.5f + rnd.Next() * 0.7f ), flLength * ( 0.34f + 0.2f * rnd.Next() ) - i * flStep * 0.3f,
				  w * 0.62f, flBirth + 0.015f, nGeneration + 1, rnd );
		}

		x = nx;
		y = ny;
	}
}

void CHudBS2Veins::BuildVeins( void )
{
	m_bBuilt = true;
	m_Segments.RemoveAll();

	CBS2VeinRand rnd( 20261005u );

	const int nRoots = 20;
	const float flPerimeter = 2.0f * ( VEIN_SPACE_W + 1.0f );

	for ( int i = 0; i < nRoots; i++ )
	{
		// along the border, evenly with a jitter
		float s = ( ( i + 0.5f + ( rnd.Next() - 0.5f ) * 0.8f ) / nRoots ) * flPerimeter;
		float x, y;

		if ( s < VEIN_SPACE_W )							{ x = s; y = 0.0f; }							// top
		else if ( s < VEIN_SPACE_W + 1.0f )				{ x = VEIN_SPACE_W; y = s - VEIN_SPACE_W; }		// right
		else if ( s < 2.0f * VEIN_SPACE_W + 1.0f )		{ x = VEIN_SPACE_W - ( s - VEIN_SPACE_W - 1.0f ); y = 1.0f; }	// bottom
		else											{ x = 0.0f; y = 1.0f - ( s - 2.0f * VEIN_SPACE_W - 1.0f ); }	// left

		// inward: towards the centre with a spread
		const float flToCenter = atan2f( 0.5f - y, VEIN_SPACE_W * 0.5f - x );
		const float flAngle = flToCenter + ( rnd.Next() - 0.5f ) * 0.9f;

		const float flLength = 0.28f + 0.22f * rnd.Next();
		Grow( x, y, flAngle, flLength, 4.2f + 2.4f * rnd.Next(), rnd.Next() * 0.18f, 0, rnd );
	}
}

//-----------------------------------------------------------------------------
void CHudBS2Veins::DrawQuad( float x0, float y0, float x1, float y1, float flHalfWidth, int r, int g, int b, int a )
{
	float dx = x1 - x0;
	float dy = y1 - y0;
	float len = sqrtf( dx * dx + dy * dy );
	if ( len < 0.01f )
		return;

	// perpendicular
	const float nx = -dy / len * flHalfWidth;
	const float ny = dx / len * flHalfWidth;

	vgui::Vertex_t verts[4];
	verts[0].Init( Vector2D( x0 + nx, y0 + ny ), Vector2D( 0, 0 ) );
	verts[1].Init( Vector2D( x1 + nx, y1 + ny ), Vector2D( 1, 0 ) );
	verts[2].Init( Vector2D( x1 - nx, y1 - ny ), Vector2D( 1, 1 ) );
	verts[3].Init( Vector2D( x0 - nx, y0 - ny ), Vector2D( 0, 1 ) );

	surface()->DrawSetColor( r, g, b, a );
	surface()->DrawTexturedPolygon( 4, verts );
}

void CHudBS2Veins::DrawVeins( float flLevel, float flThrob, int nScreenW, int nScreenH )
{
	if ( !m_bBuilt )
		BuildVeins();

	if ( m_nWhiteTexture == 0 )
	{
		m_nWhiteTexture = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_nWhiteTexture, "vgui/white", true, false );
	}
	surface()->DrawSetTexture( m_nWhiteTexture );

	const float flScaleX = (float)nScreenW / VEIN_SPACE_W;			// grown space -> pixels
	const float flScaleY = (float)nScreenH;
	const float flPixel = (float)nScreenH / 1080.0f;
	const float flThick = 1.0f + 0.22f * flThrob * flLevel;
	const float flOverall = clamp( 0.35f + flLevel * 0.9f, 0.0f, 1.0f );

	// the front of the crawl: segments are born up to 'flLevel' of their path
	const float flFront = flLevel * 1.02f;

	for ( int i = 0; i < m_Segments.Count(); i++ )
	{
		const BS2VeinSeg_t &seg = m_Segments[i];
		if ( seg.birth > flFront )
			continue;

		const float flGrown = clamp( ( flFront - seg.birth ) / 0.05f, 0.0f, 1.0f );
		const float x0 = seg.x0 * flScaleX;
		const float y0 = seg.y0 * flScaleY;
		const float x1 = x0 + ( seg.x1 * flScaleX - x0 ) * flGrown;
		const float y1 = y0 + ( seg.y1 * flScaleY - y0 ) * flGrown;

		const float flHalf = MAX( 0.55f, seg.width * flPixel * flThick * 0.5f );

		// dark body, then a thin red core
		DrawQuad( x0, y0, x1, y1, flHalf * 1.35f, 14, 0, 4, (int)( 170 * flOverall ) );
		DrawQuad( x0, y0, x1, y1, flHalf * 0.55f, 120 + (int)( 50 * flThrob ), 6, 16, (int)( 210 * flOverall ) );

		// the same line as plain vgui lines (always drawn, whatever the polygon path does)
		surface()->DrawSetColor( 150 + (int)( 40 * flThrob ), 8, 18, (int)( 230 * flOverall ) );
		surface()->DrawLine( (int)x0, (int)y0, (int)x1, (int)y1 );
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

		DrawVeins( m_flLevel, m_flThrob, nScreenW, nScreenH );
	}

	DrawLids( m_flLids, nScreenW, nScreenH );
}
