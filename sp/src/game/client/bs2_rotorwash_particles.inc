// Scene rotorwash: fast ground sheets, rolling perimeter dust, lingering veil and grit.
#include "c_te_effect_dispatch.h"
#include "particles_simple.h"

static ConVar bs2_heli_rotorwash_quality("bs2_heli_rotorwash_quality","1",0,"Scene rotorwash density (0-2).");
struct BS2WashParticle : public SimpleParticle { unsigned char layer; float phase; };
class CBS2Brownout : public CSimpleEmitter
{
public:
 CBS2Brownout(const Vector &centre,const Vector &normal):CSimpleEmitter("bs2_rotorwash_brownout"),m_Centre(centre),m_Normal(normal)
 {SetDynamicallyAllocated(true);SetSortOrigin(centre+Vector(0,0,100));SetNearClip(24,110);SetParticleCullRadius(1800);}
 static CSmartPtr<CBS2Brownout> Create(const Vector &centre,const Vector &normal){return new CBS2Brownout(centre,normal);}
 float GroundZ(const Vector &p) const {return m_Centre.z-(m_Normal.x*(p.x-m_Centre.x)+m_Normal.y*(p.y-m_Centre.y))/MAX(m_Normal.z,.25f);}
 void UpdateVelocity(SimpleParticle *base,float dt) OVERRIDE
 {
  BS2WashParticle *p=static_cast<BS2WashParticle*>(base);const float age=p->m_flLifetime;
  Vector radial=p->m_Pos-m_Centre;radial.z=0;VectorNormalize(radial);Vector tangent(-radial.y,radial.x,0);
  if(p->layer==3)
  {
   p->m_vecVelocity.z-=500*dt;
   if(p->m_Pos.z<GroundZ(p->m_Pos)+2 && p->m_vecVelocity.z<0){p->m_Pos.z=GroundZ(p->m_Pos)+2;p->m_vecVelocity.z*=-.22f;p->m_vecVelocity.x*=.65f;p->m_vecVelocity.y*=.65f;}
   return;
  }
  const float drag=expf(-dt*(p->layer==0?.35f:.21f));p->m_vecVelocity.x*=drag;p->m_vecVelocity.y*=drag;
  if(p->layer==0)
  {
   p->m_vecVelocity+=tangent*(55*sin(age*2+p->phase)*dt);
   p->m_vecVelocity.z+=(14-p->m_vecVelocity.z)*dt*2;
   if(p->m_Pos.z<GroundZ(p->m_Pos)+8)p->m_Pos.z=GroundZ(p->m_Pos)+8;
  }
  else if(p->layer==1)
  {
   p->m_vecVelocity+=tangent*(42*sin(age*1.6f+p->phase)*dt);
   p->m_vecVelocity.z+=(85+45*sin(age*1.8f+p->phase)-p->m_vecVelocity.z)*dt*.85f;
  }
  else {p->m_vecVelocity+=(Vector(26,8,35)-p->m_vecVelocity)*dt*.38f;}
 }
 float UpdateAlpha(const SimpleParticle *base) OVERRIDE
 {
  const BS2WashParticle *p=static_cast<const BS2WashParticle*>(base);
  const float age=clamp(p->m_flLifetime/p->m_flDieTime,0.0f,1.0f);
  const float fadeIn=clamp(p->m_flLifetime/(p->layer==3?.035f:.28f),0.0f,1.0f);
  return (p->m_uchStartAlpha/255.0f)*fadeIn*powf(1-age,p->layer==2?1.15f:1.65f);
 }
private: Vector m_Centre,m_Normal;
};

CLIENTEFFECT_REGISTER_BEGIN( PrecacheBS2Rotorwash )
 CLIENTEFFECT_MATERIAL("bs2/rotorwash/dust_0")
 CLIENTEFFECT_MATERIAL("bs2/rotorwash/dust_1")
 CLIENTEFFECT_MATERIAL("bs2/rotorwash/dust_2")
 CLIENTEFFECT_MATERIAL("bs2/rotorwash/dust_3")
 CLIENTEFFECT_MATERIAL("effects/fleck_cement1")
