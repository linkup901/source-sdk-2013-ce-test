#include "cbase.h"
#include "tier0/memdbgon.h"

class CEnvBSSoftFog : public CPointEntity
{
 DECLARE_CLASS(CEnvBSSoftFog,CPointEntity);
 DECLARE_DATADESC();
 DECLARE_SERVERCLASS();
public:
 CEnvBSSoftFog() { m_Size=Vector(2048,2048,512); m_Color=Vector(105,125,145); m_Density=.0006f; m_Edge=256; m_Layers=48; m_Noise=.15f; m_Speed=4; m_Enabled=true; AddEFlags(EFL_FORCE_CHECK_TRANSMIT); }
 int UpdateTransmitState() { return SetTransmitState(FL_EDICT_ALWAYS); }
 void Spawn() { BaseClass::Spawn(); Precache(); for(int i=0;i<3;++i)m_Size.GetForModify()[i]=MAX(m_Size.Get()[i],16.f); m_Density=clamp(m_Density.Get(),0.f,.02f); m_Edge=MAX(m_Edge.Get(),1.f); m_Layers=clamp(m_Layers.Get(),16,64); }
 void Precache() { PrecacheMaterial("bs2/fog/volume"); }
 void InputEnable(inputdata_t &) { m_Enabled=true; }
 void InputDisable(inputdata_t &) { m_Enabled=false; }
 void InputDensity(inputdata_t &d) { m_Density=clamp(d.value.Float(),0.f,.02f); }
private:
 CNetworkVector(m_Size); CNetworkVector(m_Color);
 CNetworkVar(float,m_Density); CNetworkVar(float,m_Edge);
 CNetworkVar(int,m_Layers); CNetworkVar(float,m_Noise); CNetworkVar(float,m_Speed); CNetworkVar(bool,m_Enabled);
};
LINK_ENTITY_TO_CLASS(env_bs_soft_fog,CEnvBSSoftFog);
BEGIN_DATADESC(CEnvBSSoftFog)
 DEFINE_KEYFIELD(m_Size,FIELD_VECTOR,"size"),
 DEFINE_KEYFIELD(m_Color,FIELD_VECTOR,"fogcolor"),
 DEFINE_KEYFIELD(m_Density,FIELD_FLOAT,"density"),
 DEFINE_KEYFIELD(m_Edge,FIELD_FLOAT,"edgesoftness"),
 DEFINE_KEYFIELD(m_Layers,FIELD_INTEGER,"layers"),
 DEFINE_KEYFIELD(m_Noise,FIELD_FLOAT,"noiseamount"),
 DEFINE_KEYFIELD(m_Speed,FIELD_FLOAT,"driftspeed"),
 DEFINE_KEYFIELD(m_Enabled,FIELD_BOOLEAN,"enabled"),
 DEFINE_INPUTFUNC(FIELD_VOID,"Enable",InputEnable),
 DEFINE_INPUTFUNC(FIELD_VOID,"Disable",InputDisable),
 DEFINE_INPUTFUNC(FIELD_FLOAT,"SetDensity",InputDensity),
END_DATADESC()
IMPLEMENT_SERVERCLASS_ST(CEnvBSSoftFog,DT_BSSoftFog)
 SendPropVector(SENDINFO(m_Size),0,SPROP_NOSCALE),
 SendPropVector(SENDINFO(m_Color),0,SPROP_NOSCALE),
 SendPropFloat(SENDINFO(m_Density),0,SPROP_NOSCALE),
 SendPropFloat(SENDINFO(m_Edge),0,SPROP_NOSCALE),
 SendPropInt(SENDINFO(m_Layers),8,SPROP_UNSIGNED),
 SendPropFloat(SENDINFO(m_Noise),0,SPROP_NOSCALE),
 SendPropFloat(SENDINFO(m_Speed),0,SPROP_NOSCALE),
 SendPropBool(SENDINFO(m_Enabled)),
END_SEND_TABLE()
