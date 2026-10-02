// Included by npc_headcrab.cpp to reuse the fast headcrab's movement and attacks.
// A mapper opts a visible switch into companion use with this floor marker.
class CCrabbySwitch : public CPointEntity
{
public:
    DECLARE_CLASS( CCrabbySwitch, CPointEntity );
    DECLARE_DATADESC();
    CCrabbySwitch() : m_bDisabled(false), m_bUsed(false) {}
    CBaseEntity *SwitchEntity() { return gEntList.FindEntityByName(NULL, m_iszSwitch); }
    bool Available() { return !m_bDisabled && !m_bUsed; }
    void InputEnable(inputdata_t &) { m_bDisabled = false; }
    void InputDisable(inputdata_t &) { m_bDisabled = true; }
    void InputReset(inputdata_t &) { m_bUsed = false; }
    string_t m_iszSwitch;
    bool m_bDisabled, m_bUsed;
    COutputEvent m_OnSearchStarted, m_OnPressed, m_OnSearchFailed, m_OnSearchCancelled;
};
LINK_ENTITY_TO_CLASS(info_crabby_switch, CCrabbySwitch);
BEGIN_DATADESC(CCrabbySwitch)
    DEFINE_KEYFIELD(m_iszSwitch, FIELD_STRING, "switch_entity"),
    DEFINE_KEYFIELD(m_bDisabled, FIELD_BOOLEAN, "StartDisabled"),
    DEFINE_FIELD(m_bUsed, FIELD_BOOLEAN),
    DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
    DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
    DEFINE_INPUTFUNC(FIELD_VOID, "Reset", InputReset),
    DEFINE_OUTPUT(m_OnSearchStarted, "OnSearchStarted"),
    DEFINE_OUTPUT(m_OnPressed, "OnPressed"),
    DEFINE_OUTPUT(m_OnSearchFailed, "OnSearchFailed"),
    DEFINE_OUTPUT(m_OnSearchCancelled, "OnSearchCancelled"),
END_DATADESC()

// Story size. The fast headcrab is scaled down; navigation keeps HULL_TINY.
#define CRABBY_DEFAULT_SCALE    0.7f
// How far from the player's eyes Crabby can be picked up (Use key).
#define CRABBY_PICKUP_REACH     110.0f
// Set-down point must be this close to the eyes when aiming at the floor.
#define CRABBY_SETDOWN_REACH    160.0f

class CNPC_Crabby : public CFastHeadcrab
{
public:
    DECLARE_CLASS( CNPC_Crabby, CFastHeadcrab );
    DECLARE_DATADESC();
    DECLARE_SERVERCLASS();

    CNPC_Crabby() : m_flCrabbyScale(CRABBY_DEFAULT_SCALE), m_bAllowRiding(true) {}