CLIENTEFFECT_REGISTER_END()

void BS2RotorwashCallback(const CEffectData &data)
{
 const float intensity=clamp(data.m_flScale,0.0f,1.0f);const float quality=clamp(bs2_heli_rotorwash_quality.GetFloat(),0.0f,2.0f);
 if(intensity<.01f || quality<=0)return;
 CSmartPtr<CBS2Brownout> effect=CBS2Brownout::Create(data.m_vOrigin,data.m_vNormal);
 PMaterialHandle materials[4];for(int i=0;i<4;i++)materials[i]=effect->GetPMaterial(VarArgs("bs2/rotorwash/dust_%d",i));
 PMaterialHandle grit=effect->GetPMaterial("effects/fleck_cement1");
 const int counts[4]={24,14,6,9};
 for(int layer=0;layer<4;layer++)
 {
  int count=(int)ceilf(counts[layer]*intensity*quality);
  for(int i=0;i<count;i++)
  {
   float angle=random->RandomFloat(0,2*M_PI);Vector radial(cosf(angle),sinf(angle),0);Vector tangent(-radial.y,radial.x,0);
   float radius=random->RandomFloat(layer==0?60:260,layer==0?330:520);
   Vector pos=data.m_vOrigin+radial*radius;pos.z=effect->GroundZ(pos)+random->RandomFloat(10,layer==2?130:35);
   BS2WashParticle *p=(BS2WashParticle*)effect->AddParticle(sizeof(BS2WashParticle),layer==3?grit:materials[random->RandomInt(0,3)],pos);if(!p)break;
   p->m_flLifetime=0;p->m_flRoll=random->RandomFloat(0,360);p->m_flRollDelta=random->RandomFloat(-.55f,.55f);p->m_iFlags=0;p->layer=layer;p->phase=random->RandomFloat(0,2*M_PI);
   const float tint=random->RandomFloat(.82f,1.08f);p->m_uchColor[0]=(uint8)(151*tint);p->m_uchColor[1]=(uint8)(127*tint);p->m_uchColor[2]=(uint8)(94*tint);p->m_uchEndAlpha=0;
   if(layer==0){p->m_flDieTime=random->RandomFloat(1.7f,2.5f);p->m_vecVelocity=radial*random->RandomFloat(230,420)+tangent*random->RandomFloat(-75,75)+Vector(0,0,14);p->m_uchStartSize=random->RandomInt(18,35);p->m_uchEndSize=random->RandomInt(90,150);p->m_uchStartAlpha=(uint8)(random->RandomInt(75,115)*intensity);}
   else if(layer==1){p->m_flDieTime=random->RandomFloat(2.8f,4.1f);p->m_vecVelocity=radial*random->RandomFloat(115,210)+tangent*random->RandomFloat(-45,45)+Vector(0,0,random->RandomFloat(85,155));p->m_uchStartSize=random->RandomInt(35,60);p->m_uchEndSize=random->RandomInt(170,245);p->m_uchStartAlpha=(uint8)(random->RandomInt(55,85)*intensity);}
   else if(layer==2){p->m_flDieTime=random->RandomFloat(4.5f,6);p->m_vecVelocity=radial*random->RandomFloat(30,70)+Vector(20,5,random->RandomFloat(35,65));p->m_uchStartSize=random->RandomInt(45,70);p->m_uchEndSize=random->RandomInt(210,255);p->m_uchStartAlpha=(uint8)(random->RandomInt(22,38)*intensity);}
   else{p->m_flDieTime=random->RandomFloat(.7f,1.4f);p->m_vecVelocity=radial*random->RandomFloat(240,440)+Vector(0,0,random->RandomFloat(80,200));p->m_uchStartSize=random->RandomInt(1,3);p->m_uchEndSize=1;p->m_uchStartAlpha=200;}
  }
 }
}
DECLARE_CLIENT_EFFECT("BS2Rotorwash",BS2RotorwashCallback);