//========= Black Stasis 2, Phase 2 A5: env_bs2_veins =========//
//
// Purpose: drives the vein overlay of the bs2_veins HUD element (client\bs2\bs2_hud_veins.cpp) from the map: a pulse for scripted collapses (power station,
// factory night), or a steady level (the infection creeping in over a scene). Damage veins and the death eyelids need no entity: the HUD element watches
// the player's health itself.
//
// Inputs   Pulse (strength / duration keyvalues), PulseStrong (strength 1), SetLevel <0..1> (ramps over RampTime), Clear
//
//=============================================================================//

#include "cbase.h"
#include "bs2/bs2_ui_messages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CBS2Veins : public CPointEntity
{
public:
	DECLARE_CLASS( CBS2Veins, CPointEntity );
	DECLARE_DATADESC();

	CBS2Veins()
	{
		m_flStrength = 0.9f;
		m_flDuration = 3.0f;
		m_flRampTime = 2.0f;
	}

	void InputPulse( inputdata_t &inputdata ) { SendPulse( m_flStrength, m_flDuration ); }
	void InputPulseStrong( inputdata_t &inputdata ) { SendPulse( 1.0f, m_flDuration ); }
	void InputSetLevel( inputdata_t &inputdata );
	void InputClear( inputdata_t &inputdata );

private:
	void SendPulse( float flStrength, float flDuration );

	float	m_flStrength;		// 0..1
	float	m_flDuration;		// seconds the pulse lasts
	float	m_flRampTime;		// seconds SetLevel takes to get there
};

LINK_ENTITY_TO_CLASS( env_bs2_veins, CBS2Veins );

BEGIN_DATADESC( CBS2Veins )

	DEFINE_KEYFIELD( m_flStrength, FIELD_FLOAT, "strength" ),
	DEFINE_KEYFIELD( m_flDuration, FIELD_FLOAT, "duration" ),
	DEFINE_KEYFIELD( m_flRampTime, FIELD_FLOAT, "ramptime" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Pulse", InputPulse ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PulseStrong", InputPulseStrong ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetLevel", InputSetLevel ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Clear", InputClear ),

END_DATADESC()

void CBS2Veins::SendPulse( float flStrength, float flDuration )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "BS2Veins" );
		WRITE_BYTE( BS2VEINS_PULSE );
		WRITE_FLOAT( flStrength );
		WRITE_FLOAT( flDuration );
	MessageEnd();
}

void CBS2Veins::InputSetLevel( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "BS2Veins" );
		WRITE_BYTE( BS2VEINS_LEVEL );
		WRITE_FLOAT( clamp( inputdata.value.Float(), 0.0f, 1.0f ) );
		WRITE_FLOAT( m_flRampTime );
	MessageEnd();
}

void CBS2Veins::InputClear( inputdata_t &inputdata )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "BS2Veins" );
		WRITE_BYTE( BS2VEINS_CLEAR );
	MessageEnd();
}
