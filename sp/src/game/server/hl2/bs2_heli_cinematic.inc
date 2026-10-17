// BS2 cinematic helicopter nodes. Included after CNPC_AttackHelicopter's declaration.
// Position uses cubic Hermite interpolation; rotation deliberately keeps full turns.
class CBS2HeliNode : public CPointEntity
{
public:
	DECLARE_CLASS( CBS2HeliNode, CPointEntity );
	DECLARE_DATADESC();
	float m_flDuration, m_flPitch, m_flYaw, m_flRoll, m_flHoverAmplitude;
	Vector m_vecVelocity;
	string_t m_iszNext;
	COutputEvent m_OnReached;
};
LINK_ENTITY_TO_CLASS( bs2_heli_node, CBS2HeliNode );
BEGIN_DATADESC( CBS2HeliNode )
	DEFINE_KEYFIELD( m_flDuration, FIELD_FLOAT, "duration" ),
	DEFINE_KEYFIELD( m_flHoverAmplitude, FIELD_FLOAT, "hoveramplitude" ),
	DEFINE_KEYFIELD( m_flPitch, FIELD_FLOAT, "pitch" ),
	DEFINE_KEYFIELD( m_flYaw, FIELD_FLOAT, "yaw" ),
	DEFINE_KEYFIELD( m_flRoll, FIELD_FLOAT, "roll" ),
	DEFINE_KEYFIELD( m_vecVelocity, FIELD_VECTOR, "velocity" ),
	DEFINE_KEYFIELD( m_iszNext, FIELD_STRING, "nextnode" ),
	DEFINE_OUTPUT( m_OnReached, "OnReached" ),
END_DATADESC()

void CNPC_AttackHelicopter::InputDeployCinematicGun(inputdata_t &data)
{
	int deploy=LookupSequence("bs2_gun_deploy");
	if(deploy<0)
	{
		// Preserve the established input on maps using the stock helicopter model.
		Vector gunBase; CBasePlayer *player=UTIL_GetLocalPlayer();
		if(player && GetAttachment(m_nGunBaseAttachment,gunBase)) { Vector aim=player->EyePosition()-gunBase; VectorNormalize(aim); PoseGunTowardTargetDirection(aim); }
		EmitSound("NPC_AttackHelicopter.ChargeGun"); return;
	}
	if(IsValidLayer(m_iCinematicGunLayer)) FastRemoveLayer(m_iCinematicGunLayer);
	m_iCinematicGunLayer=AddLayeredSequence(deploy,10);
	m_nCinematicGunStage=1; m_flCinematicGunStart=gpGlobals->curtime;
	EmitSound("NPC_AttackHelicopter.ChargeGun");
	DevMsg("CINEMATIC_TURRET_DEPLOY time=%.3f sequence=%d\n",gpGlobals->curtime,deploy);
}

void CNPC_AttackHelicopter::InputCinematicBurstAt(inputdata_t &data)
{
	m_hCinematicFireTarget = gEntList.FindEntityByName(NULL, data.value.String());
	m_flCinematicFireEnd = gpGlobals->curtime + 1.6f;
	m_flCinematicNextShot = gpGlobals->curtime;
}

void CNPC_AttackHelicopter::InputCinematicMegaBomb(inputdata_t &data)
{
	Vector position; GetAttachment(m_nBombAttachment,position);
	position.z-=48;
	Vector velocity=m_vecCinematicVelocity*.45f;
	CGrenadeHelicopter *bomb=SpawnBombEntity(position,velocity);
	if(!bomb) return;
	bomb->AddSpawnFlags(SF_GRENADE_HELICOPTER_MEGABOMB);
	bomb->SetName(AllocPooledString("scene_cinematic_megabomb"));
	bomb->SetExplodeOnContact(true);
	EmitSound("NPC_AttackHelicopter.DropMine");
	EmitSound("NPC_AttackHelicopter.MegabombAlert");
	DevMsg("CINEMATIC_MEGA_BOMB time=%.3f entity=%d origin=%.1f %.1f %.1f velocity=%.1f %.1f %.1f\n",gpGlobals->curtime,bomb->entindex(),position.x,position.y,position.z,velocity.x,velocity.y,velocity.z);
}

