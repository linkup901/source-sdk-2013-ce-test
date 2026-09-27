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
        if (m_hPuzzle) return;
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
        if (m_flPuzzleDeadline && !m_hPuzzle) FinishPuzzle(2);
        CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
        if ( !pPlayer || !pPlayer->IsAlive() ) { if(m_hPuzzle) FinishPuzzle(0); return; }
        if (m_hPuzzle)
        {
            CCrabbySwitch *p = static_cast<CCrabbySwitch *>(m_hPuzzle.Get());
            CBaseEntity *button = p->SwitchEntity();
            if (!p->Available() || !button || gpGlobals->curtime > m_flPuzzleDeadline)
            { FinishPuzzle(2); return; }
            Vector delta = p->GetAbsOrigin() - GetAbsOrigin();
            if (delta.Length2D() <= 28 && fabs(delta.z) <= 32 &&
                (button->WorldSpaceCenter() - WorldSpaceCenter()).Length() <= 96)
            {
                trace_t tr;
                UTIL_TraceLine(WorldSpaceCenter(), button->WorldSpaceCenter(), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
                if (tr.fraction == 1 || tr.m_pEnt == button)
                {
                    GetNavigator()->ClearGoal();
                    if (!m_flPressTime) m_flPressTime = gpGlobals->curtime + 0.6f;
                    if (gpGlobals->curtime >= m_flPressTime) FinishPuzzle(1);
                    return;
                }
            }
            m_flPressTime = 0;
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
                Vector forward;
                pPlayer->EyeVectors( &forward );
                trace_t tr;
                UTIL_TraceLine( pPlayer->Weapon_ShootPosition(), pPlayer->Weapon_ShootPosition() + forward * 2048,
                                MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
                if ( tr.m_pEnt && tr.m_pEnt != m_hAttackTarget.Get() )
                {
                    // Ordinary player allies fear zombies. Fear still denotes an enemy
                    // that Crabby should help the player fight when commanded by aim.
                    Disposition_t relation = BaseClass::IRelationType( tr.m_pEnt );
                    if ( relation == D_HT || relation == D_FR ) CommandAttack( tr.m_pEnt );
                }
            }
            if ( !GetEnemy() && !IsCurSchedule( SCHED_TARGET_CHASE ) &&
                 (GetAbsOrigin() - pPlayer->GetAbsOrigin()).LengthSqr() > 160*160 )
                ClearSchedule( "Crabby follow player" );
        }
    }
    int SelectSchedule()
    {
        if(m_hPuzzle) { SetTarget(m_hPuzzle); return m_flPressTime ? SCHED_IDLE_STAND : SCHED_TARGET_CHASE; }
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
    void InputFollowPlayer( inputdata_t &data ) { if(m_hPuzzle) FinishPuzzle(0); m_bFollowing = true; StopAttacking(); }
    void InputWait( inputdata_t &data ) { if(m_hPuzzle) FinishPuzzle(0); m_bFollowing = false; StopAttacking(); GetNavigator()->ClearGoal(); }
    void InputStopAttacking( inputdata_t &data ) { StopAttacking(); }
    void InputAttackTarget( inputdata_t &data )
    {
        CommandAttack( gEntList.FindEntityByName( NULL, data.value.String(), this, data.pActivator, data.pCaller ) );
    }
private:
    bool m_bFollowing;
    EHANDLE m_hAttackTarget;
    float m_flNextAssist;
    EHANDLE m_hPuzzle;
    float m_flPuzzleDeadline, m_flPressTime;
};

LINK_ENTITY_TO_CLASS( npc_crabby, CNPC_Crabby );
BEGIN_DATADESC( CNPC_Crabby )
    DEFINE_FIELD( m_bFollowing, FIELD_BOOLEAN ),
    DEFINE_FIELD( m_hAttackTarget, FIELD_EHANDLE ),
    DEFINE_FIELD( m_flNextAssist, FIELD_TIME ),
    DEFINE_FIELD( m_hPuzzle, FIELD_EHANDLE ),
    DEFINE_FIELD( m_flPuzzleDeadline, FIELD_TIME ),
    DEFINE_FIELD( m_flPressTime, FIELD_TIME ),
    DEFINE_INPUTFUNC( FIELD_STRING, "CommandSwitch", InputCommandSwitch ),
    DEFINE_INPUTFUNC( FIELD_VOID, "FollowPlayer", InputFollowPlayer ),
    DEFINE_INPUTFUNC( FIELD_VOID, "Wait", InputWait ),
    DEFINE_INPUTFUNC( FIELD_VOID, "StopAttacking", InputStopAttacking ),
    DEFINE_INPUTFUNC( FIELD_STRING, "AttackTarget", InputAttackTarget ),
END_DATADESC()

// Called once on the use-button edge, so short E taps cannot be missed by NPC think.
bool CrabbyTrySwitchCommand(CBasePlayer *player)
{
    if (!player || !player->IsAlive() || player->GetVehicle() || player->GetUseEntity()) return false;
    Vector forward; player->EyeVectors(&forward);
    trace_t tr;
    UTIL_TraceLine(player->EyePosition(), player->EyePosition()+forward*1024, MASK_SHOT, player, COLLISION_GROUP_NONE, &tr);
    if (!tr.m_pEnt) return false;
    CCrabbySwitch *marker = NULL;
    CBaseEntity *e = NULL;
    while ((e = gEntList.FindEntityByClassname(e,"info_crabby_switch")) != NULL)
    {
        CCrabbySwitch *p = static_cast<CCrabbySwitch *>(e);
        if(p->SwitchEntity() == tr.m_pEnt) { marker = p; break; }
    }
    if (!marker) return false;
    CNPC_Crabby *nearest = NULL;
    float distance = 1024*1024;
    e = NULL;
    while ((e = gEntList.FindEntityByClassname(e,"npc_crabby")) != NULL)
    {
        float d = (e->GetAbsOrigin()-player->GetAbsOrigin()).LengthSqr();
        if(e->IsAlive() && d < distance) { nearest=static_cast<CNPC_Crabby *>(e); distance=d; }
    }
    if(nearest && marker->Available()) nearest->BeginPuzzle(marker);
    else if(!nearest) ClientPrint(player, HUD_PRINTCENTER, "Crabby is too far away.");
    // Opted-in switches cannot be remotely pressed by the normal player-use path.
    return true;
}

