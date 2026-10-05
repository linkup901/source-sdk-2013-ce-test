//========= Black Stasis 2, Phase 2 A3: the skip key detector =========//
//
// Purpose: while logic_skipkey is enabled (the "BS2Skip" user message) watch for the use key and tell the server (console command bs2_skip). The client sees the
// key even when the server has the player frozen under a point_viewcontrol. The "Press E to skip" hint is drawn by the title card element (BS2_Hint_*).
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "in_buttons.h"
#include "iinput.h"
#include "bs2/bs2_ui_messages.h"
#include "bs2_hud.h"
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar bs2_skip_debug( "bs2_skip_debug", "0", 0, "Print the state of the skip key detector twice a second" );

class CHudBS2SkipKey : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBS2SkipKey, vgui::Panel );

public:
	CHudBS2SkipKey( const char *pElementName );

	virtual void	Init( void );
	virtual void	LevelInit( void );
	virtual void	LevelShutdown( void );
	virtual bool	ShouldDraw( void );
	virtual void	ProcessInput( void );

	void			MsgFunc_BS2Skip( bf_read &msg );

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	bool			m_bArmed;
	bool			m_bWaitRelease;		// the key was already down when armed: it has to go up first
};

DECLARE_HUDELEMENT( CHudBS2SkipKey );
DECLARE_HUD_MESSAGE( CHudBS2SkipKey, BS2Skip );

//-----------------------------------------------------------------------------
CHudBS2SkipKey::CHudBS2SkipKey( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudBS2SkipKey" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_bArmed = false;
	m_bWaitRelease = false;
}

void CHudBS2SkipKey::Init( void )
{
	HOOK_HUD_MESSAGE( CHudBS2SkipKey, BS2Skip );
	m_bArmed = false;
}

void CHudBS2SkipKey::LevelInit( void )
{
	m_bArmed = false;
}

void CHudBS2SkipKey::LevelShutdown( void )
{
	m_bArmed = false;
	BS2_Hint_Hide();
}

void CHudBS2SkipKey::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetVisible( false );
	SetSize( 1, 1 );
}

// the HUD calls ProcessInput only on elements that should draw (CHud::Think), so this is true while armed; nothing is painted
bool CHudBS2SkipKey::ShouldDraw( void )
{
	return m_bArmed;
}

void CHudBS2SkipKey::ProcessInput( void )
{
	if ( !m_bArmed )
		return;

	const bool bDown = ( input->GetButtonBits( 0 ) & IN_USE ) != 0;

	if ( bs2_skip_debug.GetBool() )
	{
		static float s_flNext = 0.0f;
		if ( gpGlobals->curtime >= s_flNext )
		{
			s_flNext = gpGlobals->curtime + 0.5f;
			ConMsg( "bs2 skip: armed %d, wait for release %d, use key down %d\n", (int)m_bArmed, (int)m_bWaitRelease, (int)bDown );
		}
	}

	if ( m_bWaitRelease )
	{
		if ( !bDown )
			m_bWaitRelease = false;
		return;
	}

	if ( bDown )
	{
		m_bArmed = false;
		BS2_Hint_Hide();
		if ( bs2_skip_debug.GetBool() )
			ConMsg( "bs2 skip: use key pressed, sending bs2_skip\n" );
		engine->ClientCmd_Unrestricted( "bs2_skip" );
	}
}

void CHudBS2SkipKey::MsgFunc_BS2Skip( bf_read &msg )
{
	int nCmd = msg.ReadByte();

	if ( nCmd == BS2SKIP_ARM )
	{
		float flDelay = msg.ReadFloat();
		char szText[128];
		msg.ReadString( szText, sizeof( szText ) );

		m_bArmed = true;
		m_bWaitRelease = ( input->GetButtonBits( 0 ) & IN_USE ) != 0;

		if ( bs2_skip_debug.GetBool() )
			ConMsg( "bs2 skip: armed (hint in %.1f s, key already down %d)\n", flDelay, (int)m_bWaitRelease );

		if ( flDelay >= 0.0f )
		{
			BS2_Hint_Show( szText, flDelay );
		}
	}
	else
	{
		m_bArmed = false;
		BS2_Hint_Hide();
	}
}
