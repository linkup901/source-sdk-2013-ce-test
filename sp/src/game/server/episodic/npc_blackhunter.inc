// Included after CNPC_Hunter is defined; retains the standard Hunter AI.
class CNPC_BlackHunter : public CNPC_Hunter
{
    DECLARE_CLASS( CNPC_BlackHunter, CNPC_Hunter );
    DECLARE_DATADESC();
public:
    void Precache() OVERRIDE
    {
        BaseClass::Precache();
        PrecacheModel( "models/blackhunter/hunter.mdl" );
        PrecacheModel( "sprites/redglow1.vmt" );
        PrecacheModel( "sprites/laserbeam.vmt" );
    }
    void Spawn() OVERRIDE
    {
        BaseClass::Spawn();
        SetModel( "models/blackhunter/hunter.mdl" );
        const char *eyes[] = { "top_eye", "bottom_eye" };
        for ( int i = 0; i < 2; ++i )
        {
            const int attachment = LookupAttachment( eyes[i] );
            if ( attachment <= 0 ) continue;
            Vector origin; QAngle angles;
            GetAttachment( attachment, origin, angles );
            CSprite *glow = CSprite::SpriteCreate( "sprites/redglow1.vmt", origin, false );
            if ( glow )
            {
                glow->SetAttachment( this, attachment );
                glow->SetTransparency( kRenderTransAdd, 255, 12, 4, 210, kRenderFxNone );
                glow->SetScale( 0.10f );
                glow->SetGlowProxySize( 2.0f );
                m_hEyeEffects[i*2] = glow;
            }
            CSpriteTrail *trail = CSpriteTrail::SpriteTrailCreate( "sprites/laserbeam.vmt", origin, false );
            if ( trail )
            {
                trail->SetAttachment( this, attachment );
                trail->SetTransparency( kRenderTransAdd, 255, 8, 2, 150, kRenderFxNone );
                trail->SetStartWidth( 1.6f );
                trail->SetEndWidth( 0.0f );
                trail->SetLifeTime( 0.16f );
                trail->SetTextureResolution( 0.08f );
                m_hEyeEffects[i*2+1] = trail;
            }
        }
    }
    void RemoveEyeEffects()
    {
        for ( int i = 0; i < 4; ++i )
        {
            if ( m_hEyeEffects[i] ) UTIL_Remove( m_hEyeEffects[i] );
            m_hEyeEffects[i] = NULL;
        }
    }
    void Event_Killed( const CTakeDamageInfo &info ) OVERRIDE
    {
        RemoveEyeEffects();
        BaseClass::Event_Killed( info );
    }
    void UpdateOnRemove() OVERRIDE
    {
        RemoveEyeEffects();
        BaseClass::UpdateOnRemove();
    }
private:
    EHANDLE m_hEyeEffects[4];
};

LINK_ENTITY_TO_CLASS( npc_blackhunter, CNPC_BlackHunter );
BEGIN_DATADESC( CNPC_BlackHunter )
    DEFINE_ARRAY( m_hEyeEffects, FIELD_EHANDLE, 4 ),
END_DATADESC()

