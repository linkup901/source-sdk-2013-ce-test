#include "cbase.h"
#include "view.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imesh.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/memdbgon.h"

// Explicit, unshadowed test source matching the example lamp. Not BSP light discovery.
static ConVar bs2_fog_scatter("bs2_fog_scatter","0",FCVAR_CHEAT);
static ConVar bs2_fog_light_x("bs2_fog_light_x","203.8",FCVAR_CHEAT);
static ConVar bs2_fog_light_y("bs2_fog_light_y","50",FCVAR_CHEAT);
static ConVar bs2_fog_light_z("bs2_fog_light_z","505.5",FCVAR_CHEAT);
static ConVar bs2_fog_light_radius("bs2_fog_light_radius","420",FCVAR_CHEAT);
static ConVar bs2_fog_anisotropy("bs2_fog_anisotropy","0.15",FCVAR_CHEAT);

// Depth-tested view-aligned volume integration. No scene-depth texture or
// opaque box faces; the camera may enter the volume. 16-bit vertex alpha is
// unavailable in this API, so the default density/layer count avoids tiny
// alpha values. This approximates extinction, not multiple light scattering.
class C_EnvBSSoftFog : public C_BaseEntity
{
 DECLARE_CLASS(C_EnvBSSoftFog,C_BaseEntity);
 DECLARE_CLIENTCLASS();
public:
 bool ShouldDraw() { return m_Enabled && m_Density>0; }
 bool IsTransparent() { return true; }
 bool IsTwoPass() { return false; }
 ShadowType_t ShadowCastType() { return SHADOWS_NONE; }
 void GetRenderBounds(Vector &mins,Vector &maxs) { mins=-m_Size*.5f; maxs=m_Size*.5f; }
 void OnDataChanged(DataUpdateType_t t) { BaseClass::OnDataChanged(t); if(t==DATA_UPDATE_CREATED) { SetRenderMode(kRenderTransColor); AddToLeafSystem(RENDER_GROUP_TRANSLUCENT_ENTITY); } }
 int DrawModel(int flags)
 {
  if(!m_Enabled || m_Density<=0) return 0;
  IMaterial *mat=materials->FindMaterial("bs2/fog/volume",TEXTURE_GROUP_CLIENT_EFFECTS);
  if(!mat || mat->IsErrorMaterial()) return 0;
  const Vector c=GetAbsOrigin(), half=m_Size*.5f;
  const Vector f=CurrentViewForward(), r=CurrentViewRight(), u=CurrentViewUp(), eye=CurrentViewOrigin();
  float rf=0,rr=0,ru=0;
  for(int k=0;k<3;++k){rf+=fabsf(f[k])*half[k];rr+=fabsf(r[k])*half[k];ru+=fabsf(u[k])*half[k];}
  float centerDepth=DotProduct(c-eye,f);
  float lo=MAX(-rf,2.f-centerDepth),hi=rf;
  if(lo>=hi)return 0;
  int layers=clamp(m_Layers,16,64);const int grid=32;
  float step=(hi-lo)/layers;
  // Thin slices previously rounded faint edge alpha to zero at 8-bit precision.
  // Encode a larger vertex alpha and compensate with floating material alpha.
  // Bound the scale using the largest possible density and ray correction.
  float maxAlpha=1-expf(-m_Density*step*5.f*1.5f);
  float alphaScale=clamp(.9f/MAX(maxAlpha,.0001f),1.f,8.f);
  mat->AlphaModulate(1.f/alphaScale);
  CMatRenderContextPtr ctx(materials);ctx->Bind(mat);
  ctx->MatrixMode(MATERIAL_MODEL);ctx->PushMatrix();ctx->LoadIdentity();
  IMesh *mesh=ctx->GetDynamicMesh(true,NULL,NULL,mat);
  for(int s=layers-1;s>=0;--s)
  {
   Vector pc=c+f*(lo+(s+.5f)*step);
   CMeshBuilder mb;mb.Begin(mesh,MATERIAL_TRIANGLES,grid*grid*2);
   for(int j=0;j<grid;++j)for(int i=0;i<grid;++i)
   {
    const int dx[6]={0,1,1,0,1,0},dy[6]={0,0,1,0,1,1};
    for(int v=0;v<6;++v)
    {
     Vector p=pc+r*((float(i+dx[v])/grid*2-1)*rr)+u*((float(j+dy[v])/grid*2-1)*ru);
     Vector local=p-c;float fade=1;
     // Multiplication gives soft corners as well as faces. Limit the softness
     // per axis so shallow ground mist retains a useful center density.
     for(int k=0;k<3;++k){float width=MIN(MAX(m_Edge,1.f),half[k]);float t=clamp((half[k]-fabsf(local[k]))/width,0.f,1.f);fade*=t*t*(3-2*t);}
     float height=clamp((local.z+half.z)/MAX(m_Size.z,1.f),0.f,1.f);
     float drift=gpGlobals->curtime*m_Speed;
     float noise=.5f*sinf((p.x+drift)/270.f+p.z/180.f)+.5f*sinf((p.y-drift*.6f)/340.f-p.z/210.f);
     float density=m_Density*fade*(1-.65f*height)*(1+clamp(m_Noise,0.f,.5f)*noise);
     Vector ray=p-eye;float len=ray.Length();float cosine=len>1?MAX(DotProduct(ray,f)/len,.2f):1.f;
     float alpha=1-expf(-MAX(density,0.f)*step/cosine);
     Vector color=m_Color;
     if(bs2_fog_scatter.GetFloat()>0 && fade>0)
     {
      Vector toLight=Vector(bs2_fog_light_x.GetFloat(),bs2_fog_light_y.GetFloat(),bs2_fog_light_z.GetFloat())-p;
      float distance=VectorNormalize(toLight);
      Vector toEye=eye-p;VectorNormalize(toEye);
      float g=clamp(bs2_fog_anisotropy.GetFloat(),0.f,.7f);
      float mu=DotProduct(-toLight,toEye);
      float phase=(1-g*g)/powf(MAX(1+g*g-2*g*mu,.01f),1.5f);
      float falloff=clamp(1-distance/MAX(bs2_fog_light_radius.GetFloat(),1.f),0.f,1.f);
      // Smooth, bounded response avoids a saturated orange core at high settings.
      float weight=1-expf(-MAX(bs2_fog_scatter.GetFloat(),0.f)*falloff*falloff*phase);
      color=color*(1-weight)+Vector(255,178,88)*weight;
     }
     mb.Position3fv(p.Base());mb.Color4ub((unsigned char)clamp(color.x,0.f,255.f),(unsigned char)clamp(color.y,0.f,255.f),(unsigned char)clamp(color.z,0.f,255.f),(unsigned char)clamp(alpha*alphaScale*255+.5f,0.f,255.f));mb.TexCoord2f(0,0,0);mb.AdvanceVertex();
    }
   }
   mb.End();mesh->Draw();
  }
  ctx->PopMatrix();
  mat->AlphaModulate(1.f);
  return 1;
 }
private:
 Vector m_Size,m_Color;float m_Density,m_Edge;int m_Layers;float m_Noise,m_Speed;bool m_Enabled;
};
IMPLEMENT_CLIENTCLASS_DT(C_EnvBSSoftFog,DT_BSSoftFog,CEnvBSSoftFog)
 RecvPropVector(RECVINFO(m_Size)),RecvPropVector(RECVINFO(m_Color)),
 RecvPropFloat(RECVINFO(m_Density)),RecvPropFloat(RECVINFO(m_Edge)),
 RecvPropInt(RECVINFO(m_Layers)),RecvPropFloat(RECVINFO(m_Noise)),
 RecvPropFloat(RECVINFO(m_Speed)),RecvPropBool(RECVINFO(m_Enabled)),
END_RECV_TABLE()
