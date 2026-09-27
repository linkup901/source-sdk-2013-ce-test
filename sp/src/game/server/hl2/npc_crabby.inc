// Included by npc_headcrab.cpp to reuse the fast headcrab's movement and attacks.
class CNPC_Crabby : public CFastHeadcrab
{
public:
    DECLARE_CLASS( CNPC_Crabby, CFastHeadcrab );
    DECLARE_DATADESC();
    void Precache() { PrecacheModel( "models/crabby/crabby.mdl" ); BaseClass::Precache(); }
    void Spawn()
    {
        BaseClass::Spawn();
        SetModel( "models/crabby/crabby.mdl" );
        m_iHealth = m_iMaxHealth = 100;
        m_bFollowing = true;
        m_flNextAssist = 0;
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
        if ( info.GetAttacker() && info.GetAttacker()->IsPlayer() ) return 0;
        return BaseClass::OnTakeDamage_Alive( info );
    }
    void CommandAttack( CBaseEntity *pTarget )
    {
        if ( !pTarget || !pTarget->IsNPC() || !pTarget->IsAlive() ||
             pTarget->Classify() == CLASS_PLAYER_ALLY ||
             pTarget->Classify() == CLASS_PLAYER_ALLY_VITAL || pTarget == this ) return;
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
    void PrescheduleThink()
    {
        BaseClass::PrescheduleThink();
        CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
        if ( !pPlayer || !pPlayer->IsAlive() ) return;
        if ( m_hAttackTarget && ( !m_hAttackTarget->IsAlive() ||
             (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() > 1024*1024 ) ) StopAttacking();
        if ( m_bFollowing && gpGlobals->curtime >= m_flNextAssist )
        {
            m_flNextAssist = gpGlobals->curtime + 0.15f;
            if ( pPlayer->m_nButtons & IN_ATTACK )
            {
                Vector forward;
                pPlayer->EyeVectors( &forward );
                trace_t tr;
                UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + forward * 2048,
                                MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
                if ( tr.m_pEnt && tr.m_pEnt != m_hAttackTarget.Get() &&
                     BaseClass::IRelationType( tr.m_pEnt ) == D_HT ) CommandAttack( tr.m_pEnt );
            }
            if ( !GetEnemy() && !IsCurSchedule( SCHED_TARGET_CHASE ) &&
                 (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() > 160*160 )
                ClearSchedule( "Crabby follow player" );
        }
    }
    int SelectSchedule()
    {
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
    int TranslateSchedule( int schedule )
    {
        if ( schedule == SCHED_IDLE_STAND ) return SCHED_IDLE_STAND;
        return BaseClass::TranslateSchedule( schedule );
    }
    void StartTask( const Task_t *pTask )
    {
        if ( pTask->iTask == TASK_GET_PATH_TO_TARGET )
        {
            AI_NavGoal_t goal( GOALTYPE_TARGETENT, ACT_RUN, 90, AIN_UPDATE_TARGET_POS );
            if ( GetNavigator()->SetGoal( goal ) ) TaskComplete();
            else TaskFail( FAIL_NO_ROUTE );
            return;
        }
        BaseClass::StartTask( pTask );
    }
    void InputFollowPlayer( inputdata_t &data ) { m_bFollowing = true; StopAttacking(); }
    void InputWait( inputdata_t &data ) { m_bFollowing = false; StopAttacking(); GetNavigator()->ClearGoal(); }
    void InputStopAttacking( inputdata_t &data ) { StopAttacking(); }
    void InputAttackTarget( inputdata_t &data )
    {
        CommandAttack( gEntList.FindEntityByName( NULL, data.value.String(), this, data.pActivator, data.pCaller ) );
    }
private:
    bool m_bFollowing;
    EHANDLE m_hAttackTarget;
    float m_flNextAssist;
};

LINK_ENTITY_TO_CLASS( npc_crabby, CNPC_Crabby );
BEGIN_DATADESC( CNPC_Crabby )
    DEFINE_FIELD( m_bFollowing, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_hAttackTarget, FIELD_EHANDLE ),
    DEFINE_FIELD( m_flNextAssist, FIELD_TIME ),
    DEFINE_INPUTFUNC( FIELD_VOID, "FollowPlayer", InputFollowPlayer ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Wait", InputWait ),
    DEFINE_INPUTFUNC( FIELD_VOID, "StopAttacking", InputStopAttacking ),
    DEFINE_INPUTFUNC( FIELD_STRING, "AttackTarget", InputAttackTarget ),
END_DATADESC()