void CNPC_AttackHelicopter::InputStartCinematic( inputdata_t &data )
{
	DevMsg("BS2 cinematic request: %s alive=%d active=%d\n", data.value.String(), IsAlive(), m_bCinematic);
	if ( !IsAlive() ) return;
	CBS2HeliNode *node = dynamic_cast<CBS2HeliNode *>(gEntList.FindEntityByName(NULL, data.value.String()));
	if ( !node || node->m_flDuration < 0.05f )
	{
		Warning("npc_helicopter: invalid cinematic start node\n");
		m_OnCinematicAborted.FireOutput(data.pActivator, this);
		return;
	}
	if(!m_bCinematic && m_iszCinematicModel != NULL_STRING && m_nCinematicGunStage==0)
	{
		int stowed=LookupSequence("bs2_gun_stowed");
		if(stowed>=0) { m_iCinematicGunLayer=AddLayeredSequence(stowed,10); m_nCinematicGunStage=-1; }
	}
	if(!m_bCinematic) m_nCinematicMoveType = GetMoveType();
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
	if(m_nCinematicGunStage==1 && gpGlobals->curtime-m_flCinematicGunStart>=2.4f)
	{
		if(IsValidLayer(m_iCinematicGunLayer)) FastRemoveLayer(m_iCinematicGunLayer);
		int scan=LookupSequence("bs2_gun_scan");
		m_iCinematicGunLayer=scan>=0 ? AddLayeredSequence(scan,10) : -1;
		m_nCinematicGunStage=2;
		DevMsg("CINEMATIC_TURRET_SCAN time=%.3f sequence=%d\n",gpGlobals->curtime,scan);
	}
	if(m_nCinematicGunStage==2 && gpGlobals->curtime-m_flCinematicGunStart>=8.2f)
	{
		if(IsValidLayer(m_iCinematicGunLayer)) RemoveLayer(m_iCinematicGunLayer,4.0f);
		m_iCinematicGunLayer=-1; m_nCinematicGunStage=3;
		DevMsg("CINEMATIC_TURRET_TRACK time=%.3f\n",gpGlobals->curtime);
	}
	if(m_nCinematicGunStage==3 && UTIL_GetLocalPlayer())
	{
		Vector gunBase;
		if(GetAttachment(m_nGunBaseAttachment,gunBase)) { Vector aim=UTIL_GetLocalPlayer()->EyePosition()-gunBase; VectorNormalize(aim); PoseGunTowardTargetDirection(aim); }
	}
	if (IsSequenceFinished()) { int idle=LookupSequence("idle"); if(idle>=0 && GetSequence()!=idle) ResetSequence(idle); }
	CBS2HeliNode *node = dynamic_cast<CBS2HeliNode *>(m_hCinematicNode.Get());
	if ( node )
	{
		const float duration = MAX(node->m_flDuration, 0.05f);
		const float t = clamp((gpGlobals->curtime-m_flCinematicStart)/duration, 0.0f, 1.0f);
		const float t2=t*t, t3=t2*t;
		Vector pos = (2*t3-3*t2+1)*m_vecCinematicStart
			+ (t3-2*t2+t)*duration*m_vecCinematicVelocity
			+ (-2*t3+3*t2)*node->GetAbsOrigin()
			+ (t3-t2)*duration*node->m_vecVelocity;
		const float hoverEnvelope = sin(t*M_PI)*sin(t*M_PI);
		const float hoverPhase = (gpGlobals->curtime-m_flCinematicStart)*1.45f;
		if(node->m_flHoverAmplitude > 0)
			pos += Vector(0, node->m_flHoverAmplitude*sin(hoverPhase), node->m_flHoverAmplitude*0.22f*sin(hoverPhase*1.3f))*hoverEnvelope;
		// Conservative fixed hull: author and inspect clearance for wings and rotor too.
		trace_t tr;
		UTIL_TraceHull(GetAbsOrigin(), pos, Vector(-48,-48,-32), Vector(48,48,32),
			MASK_NPCSOLID, this, COLLISION_GROUP_NPC, &tr);
		if ( tr.startsolid || tr.fraction < 1.0f )
		{
			DevMsg("BS2 cinematic obstruction node=%s position=%.1f %.1f %.1f hit=%s\n",node->GetEntityName().ToCStr(),tr.endpos.x,tr.endpos.y,tr.endpos.z,tr.m_pEnt ? tr.m_pEnt->GetClassname() : "world");
			m_hCinematicNode = NULL;
			m_OnCinematicAborted.FireOutput(this, this);
			return;
		}
		SetAbsOrigin(pos);
		const float ease = t2*(3-2*t);
		const QAngle end(node->m_flPitch, node->m_flYaw, node->m_flRoll);
		SetAbsAngles(m_angCinematicStart + (end-m_angCinematicStart)*ease + QAngle(0,0,node->m_flHoverAmplitude*0.06f*sin(hoverPhase)*hoverEnvelope));
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
			node->m_OnReached.FireOutput(this, this);
		}
	}
