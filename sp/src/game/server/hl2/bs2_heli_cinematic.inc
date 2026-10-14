// BS2 cinematic helicopter nodes. Included after CNPC_AttackHelicopter's declaration.
// Position uses cubic Hermite interpolation; rotation deliberately keeps full turns.
class CBS2HeliNode : public CPointEntity
{
public:
	DECLARE_CLASS( CBS2HeliNode, CPointEntity );
	DECLARE_DATADESC();
	float m_flDuration, m_flPitch, m_flYaw, m_flRoll;
	Vector m_vecVelocity;
	string_t m_iszNext;
};
LINK_ENTITY_TO_CLASS( bs2_heli_node, CBS2HeliNode );
BEGIN_DATADESC( CBS2HeliNode )
	DEFINE_KEYFIELD( m_flDuration, FIELD_FLOAT, "duration" ),
	DEFINE_KEYFIELD( m_flPitch, FIELD_FLOAT, "pitch" ),
	DEFINE_KEYFIELD( m_flYaw, FIELD_FLOAT, "yaw" ),
	DEFINE_KEYFIELD( m_flRoll, FIELD_FLOAT, "roll" ),
	DEFINE_KEYFIELD( m_vecVelocity, FIELD_VECTOR, "velocity" ),
	DEFINE_KEYFIELD( m_iszNext, FIELD_STRING, "nextnode" ),
END_DATADESC()

void CNPC_AttackHelicopter::InputStartCinematic( inputdata_t &data )
{
	if ( m_bCinematic || !IsAlive() ) return;
	CBS2HeliNode *node = dynamic_cast<CBS2HeliNode *>(gEntList.FindEntityByName(NULL, data.value.String()));
	if ( !node || node->m_flDuration < 0.05f )
	{
		Warning("npc_helicopter: invalid cinematic start node\n");
		m_OnCinematicAborted.FireOutput(data.pActivator, this);
		return;
	}
	m_nCinematicMoveType = GetMoveType();
	m_vecCinematicStart = GetAbsOrigin();
	m_vecCinematicVelocity = GetAbsVelocity();
	m_angCinematicStart = GetAbsAngles();
	m_flCinematicStart = gpGlobals->curtime;
	m_hCinematicNode = node;
	m_bCinematic = true;
	SetMoveType(MOVETYPE_NONE);
	SetAbsVelocity(vec3_origin);
	SetLocalAngularVelocity(vec3_angle);
	SetThink(&CBaseHelicopter::HelicopterThink);
	SetNextThink(gpGlobals->curtime);
}

void CNPC_AttackHelicopter::InputResumeFlight( inputdata_t &data )
{
	if ( !m_bCinematic ) return;
	m_bCinematic = false;
	m_hCinematicNode = NULL;
	SetMoveType((MoveType_t)m_nCinematicMoveType);
	SetAbsVelocity(vec3_origin);
	SetLocalAngularVelocity(vec3_angle);
	SetNextThink(gpGlobals->curtime);
}

void CNPC_AttackHelicopter::HelicopterThink( void )
{
	if ( !m_bCinematic ) { BaseClass::HelicopterThink(); return; }
	SetNextThink(gpGlobals->curtime + gpGlobals->interval_per_tick);
	StudioFrameAdvance();
	DispatchAnimEvents(this);
	CBS2HeliNode *node = dynamic_cast<CBS2HeliNode *>(m_hCinematicNode.Get());
	if ( node )
	{
		const float duration = MAX(node->m_flDuration, 0.05f);
		const float t = clamp((gpGlobals->curtime-m_flCinematicStart)/duration, 0.0f, 1.0f);
		const float t2=t*t, t3=t2*t;
		const Vector pos = (2*t3-3*t2+1)*m_vecCinematicStart
			+ (t3-2*t2+t)*duration*m_vecCinematicVelocity
			+ (-2*t3+3*t2)*node->GetAbsOrigin()
			+ (t3-t2)*duration*node->m_vecVelocity;
		// Conservative fixed hull: author and inspect clearance for wings and rotor too.
		trace_t tr;
		UTIL_TraceHull(GetAbsOrigin(), pos, Vector(-48,-48,-32), Vector(48,48,32),
			MASK_NPCSOLID, this, COLLISION_GROUP_NPC, &tr);
		if ( tr.startsolid || tr.fraction < 1.0f )
		{
			m_hCinematicNode = NULL;
			m_OnCinematicAborted.FireOutput(this, this);
			return;
		}
		SetAbsOrigin(pos);
		const float ease = t2*(3-2*t);
		const QAngle end(node->m_flPitch, node->m_flYaw, node->m_flRoll);
		SetAbsAngles(m_angCinematicStart + (end-m_angCinematicStart)*ease);
		if ( t >= 1.0f )
		{
			m_vecCinematicStart = node->GetAbsOrigin();
			m_vecCinematicVelocity = node->m_vecVelocity;
			m_angCinematicStart = end;
			m_flCinematicStart += duration;
			CBS2HeliNode *next = dynamic_cast<CBS2HeliNode *>(gEntList.FindEntityByName(NULL, node->m_iszNext));
			m_hCinematicNode = next;
			if ( node->m_iszNext != NULL_STRING && (!next || next->m_flDuration < 0.05f) )
			{
				m_hCinematicNode = NULL;
				m_OnCinematicAborted.FireOutput(this, this);
			}
			else if ( !next ) m_OnCinematicLanded.FireOutput(this, this);
		}
	}
#ifdef HL2_EPISODIC
	m_BoneFollowerManager.UpdateBoneFollowers(this);
#endif
	UpdatePlayerDopplerShift();
}

// Rigid camera: no look-at solver to remove the aircraft's roll.
class CBS2HeliCamera : public CBaseEntity
{
public:
	DECLARE_CLASS( CBS2HeliCamera, CBaseEntity );
	DECLARE_DATADESC();
	CHandle<CBasePlayer> m_hPlayer;
	EHANDLE m_hOldView;
	bool m_bWasFrozen;
	virtual int UpdateTransmitState() { return SetTransmitState(FL_EDICT_ALWAYS); }
	void InputEnable(inputdata_t &data)
	{
		if ( m_hPlayer ) return;
		CBasePlayer *player = ToBasePlayer(data.pActivator);
		if ( !player ) player = UTIL_GetLocalPlayer();
		if ( !player ) return;
		m_hPlayer = player;
		m_hOldView = player->GetViewEntity();
		m_bWasFrozen = (player->GetFlags() & FL_FROZEN) != 0;
		player->AddFlag(FL_FROZEN);
		player->SetViewEntity(this);
	}
	void RestoreView()
	{
		if ( !m_hPlayer ) return;
		if ( m_hPlayer->GetViewEntity() == this )
			m_hPlayer->SetViewEntity(m_hOldView ? m_hOldView.Get() : m_hPlayer.Get());
		if ( !m_bWasFrozen ) m_hPlayer->RemoveFlag(FL_FROZEN);
		m_hPlayer = NULL;
	}
	void InputDisable(inputdata_t &data) { RestoreView(); }
	virtual void UpdateOnRemove() { RestoreView(); BaseClass::UpdateOnRemove(); }
};
LINK_ENTITY_TO_CLASS( bs2_heli_camera, CBS2HeliCamera );
BEGIN_DATADESC( CBS2HeliCamera )
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hOldView, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bWasFrozen, FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()
