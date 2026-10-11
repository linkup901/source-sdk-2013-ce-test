#include "cbase.h"
#include "bs_focus_blur.h"
#include "tier1/utlvector.h"
#include "tier0/memdbgon.h"

class C_EnvBSFocusBlur;
static CUtlVector<C_EnvBSFocusBlur *> s_Controllers;

class C_EnvBSFocusBlur : public C_BaseEntity
{
    DECLARE_CLASS( C_EnvBSFocusBlur, C_BaseEntity );
public:
    DECLARE_CLIENTCLASS();
    C_EnvBSFocusBlur() : m_flFrom(0), m_flTo(0), m_flStart(0), m_flDuration(0), m_flRadius(0)
    { s_Controllers.AddToTail( this ); }
    ~C_EnvBSFocusBlur() { s_Controllers.FindAndRemove( this ); }
    float Radius() const
    {
        float t = m_flDuration <= 0 ? 1 : clamp( (gpGlobals->curtime - m_flStart) / m_flDuration, 0.0f, 1.0f );
        t = t * t * (3 - 2 * t);
        return clamp( m_flFrom + (m_flTo - m_flFrom) * t, 0.0f, 1.0f ) * clamp( m_flRadius, 0.0f, 32.0f );
    }
private:
    float m_flFrom, m_flTo, m_flStart, m_flDuration, m_flRadius;
};
IMPLEMENT_CLIENTCLASS_DT( C_EnvBSFocusBlur, DT_BSFocusBlur, CEnvBSFocusBlur )
    RecvPropFloat( RECVINFO( m_flFrom ) ),
    RecvPropFloat( RECVINFO( m_flTo ) ),
    RecvPropFloat( RECVINFO( m_flStart ) ),
    RecvPropFloat( RECVINFO( m_flDuration ) ),
    RecvPropFloat( RECVINFO( m_flRadius ) ),
END_RECV_TABLE()

float BS_FocusBlurRadius()
{
    float radius = 0;
    for ( int i = 0; i < s_Controllers.Count(); ++i )
        radius = MAX( radius, s_Controllers[i]->Radius() );
    return radius;
}
