//========= Black Stasis 2, Phase 2 A2: point_bs2_credits =========//
//
// Purpose: starts / stops the credits roll of the bs2_credits HUD element (client\bs2\bs2_hud_credits.cpp): credits.txt scrolling up over black.
//
// Inputs   Start, Stop, Cue (fires OnMusicCue now)
// Outputs  OnStarted, OnMusicCue (Start + musiccuedelay seconds, or the Cue input), OnFinished (the client reports the roll has run out: console command
//          bs2_creditsdone, like HL2's creditsdone)
//
//=============================================================================//

#include "cbase.h"
#include "bs2/bs2_ui_messages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
class CBS2Credits : public CPointEntity
{
public:
	DECLARE_CLASS( CBS2Credits, CPointEntity );
	DECLARE_DATADESC();

	CBS2Credits();

	void InputStart( inputdata_t &inputdata );
	void InputStop( inputdata_t &inputdata );
	void InputCue( inputdata_t &inputdata );

	void CueThink( void );

	COutputEvent	m_OnStarted;
	COutputEvent	m_OnMusicCue;
	COutputEvent	m_OnFinished;

private:
	string_t	m_iszFile;			// default credits.txt
	float		m_flSpeed;			// pixels per second at 1080 lines
	float		m_flFadeIn;			// seconds the black takes to cover the screen
	float		m_flHoldAfter;		// seconds to stay on black after the last line has gone
	float		m_flMusicCueDelay;	// < 0: only the Cue input
	bool		m_bStopAtEnd;		// the last line comes to rest in the middle of the screen
};

LINK_ENTITY_TO_CLASS( point_bs2_credits, CBS2Credits );

BEGIN_DATADESC( CBS2Credits )

	DEFINE_KEYFIELD( m_iszFile, FIELD_STRING, "file" ),
	DEFINE_KEYFIELD( m_flSpeed, FIELD_FLOAT, "speed" ),
	DEFINE_KEYFIELD( m_flFadeIn, FIELD_FLOAT, "fadein" ),
	DEFINE_KEYFIELD( m_flHoldAfter, FIELD_FLOAT, "holdafter" ),
	DEFINE_KEYFIELD( m_flMusicCueDelay, FIELD_FLOAT, "musiccuedelay" ),
	DEFINE_KEYFIELD( m_bStopAtEnd, FIELD_BOOLEAN, "stopatend" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Start", InputStart ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Stop", InputStop ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Cue", InputCue ),

	DEFINE_OUTPUT( m_OnStarted, "OnStarted" ),
	DEFINE_OUTPUT( m_OnMusicCue, "OnMusicCue" ),
	DEFINE_OUTPUT( m_OnFinished, "OnFinished" ),

	DEFINE_THINKFUNC( CueThink ),

END_DATADESC()

//-----------------------------------------------------------------------------
CBS2Credits::CBS2Credits()
{
	m_iszFile = NULL_STRING;
	m_flSpeed = 55.0f;
	m_flFadeIn = 2.0f;
	m_flHoldAfter = 3.0f;
	m_flMusicCueDelay = -1.0f;
	m_bStopAtEnd = false;
}

void CBS2Credits::InputStart( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	const char *pszFile = ( m_iszFile != NULL_STRING && STRING( m_iszFile )[0] ) ? STRING( m_iszFile ) : "credits.txt";

	UserMessageBegin( user, "BS2Credits" );
		WRITE_BYTE( BS2CREDITS_START );
		WRITE_STRING( pszFile );
		WRITE_FLOAT( m_flSpeed );
		WRITE_FLOAT( m_flFadeIn );
		WRITE_FLOAT( m_flHoldAfter );
		WRITE_SHORT( m_bStopAtEnd ? 1 : 0 );
	MessageEnd();

	m_OnStarted.FireOutput( inputdata.pActivator, this );

	if ( m_flMusicCueDelay >= 0.0f )
	{
		SetThink( &CBS2Credits::CueThink );
		SetNextThink( gpGlobals->curtime + m_flMusicCueDelay );
	}
}

void CBS2Credits::InputStop( inputdata_t &inputdata )
{
	SetThink( NULL );

	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "BS2Credits" );
		WRITE_BYTE( BS2CREDITS_STOP );
	MessageEnd();
}

void CBS2Credits::InputCue( inputdata_t &inputdata )
{
	m_OnMusicCue.FireOutput( inputdata.pActivator, this );
}

void CBS2Credits::CueThink( void )
{
	SetThink( NULL );
	m_OnMusicCue.FireOutput( this, this );
}

//-----------------------------------------------------------------------------
// Purpose: the HUD element runs "bs2_creditsdone" when the roll (and the hold time after it) is over
//-----------------------------------------------------------------------------
static void BS2CreditsDone_f( void )
{
	CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, "point_bs2_credits" );
	while ( pEntity )
	{
		CBS2Credits *pCredits = dynamic_cast<CBS2Credits *>( pEntity );
		if ( pCredits )
		{
			pCredits->m_OnFinished.FireOutput( pCredits, pCredits );
		}
		pEntity = gEntList.FindEntityByClassname( pEntity, "point_bs2_credits" );
	}
}

static ConCommand bs2_creditsdone( "bs2_creditsdone", BS2CreditsDone_f, "Run by the credits roll when it has finished." );