#ifdef HL2_EPISODIC
	m_BoneFollowerManager.UpdateBoneFollowers(this);
#endif
	UpdatePlayerDopplerShift();
	if (m_hCinematicFireTarget && gpGlobals->curtime < m_flCinematicFireEnd && gpGlobals->curtime >= m_flCinematicNextShot)
	{
		Vector muzzle, base;
		GetAttachment(m_nGunTipAttachment, muzzle);
		GetAttachment(m_nGunBaseAttachment, base);
		Vector direction = m_hCinematicFireTarget->WorldSpaceCenter()-base;
		VectorNormalize(direction);
		PoseGunTowardTargetDirection(direction);
		GetAttachment(m_nGunTipAttachment, muzzle);
		FireBulletsInfo_t shot;
		shot.m_iShots=1; shot.m_vecSrc=muzzle; shot.m_vecDirShooting=direction;
		shot.m_vecSpread=Vector(0.025f,0.025f,0.025f); shot.m_flDistance=8192;
		shot.m_iAmmoType=m_iAmmoType; shot.m_iTracerFreq=1; shot.m_flDamage=5;
		FireBullets(shot); DoMuzzleFlash(); EmitSound("NPC_AttackHelicopter.FireGun");
		m_flCinematicNextShot=gpGlobals->curtime+0.08f;
	}
}