    void Precache() { PrecacheModel( "models/crabby/crabby.mdl" ); BaseClass::Precache(); }
    void Spawn()
    {
        BaseClass::Spawn();
        SetModel( "models/crabby/crabby.mdl" );
        float scale = m_flCrabbyScale > 0.0f ? m_flCrabbyScale : CRABBY_DEFAULT_SCALE;
        SetModelScale( clamp( scale, 0.3f, 1.5f ) );
        RestoreTinyHull();
        m_iHealth = m_iMaxHealth = 100;
        m_bFollowing = true;
        m_flNextAssist = 0;
        m_bRiding = false;
        m_flNextRideIdle = 0;
        m_iForcedSeq = -1;
        m_flForcedSeqEnd = 0;
    }
    // SetModel and SetModelScale both rewrite the collision box. Navigation
    // plans with HULL_TINY, so collision must match it at any visual scale.
    void RestoreTinyHull()
    {
        SetHullSizeNormal( true );
        float s = GetModelScale();
        if ( s > 0.0f && s != 1.0f )
            UTIL_SetSize( this, GetHullMins() / s, GetHullMaxs() / s );
    }
    void OnRestore()
    {
        BaseClass::OnRestore();
        // A ride that did not survive a level change (or a parent that is gone)
        // must not leave Crabby frozen in mid air.
        if ( m_bRiding && !ToBasePlayer( GetMoveParent() ) )
            EndRide( NULL, NULL );
    }
    Class_T Classify() { return CLASS_PLAYER_ALLY; }
    Disposition_t IRelationType( CBaseEntity *pOther )
    {
        if ( !pOther ) return D_NU;
        if ( pOther->IsPlayer() || pOther->Classify() == CLASS_PLAYER_ALLY ||
             pOther->Classify() == CLASS_PLAYER_ALLY_VITAL ) return D_LI;
        return pOther == m_hAttackTarget.Get() ? D_HT : D_NU;
    }
    int OnTakeDamage_Alive( const CTakeDamageInfo &info )
    {
        if ( m_bRiding ) return 0;  // the player's body shields it
        if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() ) return 0;
        return BaseClass::OnTakeDamage_Alive( info );
    }
    int TranslateSchedule( int schedule )
    {
        if ( schedule == SCHED_IDLE_STAND || schedule == SCHED_WAIT_FOR_SCRIPT ) return schedule;
        return BaseClass::TranslateSchedule( schedule );
    }

    //-------------------------------------------------------------------------
    // Hand-played sequences (paw press, landing, look-around on the shoulder).
    // The schedule waits while one plays so the AI does not override it.
    //-------------------------------------------------------------------------
    float PlayCrabbySequence( const char *name, float rate = 1.0f )
    {
        int seq = LookupSequence( name );
        if ( seq < 0 ) return 0.0f;
        SetIdealActivity( ACT_DO_NOT_DISTURB );
        ResetSequence( seq );
        SetCycle( 0 );
        m_flPlaybackRate = rate;
        m_iForcedSeq = seq;
        float duration = SequenceDuration( seq ) / MAX( rate, 0.1f );
        m_flForcedSeqEnd = gpGlobals->curtime + duration;
        ClearSchedule( "Crabby plays a sequence" );
        return duration;
    }
    void StopCrabbySequence()
    {
        if ( !m_flForcedSeqEnd ) return;
        m_flForcedSeqEnd = 0;
        m_iForcedSeq = -1;
        m_flPlaybackRate = 1.0f;
        SetIdealActivity( ACT_IDLE );
        ClearSchedule( "Crabby sequence done" );
    }

    void CommandAttack( CBaseEntity *pTarget )
    {
        if (m_hPuzzle) return;
        if ( !pTarget || !pTarget->IsNPC() || !pTarget->IsAlive() ||
             pTarget->Classify() == CLASS_PLAYER_ALLY ||
             pTarget->Classify() == CLASS_PLAYER_ALLY_VITAL || pTarget == this ) return;
        if ( m_bRiding )
        {
            Vector face = pTarget->WorldSpaceCenter();
            EndRide( ToBasePlayer( GetMoveParent() ), NULL, &face );
        }
        StopCrabbySequence();
        m_hAttackTarget = pTarget;
        SetEnemy( pTarget );
        UpdateEnemyMemory( pTarget, pTarget->GetAbsOrigin() );
        SetIdealState( NPC_STATE_COMBAT );
        ClearSchedule( "Crabby attack command" );
    }
    void StopAttacking()
    {
        m_hAttackTarget = NULL;
        SetEnemy( NULL );
        SetIdealState( NPC_STATE_IDLE );
        ClearSchedule( "Crabby recalled" );
    }

    //-------------------------------------------------------------------------
    // Shoulder riding
    //-------------------------------------------------------------------------
    bool IsRiding() const { return m_bRiding; }
    bool CanStartRide( CBasePlayer *pPlayer )
    {
        return pPlayer && pPlayer->IsAlive() && IsAlive() && !m_bRiding && m_bAllowRiding &&
               !pPlayer->IsInAVehicle() && pPlayer->GetWaterLevel() < WL_Waist &&
               pPlayer->GetMoveType() != MOVETYPE_NOCLIP;
    }
    // Server-side seat on the left shoulder. The local first-person view draws
    // Crabby view-locked in the lower-left corner instead (c_npc_crabby.cpp).
    void UpdateSeat( CBasePlayer *pPlayer )
    {
        Vector forward, right;
        QAngle yaw( 0, pPlayer->EyeAngles().y, 0 );
        AngleVectors( yaw, &forward, &right, NULL );
        Vector seat = pPlayer->GetAbsOrigin() + forward * 2.0f - right * 11.0f;
        seat.z += pPlayer->GetViewOffset().z - 12.0f;
        SetAbsOrigin( seat );
        SetAbsAngles( yaw );
    }
    bool BeginRide( CBasePlayer *pPlayer )
    {
        if ( !CanStartRide( pPlayer ) ) return false;
        if ( m_hPuzzle ) FinishPuzzle( 0 );
        StopCrabbySequence();
        StopAttacking();
        m_bFollowing = true;
        GetNavigator()->ClearGoal();
        m_bRiding = true;
        SetGroundEntity( NULL );
        SetMoveType( MOVETYPE_NONE );
        AddSolidFlags( FSOLID_NOT_SOLID );
        AddEffects( EF_NOSHADOW );
        SetAbsVelocity( vec3_origin );
        SetParent( pPlayer );
        UpdateSeat( pPlayer );
        m_flNextRideIdle = gpGlobals->curtime + random->RandomFloat( 5.0f, 10.0f );
        ClearSchedule( "Crabby climbs onto the shoulder" );
        EmitSound( "NPC_FastHeadcrab.Idle" );
        m_OnRideStart.FireOutput( pPlayer, this );
        return true;
    }
    // Finds standable floor for the tiny hull. Prefers the aimed spot, then
    // in front, left, right and behind the player, and never through a wall.
    bool FindDismountSpot( CBasePlayer *pPlayer, const Vector *pAim, Vector &out )
    {
        Vector candidates[6];
        int count = 0;
        if ( pAim ) candidates[count++] = *pAim;
        Vector eye = pPlayer ? pPlayer->EyePosition() : WorldSpaceCenter();
        if ( pPlayer )
        {
            Vector forward, right;
            AngleVectors( QAngle( 0, pPlayer->EyeAngles().y, 0 ), &forward, &right, NULL );
            Vector base = pPlayer->GetAbsOrigin();
            candidates[count++] = base + forward * 40.0f;
            candidates[count++] = base - right * 36.0f;
            candidates[count++] = base + right * 36.0f;
            candidates[count++] = base - forward * 36.0f;
        }
        else
            candidates[count++] = GetAbsOrigin();

        Vector mins = WorldAlignMins(), maxs = WorldAlignMaxs();
        CTraceFilterSkipTwoEntities filter( this, pPlayer, COLLISION_GROUP_NPC );
        for ( int i = 0; i < count; ++i )
        {
            trace_t tr;
            UTIL_TraceHull( candidates[i] + Vector( 0, 0, 24 ), candidates[i] - Vector( 0, 0, 72 ),
                            mins, maxs, MASK_NPCSOLID, &filter, &tr );
            if ( tr.startsolid || tr.allsolid || tr.fraction >= 1.0f || tr.plane.normal.z < 0.7f )
                continue;
            trace_t sight;
            UTIL_TraceLine( eye, tr.endpos + Vector( 0, 0, 8 ), MASK_SOLID_BRUSHONLY, pPlayer, COLLISION_GROUP_NONE, &sight );
            if ( sight.fraction < 0.98f )
                continue;
            out = tr.endpos;
            return true;
        }
        out = pPlayer ? pPlayer->GetAbsOrigin() : GetAbsOrigin();
        return false;
    }
    void EndRide( CBasePlayer *pPlayer, const Vector *pAim, const Vector *pFace = NULL )
    {
        if ( !m_bRiding ) return;
        Vector spot;
        FindDismountSpot( pPlayer, pAim, spot );
        QAngle facing( 0, pPlayer ? pPlayer->EyeAngles().y : GetAbsAngles().y, 0 );
        if ( pFace ) facing.y = UTIL_VecToYaw( *pFace - spot );

        SetParent( NULL );
        m_bRiding = false;
        RemoveSolidFlags( FSOLID_NOT_SOLID );
        RemoveEffects( EF_NOSHADOW );
        SetMoveType( MOVETYPE_STEP );
        Teleport( &spot, &facing, &vec3_origin );
        GetMotor()->SetIdealYaw( facing.y );
        UTIL_DropToFloor( this, MASK_NPCSOLID );
        RestoreTinyHull();
        GetNavigator()->ClearGoal();
        EmitSound( "NPC_FastHeadcrab.Idle" );
        if ( !pFace ) PlayCrabbySequence( "ceiling_land" );
        else ClearSchedule( "Crabby hops off" );
        if ( pPlayer ) m_OnRideEnd.FireOutput( pPlayer, this );
        else m_OnRideEnd.FireOutput( this, this );
    }
    // Runs every think while riding.
    void RideThink()
    {
        CBasePlayer *pRider = ToBasePlayer( GetMoveParent() );
        if ( !pRider || !pRider->IsAlive() || pRider->IsInAVehicle() ||
             pRider->GetWaterLevel() >= WL_Waist || pRider->GetMoveType() == MOVETYPE_NOCLIP )
        {
            EndRide( pRider, NULL );
            return;
        }
        UpdateSeat( pRider );

        // Hop off to help when the player opens fire on something hostile.
        if ( gpGlobals->curtime >= m_flNextAssist )
        {
            m_flNextAssist = gpGlobals->curtime + 0.15f;
            if ( pRider->m_nButtons & IN_ATTACK )
            {
                CBaseEntity *pTarget = AimedHostile( pRider );
                if ( pTarget ) { CommandAttack( pTarget ); return; }
            }
        }

        // A little life on the shoulder: a chirp or a look around now and then.
        if ( gpGlobals->curtime >= m_flNextRideIdle && !m_flForcedSeqEnd )
        {
            m_flNextRideIdle = gpGlobals->curtime + random->RandomFloat( 7.0f, 16.0f );
            if ( random->RandomInt( 0, 2 ) == 0 ) IdleSound();
            else PlayCrabbySequence( "lookaround" );
        }
    }
    CBaseEntity *AimedHostile( CBasePlayer *pPlayer )
    {
        Vector forward;
        pPlayer->EyeVectors( &forward );
        trace_t tr;
        UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + forward * 2048,
                        MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
        if ( !tr.m_pEnt || tr.m_pEnt == this || tr.m_pEnt == m_hAttackTarget.Get() ) return NULL;
        // Ordinary player allies fear zombies. Fear still denotes an enemy
        // that Crabby should help the player fight when commanded by aim.
        Disposition_t relation = BaseClass::IRelationType( tr.m_pEnt );
        return ( relation == D_HT || relation == D_FR ) ? tr.m_pEnt : NULL;
    }

    void PrescheduleThink()
    {
        BaseClass::PrescheduleThink();

        if ( m_flForcedSeqEnd )
        {
            if ( gpGlobals->curtime >= m_flForcedSeqEnd || !IsAlive() ) StopCrabbySequence();
            else if ( GetIdealActivity() != ACT_DO_NOT_DISTURB ) SetIdealActivity( ACT_DO_NOT_DISTURB );
        }

        if ( m_bRiding ) { RideThink(); return; }

        if (m_flPuzzleDeadline && !m_hPuzzle) FinishPuzzle(2);
        CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
        if ( !pPlayer || !pPlayer->IsAlive() ) { if(m_hPuzzle) FinishPuzzle(0); return; }
        if (m_hPuzzle)
        {
            CCrabbySwitch *p = static_cast<CCrabbySwitch *>(m_hPuzzle.Get());
            CBaseEntity *button = p->SwitchEntity();
            if (!p->Available() || !button || gpGlobals->curtime > m_flPuzzleDeadline)
            { FinishPuzzle(2); return; }
            if (m_flPressTime)
            {
                // Paw press in progress: the rear-up comes down on the switch.
                if (gpGlobals->curtime >= m_flPressTime) FinishPuzzle(1);
                return;
            }
            Vector delta = p->GetAbsOrigin() - GetAbsOrigin();
            if (delta.Length2D() <= 28 && fabs(delta.z) <= 32 &&
                (button->WorldSpaceCenter() - WorldSpaceCenter()).Length() <= 96)
            {
                trace_t tr;
                UTIL_TraceLine(WorldSpaceCenter(), button->WorldSpaceCenter(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
                if (tr.fraction == 1 || tr.m_pEnt == button)
                {
                    GetNavigator()->ClearGoal();
                    QAngle face( 0, UTIL_VecToYaw( button->WorldSpaceCenter() - GetAbsOrigin() ), 0 );
                    SetAbsAngles( face );
                    GetMotor()->SetIdealYaw( face.y );
                    float duration = PlayCrabbySequence( "rearup" );
                    m_flPressTime = gpGlobals->curtime + ( duration > 0 ? clamp( duration * 0.6f, 0.3f, 1.2f ) : 0.6f );
                    return;
                }
            }
            if (!IsCurSchedule(SCHED_TARGET_CHASE)) ClearSchedule("Crabby searches for switch route");
            return;
        }
        if ( m_hAttackTarget && ( !m_hAttackTarget->IsAlive() ||
             (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() > 1024*1024 ) ) StopAttacking();
        if ( m_bFollowing && gpGlobals->curtime >= m_flNextAssist )
        {
            m_flNextAssist = gpGlobals->curtime + 0.15f;
            if ( pPlayer->m_nButtons & IN_ATTACK )
            {
                CBaseEntity *pTarget = AimedHostile( pPlayer );
                if ( pTarget ) CommandAttack( pTarget );
            }
            if ( !GetEnemy() && !m_flForcedSeqEnd && !IsCurSchedule( SCHED_TARGET_CHASE ) &&
                 (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() > 160*160 )
                ClearSchedule( "Crabby follow player" );
        }
    }
    int SelectSchedule()
    {
        if ( m_flForcedSeqEnd ) return SCHED_WAIT_FOR_SCRIPT;
        if ( m_bRiding ) return SCHED_IDLE_STAND;
        if(m_hPuzzle) { SetTarget(m_hPuzzle); return SCHED_TARGET_CHASE; }
        if ( GetEnemy() ) return BaseClass::SelectSchedule();
        CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
        if ( m_bFollowing && pPlayer && pPlayer->IsAlive() &&
             (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() > 120*120 )
        {
            SetTarget( pPlayer );
            return SCHED_TARGET_CHASE;
        }
        return SCHED_IDLE_STAND;
    }
    void StartTask( const Task_t *pTask )
    {
        if ( pTask->iTask == TASK_GET_PATH_TO_TARGET )
        {
            AI_NavGoal_t goal( GOALTYPE_TARGETENT, ACT_RUN, m_hPuzzle ? 12 : 90, AIN_UPDATE_TARGET_POS );
            if ( GetNavigator()->SetGoal( goal ) ) TaskComplete();
            else { TaskFail( FAIL_NO_ROUTE ); if(m_hPuzzle) FinishPuzzle(2); }
            return;
        }
        BaseClass::StartTask( pTask );
    }
    bool BeginPuzzle(CCrabbySwitch *p)
    {
        if (!p || !p->Available() || !p->SwitchEntity() || !IsAlive()) return false;
        if (m_hPuzzle == p) return true;
        if (m_hPuzzle) FinishPuzzle(0);
        if (m_bRiding)
        {
            Vector face = p->GetAbsOrigin();
            EndRide(ToBasePlayer(GetMoveParent()), NULL, &face);
        }
        StopCrabbySequence();
        StopAttacking();
        m_bFollowing = false;
        m_hPuzzle = p;
        m_flPressTime = 0;
        m_flPuzzleDeadline = gpGlobals->curtime + 30;
        SetTarget(p);
        ClearSchedule("Crabby switch search starts");
        EmitSound("NPC_FastHeadcrab.Alert");
        DevMsg("Crabby: searching for switch route\n");
        p->m_OnSearchStarted.FireOutput(this, p);
        return true;
    }
    void FinishPuzzle(int result)
    {
        CCrabbySwitch *p = static_cast<CCrabbySwitch *>(m_hPuzzle.Get());
        m_hPuzzle = NULL;
        m_flPuzzleDeadline = 0;
        m_flPressTime = 0;
        m_bFollowing = true;
        GetNavigator()->ClearGoal();
        if (result != 1) StopCrabbySequence();  // a press lets its rear-up finish
        ClearSchedule("Crabby switch search ends");
        if (!p) return;
        if (result == 1) { p->m_bUsed = true; DevMsg("Crabby: switch pressed\n"); p->m_OnPressed.FireOutput(this,p); }
        else if (result == 2) { DevMsg("Crabby: switch route failed\n"); p->m_OnSearchFailed.FireOutput(this,p); }
        else p->m_OnSearchCancelled.FireOutput(this,p);
    }
    void InputCommandSwitch(inputdata_t &data)
    {
        CBaseEntity *p = gEntList.FindEntityByName(NULL, data.value.String());
        if(p && FClassnameIs(p,"info_crabby_switch")) BeginPuzzle(static_cast<CCrabbySwitch *>(p));
    }
    void InputFollowPlayer( inputdata_t &data ) { if(m_hPuzzle) FinishPuzzle(0); m_bFollowing = true; if (!m_bRiding) StopAttacking(); }
    void InputWait( inputdata_t &data )
    {
        if(m_hPuzzle) FinishPuzzle(0);
        if (m_bRiding) EndRide(ToBasePlayer(GetMoveParent()), NULL);
        m_bFollowing = false; StopAttacking(); GetNavigator()->ClearGoal();
    }
    void InputStopAttacking( inputdata_t &data ) { StopAttacking(); }
    void InputAttackTarget( inputdata_t &data )
    {
        CommandAttack( gEntList.FindEntityByName( NULL, data.value.String(), this, data.pActivator, data.pCaller ) );
    }
    // Scripted ride: Crabby jumps up from wherever it is (no distance limit,
    // works even while AllowRiding is off).
    void InputRideShoulder( inputdata_t &data )
    {
        CBasePlayer *pPlayer = ToBasePlayer( data.pActivator );
        if ( !pPlayer ) pPlayer = UTIL_GetLocalPlayer();
        bool allowed = m_bAllowRiding;
        m_bAllowRiding = true;
        BeginRide( pPlayer );
        m_bAllowRiding = allowed;
    }
    void InputDismount( inputdata_t &data ) { if ( m_bRiding ) EndRide( ToBasePlayer( GetMoveParent() ), NULL ); }
    void InputEnableRiding( inputdata_t &data ) { m_bAllowRiding = true; }
    void InputDisableRiding( inputdata_t &data )
    {
        m_bAllowRiding = false;
        if ( m_bRiding ) EndRide( ToBasePlayer( GetMoveParent() ), NULL );
    }
    void InputSetCrabbyScale( inputdata_t &data )
    {
        m_flCrabbyScale = clamp( data.value.Float(), 0.3f, 1.5f );
        SetModelScale( m_flCrabbyScale );
        RestoreTinyHull();
    }

    float m_flCrabbyScale;
    bool m_bAllowRiding;
    COutputEvent m_OnRideStart, m_OnRideEnd;
private:
    bool m_bFollowing;
    EHANDLE m_hAttackTarget;
    float m_flNextAssist;
    EHANDLE m_hPuzzle;
    float m_flPuzzleDeadline, m_flPressTime;
    CNetworkVar( bool, m_bRiding );
    float m_flNextRideIdle;
    int m_iForcedSeq;
    float m_flForcedSeqEnd;
};

LINK_ENTITY_TO_CLASS( npc_crabby, CNPC_Crabby );

// The client draws a riding Crabby view-locked; see c_npc_crabby.cpp.
IMPLEMENT_SERVERCLASS_ST( CNPC_Crabby, DT_NPC_Crabby )
    SendPropBool( SENDINFO( m_bRiding ) ),
END_SEND_TABLE()

BEGIN_DATADESC( CNPC_Crabby )
    DEFINE_FIELD( m_bFollowing, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_hAttackTarget, FIELD_EHANDLE ),
    DEFINE_FIELD( m_flNextAssist, FIELD_TIME ),
    DEFINE_FIELD( m_hPuzzle, FIELD_EHANDLE ),
    DEFINE_FIELD( m_flPuzzleDeadline, FIELD_TIME ),
    DEFINE_FIELD( m_flPressTime, FIELD_TIME ),
    DEFINE_FIELD( m_bRiding, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_flNextRideIdle, FIELD_TIME ),
    DEFINE_FIELD( m_iForcedSeq, FIELD_INTEGER ),
    DEFINE_FIELD( m_flForcedSeqEnd, FIELD_TIME ),
    DEFINE_KEYFIELD( m_flCrabbyScale, FIELD_FLOAT, "CrabbyScale" ),
    DEFINE_KEYFIELD( m_bAllowRiding, FIELD_BOOLEAN, "AllowRiding" ),
    DEFINE_INPUTFUNC( FIELD_STRING, "CommandSwitch", InputCommandSwitch ),
    DEFINE_INPUTFUNC( FIELD_VOID, "FollowPlayer", InputFollowPlayer ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Wait", InputWait ),
    DEFINE_INPUTFUNC( FIELD_VOID, "StopAttacking", InputStopAttacking ),
    DEFINE_INPUTFUNC( FIELD_STRING, "AttackTarget", InputAttackTarget ),
    DEFINE_INPUTFUNC( FIELD_VOID, "RideShoulder", InputRideShoulder ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Dismount", InputDismount ),
    DEFINE_INPUTFUNC( FIELD_VOID, "EnableRiding", InputEnableRiding ),
    DEFINE_INPUTFUNC( FIELD_VOID, "DisableRiding", InputDisableRiding ),
    DEFINE_INPUTFUNC( FIELD_FLOAT, "SetCrabbyScale", InputSetCrabbyScale ),
    DEFINE_OUTPUT( m_OnRideStart, "OnRideStart" ),
    DEFINE_OUTPUT( m_OnRideEnd, "OnRideEnd" ),
END_DATADESC()

static CNPC_Crabby *CrabbyRidingOn( CBasePlayer *player )
{
    CBaseEntity *e = NULL;
    while ((e = gEntList.FindEntityByClassname(e,"npc_crabby")) != NULL)
    {
        CNPC_Crabby *c = static_cast<CNPC_Crabby *>(e);
        if (c->IsRiding() && c->GetMoveParent() == player) return c;
    }
    return NULL;
}

// The Crabby the player is looking at (or very nearly at), within pickup reach.
static CNPC_Crabby *CrabbyUnderAim( CBasePlayer *player, const trace_t &aim )
{
    Vector eye = player->EyePosition(), forward;
    player->EyeVectors(&forward);
    if (aim.m_pEnt && FClassnameIs(aim.m_pEnt, "npc_crabby"))
    {
        CNPC_Crabby *c = static_cast<CNPC_Crabby *>(aim.m_pEnt);
        if ((c->WorldSpaceCenter() - eye).Length() <= CRABBY_PICKUP_REACH) return c;
    }
    CNPC_Crabby *best = NULL;
    float bestDot = 0.94f;
    CBaseEntity *e = NULL;
    while ((e = gEntList.FindEntityByClassname(e,"npc_crabby")) != NULL)
    {
        if (!e->IsAlive()) continue;
        Vector dir = e->WorldSpaceCenter() - eye;
        float dist = VectorNormalize(dir);
        if (dist > CRABBY_PICKUP_REACH) continue;
        float dot = DotProduct(dir, forward);
        if (dot <= bestDot) continue;
        trace_t tr;
        UTIL_TraceLine(eye, e->WorldSpaceCenter(), MASK_SOLID_BRUSHONLY, player, COLLISION_GROUP_NONE, &tr);
        if (tr.fraction < 0.98f) continue;
        best = static_cast<CNPC_Crabby *>(e);
        bestDot = dot;
    }
    return best;
}

// Called once on the use-button edge, so short E taps cannot be missed by NPC think.
// Handles, in order: switch commands, setting a riding Crabby down, picking Crabby up.
bool CrabbyTrySwitchCommand(CBasePlayer *player)
{
    if (!player || !player->IsAlive() || player->GetVehicle() || player->GetUseEntity()) return false;
    Vector eye = player->EyePosition(), forward; player->EyeVectors(&forward);
    trace_t tr;
    UTIL_TraceLine(eye, eye+forward*1024, MASK_SHOT, player, COLLISION_GROUP_NONE, &tr);
    CNPC_Crabby *rider = CrabbyRidingOn(player);

    CCrabbySwitch *marker = NULL;
    CBaseEntity *e = NULL;
    if (tr.m_pEnt)
    {
        while ((e = gEntList.FindEntityByClassname(e,"info_crabby_switch")) != NULL)
        {
            CCrabbySwitch *p = static_cast<CCrabbySwitch *>(e);
            if(p->SwitchEntity() == tr.m_pEnt) { marker = p; break; }
        }
    }
    if (marker)
    {
        CNPC_Crabby *nearest = rider;
        if (!nearest)
        {
            float distance = 1024*1024;
            e = NULL;
            while ((e = gEntList.FindEntityByClassname(e,"npc_crabby")) != NULL)
            {
                float d = (e->GetAbsOrigin()-player->GetAbsOrigin()).LengthSqr();
                if(e->IsAlive() && d < distance) { nearest=static_cast<CNPC_Crabby *>(e); distance=d; }
            }
        }
        if(nearest && marker->Available()) nearest->BeginPuzzle(marker);
        else if(!nearest) ClientPrint(player, HUD_PRINTCENTER, "Crabby is too far away.");
        // Opted-in switches cannot be remotely pressed by the normal player-use path.
        return true;
    }

    if (rider)
    {
        // Doors, buttons and pickups keep working while Crabby rides.
        if (player->FindUseEntity()) return false;
        bool floorAim = tr.fraction < 1.0f && tr.plane.normal.z >= 0.7f &&
                        (tr.endpos - eye).Length() <= CRABBY_SETDOWN_REACH;
        Vector spot = tr.endpos + tr.plane.normal * 2.0f;
        rider->EndRide(player, floorAim ? &spot : NULL);
        return true;
    }

    CNPC_Crabby *pick = CrabbyUnderAim(player, tr);
    if (pick && pick->CanStartRide(player)) return pick->BeginRide(player);
    return false;
}
