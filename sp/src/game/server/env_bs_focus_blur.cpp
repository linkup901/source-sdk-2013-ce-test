#include "cbase.h"
#include "tier0/memdbgon.h"

// Singleplayer, full-screen focus control. All transition state is saved and
// networked, so loading a game resumes the same transition.
class CEnvBSFocusBlur : public CPointEntity
{
    DECLARE_CLASS( CEnvBSFocusBlur, CPointEntity );
public:
    DECLARE_DATADESC();
    DECLARE_SERVERCLASS();
    CEnvBSFocusBlur()
    {
        AddEFlags( EFL_FORCE_CHECK_TRANSMIT );
        m_flInitial = 0;
        m_flRadius = 32;
        m_flFrom = m_flTo = m_flStart = m_flDuration = 0;
    }
    int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }
    void Spawn()
    {
        BaseClass::Spawn();
        Precache();
        m_flRadius = clamp( m_flRadius.Get(), 0.0f, 32.0f );
        m_flFrom = m_flTo = clamp( m_flInitial, 0.0f, 1.0f );
        m_flStart = gpGlobals->curtime;
        m_flDuration = 0;
    }
    void Precache() { PrecacheMaterial( "effects/bs_focus_blur" ); }
private:
    float Amount() const
    {
        float t = m_flDuration <= 0 ? 1 : clamp( (gpGlobals->curtime - m_flStart) / m_flDuration, 0.0f, 1.0f );
        t = t * t * (3 - 2 * t);
        return m_flFrom + (m_flTo - m_flFrom) * t;
    }
    void Transition( float target, float seconds )
    {
        m_flFrom = Amount();
        m_flTo = target;
        m_flStart = gpGlobals->curtime;
        m_flDuration = MAX( seconds, 0.0f );
    }
    void InputBlurIn( inputdata_t &data ) { Transition( 0, data.value.Float() ); }
    void InputBlurOut( inputdata_t &data ) { Transition( 1, data.value.Float() ); }
    void InputSetBlur( inputdata_t &data ) { Transition( clamp( data.value.Float(), 0.0f, 1.0f ), 0 ); }
    CNetworkVar( float, m_flFrom );
    CNetworkVar( float, m_flTo );
    CNetworkVar( float, m_flStart );
    CNetworkVar( float, m_flDuration );
    CNetworkVar( float, m_flRadius );
    float m_flInitial;
};

LINK_ENTITY_TO_CLASS( env_bs_focus_blur, CEnvBSFocusBlur );
BEGIN_DATADESC( CEnvBSFocusBlur )
    DEFINE_KEYFIELD( m_flInitial, FIELD_FLOAT, "initialblur" ),
    DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
    DEFINE_FIELD( m_flFrom, FIELD_FLOAT ),
    DEFINE_FIELD( m_flTo, FIELD_FLOAT ),
    DEFINE_FIELD( m_flStart, FIELD_TIME ),
    DEFINE_FIELD( m_flDuration, FIELD_FLOAT ),
    DEFINE_INPUTFUNC( FIELD_FLOAT, "BlurIn", InputBlurIn ),
    DEFINE_INPUTFUNC( FIELD_FLOAT, "BlurOut", InputBlurOut ),
    DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBlur", InputSetBlur ),
END_DATADESC()
IMPLEMENT_SERVERCLASS_ST( CEnvBSFocusBlur, DT_BSFocusBlur )
    SendPropFloat( SENDINFO( m_flFrom ), 0, SPROP_NOSCALE ),
    SendPropFloat( SENDINFO( m_flTo ), 0, SPROP_NOSCALE ),
    SendPropFloat( SENDINFO( m_flStart ), 0, SPROP_NOSCALE ),
    SendPropFloat( SENDINFO( m_flDuration ), 0, SPROP_NOSCALE ),
    SendPropFloat( SENDINFO( m_flRadius ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()