// Rigid camera: no look-at solver to remove the aircraft's roll.
class CBS2HeliCamera : public CBaseEntity
{
public:
	DECLARE_CLASS( CBS2HeliCamera, CBaseEntity );
	DECLARE_DATADESC();
	CHandle<CBasePlayer> m_hPlayer;
	string_t m_iszLookEntity, m_iszLookAttachment;
	EHANDLE m_hOldView;
	bool m_bWasFrozen;
	bool m_bStartFromPlayer;
	float m_flPlayerViewStart;
	QAngle m_angPlayerViewStart;
	EHANDLE m_hFrozenVehicle;
	bool m_bVehicleWasHeld;
	void InputHoldBuggy(inputdata_t &data)
	{
		if(m_hFrozenVehicle) return;
		CBasePlayer *player=UTIL_GetLocalPlayer();
		CBaseEntity *vehicle=player ? player->GetVehicleEntity() : NULL;
		CPropVehicleDriveable *driveable=dynamic_cast<CPropVehicleDriveable*>(vehicle);
		if(!driveable) return;
		m_hFrozenVehicle=vehicle; m_bVehicleWasHeld=driveable->IsCinematicHeld();
		variant_t value;
		vehicle->AcceptInput("BeginCinematicHold",this,this,value,0);
		vehicle->AcceptInput("HandBrakeOn",this,this,value,0);
	}
	float m_flShakeAmplitude, m_flShakeDuration, m_flShakeFrequency;
	float m_flShakeStart, m_flShakeEnd;
	Vector m_vecShakeBase;
	QAngle m_angShakeBase;
	bool m_bShaking;
	bool m_bMoving;
	EHANDLE m_hMoveTarget;
	Vector m_vecMoveStart;
	QAngle m_angMoveStart;
	float m_flMoveStart, m_flMoveDuration, m_flZoomDuration;
	void InputEnterBuggy(inputdata_t &data)
	{
		CBaseEntity *vehicle=gEntList.FindEntityByName(NULL,data.value.String());
		CBasePlayer *player=UTIL_GetLocalPlayer();
		if(vehicle && player && vehicle->GetServerVehicle() && !player->IsInAVehicle())
		{
			if(player->GetInVehicle(vehicle->GetServerVehicle(),0))
			{
				CBaseAnimating *model=vehicle->GetBaseAnimating();
				int eyes=model ? model->LookupAttachment("vehicle_driver_eyes") : 0;
				Vector position; QAngle angles;
				if(eyes && model->GetAttachmentLocal(eyes,position,angles)) player->SnapEyeAngles(angles);
			}
		}
	}
	void InputZoom(inputdata_t &data)
	{
		if(m_hPlayer) m_hPlayer->SetFOV(this, clamp(data.value.Int(),20,110),MAX(m_flZoomDuration,0.0f));
	}
	void InputMoveTo(inputdata_t &data)
	{
		CBaseEntity *target=gEntList.FindEntityByName(NULL,data.value.String());
		if(!target || target->GetMoveParent()!=GetMoveParent()) { Warning("BS2 camera MoveTo needs a target with the same parent\n"); return; }
		StopShake();
		m_vecMoveStart=GetLocalOrigin(); m_angMoveStart=GetLocalAngles();
		m_vecShakeBase=m_vecMoveStart; m_angShakeBase=m_angMoveStart;
		m_hMoveTarget=target; m_flMoveStart=gpGlobals->curtime; m_bMoving=true;
		SetThink(&CBS2HeliCamera::ShakeThink); SetNextThink(gpGlobals->curtime);
	}
	void StopShake()
	{
		if ( !m_bShaking ) return;
		SetLocalOrigin(m_vecShakeBase);
		SetLocalAngles(m_angShakeBase);
		m_bShaking = false;
		if(!m_bMoving) SetThink(NULL);
	}
	void InputStartShake(inputdata_t &data)
	{
		StopShake();
		m_vecShakeBase = GetLocalOrigin();
		m_angShakeBase = GetLocalAngles();
		m_flShakeStart = gpGlobals->curtime;
		m_flShakeEnd = m_flShakeStart + MAX(m_flShakeDuration, 0.05f);
		m_bShaking = true;
		SetThink(&CBS2HeliCamera::ShakeThink);
		SetNextThink(gpGlobals->curtime);
	}
	void ShakeThink()
	{
		if(m_bMoving)
		{
			if(!m_hMoveTarget) m_bMoving=false;
			else
			{
				float t=clamp((gpGlobals->curtime-m_flMoveStart)/MAX(m_flMoveDuration,0.05f),0.0f,1.0f);
				float ease=t*t*(3-2*t);
				m_vecShakeBase=m_vecMoveStart+(m_hMoveTarget->GetLocalOrigin()-m_vecMoveStart)*ease;
				m_angShakeBase=m_angMoveStart+(m_hMoveTarget->GetLocalAngles()-m_angMoveStart)*ease;
				SetLocalOrigin(m_vecShakeBase); SetLocalAngles(m_angShakeBase);
				if(t>=1) m_bMoving=false;
			}
		}
		if (m_bShaking && gpGlobals->curtime >= m_flShakeEnd) StopShake();
		if(m_hPlayer && m_iszLookEntity != NULL_STRING)
		{
			CBaseAnimating *target = dynamic_cast<CBaseAnimating *>(gEntList.FindEntityByName(NULL,m_iszLookEntity));
			Vector aim; QAngle angles;
			int attachment = target ? target->LookupAttachment(STRING(m_iszLookAttachment)) : 0;
			if(attachment && target->GetAttachment(attachment,aim))
			{
				VectorAngles(aim-GetAbsOrigin(),angles);
				if(m_bStartFromPlayer)
				{
					float t=clamp((gpGlobals->curtime-m_flPlayerViewStart)/MAX(m_flZoomDuration,0.05f),0.0f,1.0f);
					float ease=t*t*(3-2*t);
					for(int i=0;i<3;++i) angles[i]=m_angPlayerViewStart[i]+AngleNormalize(angles[i]-m_angPlayerViewStart[i])*ease;
				}
				SetAbsAngles(angles);
			}
		}
		if(!m_bShaking) { if(m_bMoving || (m_hPlayer && m_iszLookEntity != NULL_STRING)) SetNextThink(gpGlobals->curtime+gpGlobals->interval_per_tick); else SetThink(NULL); return; }
		const float elapsed = gpGlobals->curtime-m_flShakeStart;
		const float progress = elapsed/(m_flShakeEnd-m_flShakeStart);
		const float envelope = MIN(elapsed/0.05f, 1.0f)*(1-progress)*(1-progress);
		const float phase = elapsed*MAX(m_flShakeFrequency, 0.1f)*2*M_PI;
		const float a = MAX(m_flShakeAmplitude, 0.0f)*envelope;
		// Unequal frequencies give an irregular jolt; the envelope settles to the original mount.
		SetLocalAngles(m_angShakeBase + QAngle(a*sin(phase), a*0.55f*sin(phase*1.37f), a*0.7f*sin(phase*0.83f)));
		SetLocalOrigin(m_vecShakeBase + Vector(0, a*0.5f*sin(phase*1.19f), a*0.6f*sin(phase*1.61f)));
		SetNextThink(gpGlobals->curtime + gpGlobals->interval_per_tick);
	}
	virtual int UpdateTransmitState() { return SetTransmitState(FL_EDICT_ALWAYS); }
	void InputEnable(inputdata_t &data)
	{
		DevMsg("BS2 helicopter camera Enable\n");
		if ( m_hPlayer ) return;
		CBasePlayer *player = ToBasePlayer(data.pActivator);
		if ( !player ) player = UTIL_GetLocalPlayer();
		if ( !player ) return;
		if(m_bStartFromPlayer)
		{
			Vector position=player->EyePosition(); QAngle angles=player->EyeAngles();
			if(player->GetVehicle()) player->GetVehicle()->GetVehicleViewPosition(VEHICLE_ROLE_DRIVER,&position,&angles);
			SetAbsOrigin(position); SetAbsAngles(angles);
			m_angPlayerViewStart=angles; m_flPlayerViewStart=gpGlobals->curtime;
			DevMsg("CINEMATIC_CAMERA_PLAYER_VIEW time=%.3f origin=%.3f %.3f %.3f angles=%.3f %.3f %.3f\n",gpGlobals->curtime,position.x,position.y,position.z,angles.x,angles.y,angles.z);
		}
		m_hPlayer = player;
		InputHoldBuggy(data);
		m_hOldView = player->GetViewEntity();
		m_bWasFrozen = (player->GetFlags() & FL_FROZEN) != 0;
		player->AddFlag(FL_FROZEN);
		player->SetViewEntity(this);
		if(m_iszLookEntity != NULL_STRING) { SetThink(&CBS2HeliCamera::ShakeThink); SetNextThink(gpGlobals->curtime); }
		DevMsg("BS2 helicopter camera player=%d origin=%.1f %.1f %.1f\n", player->entindex(), GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z);
	}
	void RestoreView()
	{
		m_bMoving=false;
		StopShake();
		SetThink(NULL);
		if(m_hFrozenVehicle)
		{
			variant_t value;
			if(!m_bVehicleWasHeld) m_hFrozenVehicle->AcceptInput("EndCinematicHold",this,this,value,0);
			m_hFrozenVehicle->AcceptInput("HandBrakeOff",this,this,value,0);
			m_hFrozenVehicle=NULL;
		}
		if ( !m_hPlayer ) return;
		if ( m_hPlayer->GetViewEntity() == this )
			m_hPlayer->SetViewEntity(m_hOldView ? m_hOldView.Get() : m_hPlayer.Get());
		if ( !m_bWasFrozen ) m_hPlayer->RemoveFlag(FL_FROZEN);
		m_hPlayer->SetFOV(this,0,0);
		m_hPlayer = NULL;
	}
	void InputDisable(inputdata_t &data) { RestoreView(); }
	virtual void UpdateOnRemove() { RestoreView(); BaseClass::UpdateOnRemove(); }
};
LINK_ENTITY_TO_CLASS( bs2_heli_camera, CBS2HeliCamera );
BEGIN_DATADESC( CBS2HeliCamera )
	DEFINE_KEYFIELD(m_bStartFromPlayer,FIELD_BOOLEAN,"startfromplayer"),
	DEFINE_FIELD(m_flPlayerViewStart,FIELD_TIME),
	DEFINE_FIELD(m_angPlayerViewStart,FIELD_VECTOR),
	DEFINE_KEYFIELD(m_iszLookEntity,FIELD_STRING,"lookentity"),
	DEFINE_KEYFIELD(m_iszLookAttachment,FIELD_STRING,"lookattachment"),
	DEFINE_FIELD(m_hFrozenVehicle,FIELD_EHANDLE),
	DEFINE_FIELD(m_bVehicleWasHeld,FIELD_BOOLEAN),
	DEFINE_INPUTFUNC(FIELD_VOID,"HoldBuggy",InputHoldBuggy),
	DEFINE_FIELD(m_bMoving,FIELD_BOOLEAN),
	DEFINE_FIELD(m_hMoveTarget,FIELD_EHANDLE),
	DEFINE_FIELD(m_vecMoveStart,FIELD_VECTOR),
	DEFINE_FIELD(m_angMoveStart,FIELD_VECTOR),
	DEFINE_FIELD(m_flMoveStart,FIELD_TIME),
	DEFINE_KEYFIELD(m_flMoveDuration,FIELD_FLOAT,"moveduration"),
	DEFINE_KEYFIELD(m_flZoomDuration,FIELD_FLOAT,"zoomduration"),
	DEFINE_INPUTFUNC(FIELD_STRING,"MoveTo",InputMoveTo),
	DEFINE_INPUTFUNC(FIELD_INTEGER,"Zoom",InputZoom),
	DEFINE_INPUTFUNC(FIELD_STRING,"EnterBuggy",InputEnterBuggy),
	DEFINE_KEYFIELD( m_flShakeAmplitude, FIELD_FLOAT, "shakeamplitude" ),
	DEFINE_KEYFIELD( m_flShakeDuration, FIELD_FLOAT, "shakeduration" ),
	DEFINE_KEYFIELD( m_flShakeFrequency, FIELD_FLOAT, "shakefrequency" ),
	DEFINE_FIELD( m_flShakeStart, FIELD_TIME ),
	DEFINE_FIELD( m_flShakeEnd, FIELD_TIME ),
	DEFINE_FIELD( m_vecShakeBase, FIELD_VECTOR ),
	DEFINE_FIELD( m_angShakeBase, FIELD_VECTOR ),
	DEFINE_FIELD( m_bShaking, FIELD_BOOLEAN ),
	DEFINE_THINKFUNC( ShakeThink ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StartShake", InputStartShake ),
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hOldView, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bWasFrozen, FIELD_BOOLEAN ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()

class CBS2HeliLamp : public CPointEntity
{
public:
	DECLARE_CLASS(CBS2HeliLamp,CPointEntity); DECLARE_DATADESC();
	string_t m_iszLight,m_iszBeam,m_iszTarget,m_iszBlindRelay;
	EHANDLE m_hLight,m_hBeam,m_hTarget;
	float m_flStart; Vector m_vecStartForward; bool m_bBlinded;
	void InputEnable(inputdata_t &data)
	{
		m_hLight=gEntList.FindEntityByName(NULL,m_iszLight);m_hBeam=gEntList.FindEntityByName(NULL,m_iszBeam);m_hTarget=gEntList.FindEntityByName(NULL,m_iszTarget);
		m_flStart=gpGlobals->curtime;m_bBlinded=false;
		if(m_hLight) AngleVectors(m_hLight->GetAbsAngles(),&m_vecStartForward);
		else m_vecStartForward.Init(0,0,-1);
		SetThink(&CBS2HeliLamp::LampThink);SetNextThink(gpGlobals->curtime);
	}
	void InputDisable(inputdata_t &data) {SetThink(NULL);}
	void LampThink()
	{
		if(!m_hLight || !m_hTarget) {SetThink(NULL);return;}
		CBasePlayer *player=UTIL_GetLocalPlayer();
		Vector goal=m_hTarget->WorldSpaceCenter()+Vector(0,0,40);
		if(player && player->GetVehicleEntity()==m_hTarget.Get()) goal=player->EyePosition();
		Vector direction=goal-m_hLight->GetAbsOrigin();VectorNormalize(direction);
		float t=clamp((gpGlobals->curtime-m_flStart)/.35f,0.0f,1.0f);t=t*t*(3-2*t);
		Vector forward=m_vecStartForward*(1-t)+direction*t;VectorNormalize(forward);
		QAngle angles;VectorAngles(forward,angles);m_hLight->SetAbsAngles(angles);if(m_hBeam)m_hBeam->SetAbsAngles(angles);
		if(!m_bBlinded && player && DotProduct(forward,direction)>.992f)
		{
			trace_t tr;UTIL_TraceLine(m_hLight->GetAbsOrigin()+forward*12,player->EyePosition(),MASK_OPAQUE,GetMoveParent(),COLLISION_GROUP_NONE,&tr);
			if(tr.fraction>.98f || tr.m_pEnt==m_hTarget.Get() || tr.m_pEnt==player)
			{
				m_bBlinded=true;CBaseEntity *relay=gEntList.FindEntityByName(NULL,m_iszBlindRelay);variant_t value;
				if(relay)relay->AcceptInput("Trigger",player,this,value,0);
				DevMsg("HELI_BEAM_IN_PLAYER_FACE time=%.3f dot=%.3f\n",gpGlobals->curtime,DotProduct(forward,direction));
			}
		}
		SetNextThink(gpGlobals->curtime+gpGlobals->interval_per_tick);
	}
};
LINK_ENTITY_TO_CLASS(bs2_heli_lamp,CBS2HeliLamp);
BEGIN_DATADESC(CBS2HeliLamp)
 DEFINE_KEYFIELD(m_iszLight,FIELD_STRING,"lightentity"), DEFINE_KEYFIELD(m_iszBeam,FIELD_STRING,"beamentity"),
 DEFINE_KEYFIELD(m_iszTarget,FIELD_STRING,"targetentity"), DEFINE_KEYFIELD(m_iszBlindRelay,FIELD_STRING,"blindrelay"),
 DEFINE_FIELD(m_hLight,FIELD_EHANDLE), DEFINE_FIELD(m_hBeam,FIELD_EHANDLE), DEFINE_FIELD(m_hTarget,FIELD_EHANDLE),
 DEFINE_FIELD(m_flStart,FIELD_TIME), DEFINE_FIELD(m_vecStartForward,FIELD_VECTOR), DEFINE_FIELD(m_bBlinded,FIELD_BOOLEAN),
 DEFINE_INPUTFUNC(FIELD_VOID,"Enable",InputEnable), DEFINE_INPUTFUNC(FIELD_VOID,"Disable",InputDisable),DEFINE_THINKFUNC(LampThink),
END_DATADESC()

class CBS2Rotorwash : public CPointEntity
{
public:
 DECLARE_CLASS(CBS2Rotorwash,CPointEntity); DECLARE_DATADESC();
 string_t m_iszHelicopter;EHANDLE m_hHelicopter;float m_flStart;
 void InputStart(inputdata_t &data) {m_hHelicopter=gEntList.FindEntityByName(NULL,m_iszHelicopter);m_flStart=gpGlobals->curtime;SetThink(&CBS2Rotorwash::WashThink);SetNextThink(gpGlobals->curtime);}
 void InputStop(inputdata_t &data) {SetThink(NULL);}
 void WashThink()
 {
  if(!m_hHelicopter){SetThink(NULL);return;}
  Vector top=m_hHelicopter->GetAbsOrigin();trace_t tr;UTIL_TraceLine(top,top-Vector(0,0,1600),MASK_SOLID_BRUSHONLY,m_hHelicopter.Get(),COLLISION_GROUP_NONE,&tr);
  if(tr.fraction<1 && !tr.startsolid)
  {
   float height=top.z-tr.endpos.z;
   float strength=clamp((900-height)/650.0f,0.0f,1.0f)*clamp((gpGlobals->curtime-m_flStart)/.6f,0.0f,1.0f);
   if(strength>.02f){CEffectData effect;effect.m_vOrigin=tr.endpos+tr.plane.normal*8;effect.m_vNormal=tr.plane.normal;effect.m_vStart=top;effect.m_flScale=strength;effect.m_flMagnitude=340;DispatchEffect("BS2Rotorwash",effect);}
  }
  SetNextThink(gpGlobals->curtime+.14f);
 }
};
LINK_ENTITY_TO_CLASS(bs2_rotorwash,CBS2Rotorwash);
BEGIN_DATADESC(CBS2Rotorwash)
 DEFINE_KEYFIELD(m_iszHelicopter,FIELD_STRING,"helicopter"), DEFINE_FIELD(m_hHelicopter,FIELD_EHANDLE),DEFINE_FIELD(m_flStart,FIELD_TIME),
 DEFINE_INPUTFUNC(FIELD_VOID,"Start",InputStart),DEFINE_INPUTFUNC(FIELD_VOID,"Stop",InputStop),DEFINE_THINKFUNC(WashThink),
END_DATADESC()
