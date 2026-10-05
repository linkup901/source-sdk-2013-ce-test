//========= Black Stasis 2, Phase 2 A3: logic_skipkey =========//
//
// Purpose: "Press E to skip" for the cinematics (the prelude and the forest intro). While it is enabled the use key fires OnPressedUse - no cheats, no bindings
// to add - as long as a point_viewcontrol is the player's view (RequireCamera).
//
// Why the key press comes from the client: point_viewcontrol usually freezes the player (FL_FROZEN) and the server then reads every button as 0. The
// bs2_skipkey HUD element (client\bs2\bs2_hud_skipkey.cpp) sees the real key state and runs the console command bs2_skip, which fires the enabled entities.
//
// Inputs   Enable (arm; the hint fades in after HintDelay), Disable
// Outputs  OnPressedUse (the entity disables itself; wire it to env_fade 0.3 s -> changelevel / the cut to gameplay)
//
//=============================================================================//

#include "cbase.h"
#include "bs2/bs2_ui_messages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
class CLogicSkipKey : public CPointEntity
{
public:
	DECLARE_CLASS( CLogicSkipKey, CPointEntity );
	DECLARE_DATADESC();

	CLogicSkipKey();

	virtual void Spawn( void );
	virtual void OnRestore( void );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	// the player does not exist yet while the map entities spawn (and not at once after a load): tell the client a little later
	void ArmThink( void );

	// bs2_skip: true if this entity was armed and the press was accepted
	bool OnSkipCommand( CBasePlayer *pPlayer );

	bool IsArmed( void ) const { return m_bArmed; }

private:
	void Arm( float flHintDelay );
	void Disarm( void );
	void SendArm( float flHintDelay );
	void SendDisarm( void );

	bool		m_bArmed;
	bool		m_bStartEnabled;
	bool		m_bRequireCamera;
	float		m_flHintDelay;		// seconds after Enable; < 0: no hint
	string_t	m_iszHintText;

	COutputEvent	m_OnPressedUse;
};

LINK_ENTITY_TO_CLASS( logic_skipkey, CLogicSkipKey );

BEGIN_DATADESC( CLogicSkipKey )

	DEFINE_FIELD( m_bArmed, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_bStartEnabled, FIELD_BOOLEAN, "startenabled" ),
	DEFINE_KEYFIELD( m_bRequireCamera, FIELD_BOOLEAN, "requirecamera" ),
	DEFINE_KEYFIELD( m_flHintDelay, FIELD_FLOAT, "hintdelay" ),
	DEFINE_KEYFIELD( m_iszHintText, FIELD_STRING, "hinttext" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

	DEFINE_OUTPUT( m_OnPressedUse, "OnPressedUse" ),

	DEFINE_THINKFUNC( ArmThink ),

END_DATADESC()

//-----------------------------------------------------------------------------
CLogicSkipKey::CLogicSkipKey()
{
	m_bArmed = false;
	m_bStartEnabled = false;
	m_bRequireCamera = true;
	m_flHintDelay = 3.0f;
	m_iszHintText = NULL_STRING;
}

void CLogicSkipKey::Spawn( void )
{
	BaseClass::Spawn();

	if ( m_bStartEnabled )
	{
		m_bArmed = true;
		SetThink( &CLogicSkipKey::ArmThink );
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
}

void CLogicSkipKey::ArmThink( void )
{
	if ( !m_bArmed )
	{
		SetThink( NULL );
		return;
	}

	if ( UTIL_GetLocalPlayer() )
	{
		SendArm( m_flHintDelay );
		SetThink( NULL );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
}

// a save / load in the middle of a cinematic: the client forgot it was armed
void CLogicSkipKey::OnRestore( void )
{
	BaseClass::OnRestore();

	if ( m_bArmed )
	{
		SetThink( &CLogicSkipKey::ArmThink );
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
}

void CLogicSkipKey::SendArm( float flHintDelay )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	const char *pszText = ( m_iszHintText != NULL_STRING && STRING( m_iszHintText )[0] ) ? STRING( m_iszHintText ) : "Press %use% to skip";

	UserMessageBegin( user, "BS2Skip" );
		WRITE_BYTE( BS2SKIP_ARM );
		WRITE_FLOAT( flHintDelay );
		WRITE_STRING( pszText );
	MessageEnd();
}

void CLogicSkipKey::SendDisarm( void )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "BS2Skip" );
		WRITE_BYTE( BS2SKIP_DISARM );
	MessageEnd();
}

void CLogicSkipKey::Arm( float flHintDelay )
{
	m_bArmed = true;
	SendArm( flHintDelay );
}

void CLogicSkipKey::Disarm( void )
{
	if ( m_bArmed )
	{
		m_bArmed = false;
		SendDisarm();
	}
}

void CLogicSkipKey::InputEnable( inputdata_t &inputdata )
{
	Arm( m_flHintDelay );
}

void CLogicSkipKey::InputDisable( inputdata_t &inputdata )
{
	Disarm();
}

bool CLogicSkipKey::OnSkipCommand( CBasePlayer *pPlayer )
{
	if ( !m_bArmed )
		return false;

	if ( m_bRequireCamera && ( !pPlayer || pPlayer->GetViewEntity() == NULL || pPlayer->GetViewEntity() == pPlayer ) )
		return false;

	Disarm();
	m_OnPressedUse.FireOutput( pPlayer, this );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: run by the client HUD element when the use key goes down while it is armed
//-----------------------------------------------------------------------------
static void BS2Skip_f( void )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();

	CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, "logic_skipkey" );
	while ( pEntity )
	{
		// OnSkipCommand may remove or disable entities through its output: take the next one first
		CBaseEntity *pNext = gEntList.FindEntityByClassname( pEntity, "logic_skipkey" );

		CLogicSkipKey *pSkip = dynamic_cast<CLogicSkipKey *>( pEntity );
		if ( pSkip )
		{
			pSkip->OnSkipCommand( pPlayer );
		}

		pEntity = pNext;
	}
}

static ConCommand bs2_skip( "bs2_skip", BS2Skip_f, "Run by the HUD when the use key is pressed during a cinematic; fires logic_skipkey." );
