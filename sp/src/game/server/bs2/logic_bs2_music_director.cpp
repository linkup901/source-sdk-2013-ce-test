//========= Black Stasis 2: logic_bs2_music_director (Claude, 15 Nov 2026, audio idea C9) =========//
//
// Purpose: gameplay-driven music intensity. Watches how many living NPCs within Radius have the player as their enemy and
// fires outputs the map wires to its existing music (ambient_generic FadeIn/FadeOut/Volume, PlaySound/StopSound):
//   OnCombatStart   first NPC turns on the player (after StartDelay of sustained threat, so a single glance doesn't flip it)
//   OnCombatEnd     no NPC has had the player as enemy for CalmDelay seconds
//   OnThreat        0..1 = hunting NPCs / ThreatMax, fired when it changes by 0.1 or more (wire to a layer's Volume via
//                   a math_remap if wanted)
// Inputs  Enable, Disable (Disable while in combat fires OnCombatEnd), ForceCalm
// Keys    radius (2048), calmdelay (8), startdelay (0.6), threatmax (6), StartDisabled (0)
//=============================================================================//

#include "cbase.h"
#include "ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CBS2MusicDirector : public CLogicalEntity
{
public:
	DECLARE_CLASS( CBS2MusicDirector, CLogicalEntity );
	DECLARE_DATADESC();

	CBS2MusicDirector()
	{
		m_flRadius = 2048.0f; m_flCalmDelay = 8.0f; m_flStartDelay = 0.6f; m_flThreatMax = 6.0f;
		m_bDisabled = false; m_bCombat = false; m_flLastThreat = 0; m_flThreatSince = -1; m_flLastSent = -1;
	}

	void Spawn( void );
	void Think( void );
	void InputEnable( inputdata_t &inputdata ) { m_bDisabled = false; SetNextThink( gpGlobals->curtime + 0.1f ); }
	void InputDisable( inputdata_t &inputdata ) { m_bDisabled = true; EndCombat( inputdata.pActivator ); }
	void InputForceCalm( inputdata_t &inputdata ) { EndCombat( inputdata.pActivator ); m_flThreatSince = -1; }

private:
	int CountHunters( CBasePlayer *pPlayer );
	void EndCombat( CBaseEntity *pActivator );

	float	m_flRadius, m_flCalmDelay, m_flStartDelay, m_flThreatMax;
	bool	m_bDisabled, m_bCombat;
	float	m_flLastThreat;		// time a hunter was last seen
	float	m_flThreatSince;	// time the current run of threat started (-1 = none)
	float	m_flLastSent;		// last OnThreat value
	COutputEvent	m_OnCombatStart, m_OnCombatEnd;
	COutputFloat	m_OnThreat;
};

LINK_ENTITY_TO_CLASS( logic_bs2_music_director, CBS2MusicDirector );

BEGIN_DATADESC( CBS2MusicDirector )
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_KEYFIELD( m_flCalmDelay, FIELD_FLOAT, "calmdelay" ),
	DEFINE_KEYFIELD( m_flStartDelay, FIELD_FLOAT, "startdelay" ),
	DEFINE_KEYFIELD( m_flThreatMax, FIELD_FLOAT, "threatmax" ),
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_bCombat, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flLastThreat, FIELD_TIME ),
	DEFINE_FIELD( m_flThreatSince, FIELD_TIME ),
	DEFINE_FIELD( m_flLastSent, FIELD_FLOAT ),
	DEFINE_THINKFUNC( Think ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ForceCalm", InputForceCalm ),
	DEFINE_OUTPUT( m_OnCombatStart, "OnCombatStart" ),
	DEFINE_OUTPUT( m_OnCombatEnd, "OnCombatEnd" ),
	DEFINE_OUTPUT( m_OnThreat, "OnThreat" ),
END_DATADESC()

void CBS2MusicDirector::Spawn( void )
{
	BaseClass::Spawn();
	SetThink( &CBS2MusicDirector::Think );
	SetNextThink( gpGlobals->curtime + 1.0f );
}

int CBS2MusicDirector::CountHunters( CBasePlayer *pPlayer )
{
	int n = 0; const float r2 = m_flRadius * m_flRadius;
	CAI_BaseNPC **ppAIs = g_AI_Manager.AccessAIs();
	for ( int i = 0; i < g_AI_Manager.NumAIs(); i++ )
	{
		CAI_BaseNPC *pNPC = ppAIs[i];
		if ( !pNPC || !pNPC->IsAlive() || pNPC->GetEnemy() != pPlayer ) continue;
		if ( ( pNPC->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).LengthSqr() > r2 ) continue;
		n++;
	}
	return n;
}

void CBS2MusicDirector::EndCombat( CBaseEntity *pActivator )
{
	if ( !m_bCombat ) return;
	m_bCombat = false; m_OnCombatEnd.FireOutput( pActivator ? pActivator : this, this );
	if ( m_flLastSent != 0 ) { m_flLastSent = 0; m_OnThreat.Set( 0.0f, this, this ); }
}

void CBS2MusicDirector::Think( void )
{
	if ( m_bDisabled ) return;   // Enable restarts thinking
	SetNextThink( gpGlobals->curtime + 0.25f );
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() ) { EndCombat( pPlayer ); return; }
	const int n = CountHunters( pPlayer ); const float now = gpGlobals->curtime;
	if ( n > 0 )
	{
		m_flLastThreat = now;
		if ( m_flThreatSince < 0 ) m_flThreatSince = now;
		if ( !m_bCombat && now - m_flThreatSince >= m_flStartDelay ) { m_bCombat = true; m_OnCombatStart.FireOutput( pPlayer, this ); }
	}
	else
	{
		m_flThreatSince = -1;
		if ( m_bCombat && now - m_flLastThreat >= m_flCalmDelay ) EndCombat( pPlayer );
	}
	if ( m_bCombat )
	{
		float t = clamp( n / MAX( m_flThreatMax, 1.0f ), 0.0f, 1.0f );
		if ( fabs( t - m_flLastSent ) >= 0.1f ) { m_flLastSent = t; m_OnThreat.Set( t, pPlayer, this ); }
	}
}
