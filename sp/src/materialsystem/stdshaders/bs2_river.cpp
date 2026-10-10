// BS2 river: a single static surface, with UV-directed flow and cubemap reflection.
#include "BaseVSShader.h"
#include "tier0/memdbgon.h"
BEGIN_VS_SHADER( BS2_River, "BS2 curved river surface (static meshes, SM3)" )
 BEGIN_SHADER_PARAMS
  SHADER_PARAM(NORMALMAP,SHADER_PARAM_TYPE_TEXTURE,"","Tileable flow normal")
  SHADER_PARAM(RAINMAP,SHADER_PARAM_TYPE_TEXTURE,"","Animated rain normal")
  SHADER_PARAM(RAINFRAME,SHADER_PARAM_TYPE_INTEGER,"0","")
  SHADER_PARAM(ENVMAP,SHADER_PARAM_TYPE_TEXTURE,"env_cubemap","Baked reflection")
  SHADER_PARAM(WATERTINT,SHADER_PARAM_TYPE_VEC3,"[0.045 0.14 0.12]","Linear water color")
  SHADER_PARAM(FLOWSPEED,SHADER_PARAM_TYPE_FLOAT,"0.10","UV units per second")
  SHADER_PARAM(NORMALSTRENGTH,SHADER_PARAM_TYPE_FLOAT,"0.35","")
  SHADER_PARAM(RAINSTRENGTH,SHADER_PARAM_TYPE_FLOAT,"0.18","")
  SHADER_PARAM(REFLECTION,SHADER_PARAM_TYPE_FLOAT,"0.85","")
  SHADER_PARAM(REFRACTION,SHADER_PARAM_TYPE_FLOAT,"0.008","Screen UV distortion")
  SHADER_PARAM(TRANSMISSION,SHADER_PARAM_TYPE_FLOAT,"0.55","Background visibility")
  SHADER_PARAM(FOAM,SHADER_PARAM_TYPE_FLOAT,"0.6","")
  SHADER_PARAM(SUNDIRECTION,SHADER_PARAM_TYPE_VEC3,"[-0.4 0.3 0.85]","World direction TO sun")
  SHADER_PARAM(SUNCOLOR,SHADER_PARAM_TYPE_VEC3,"[1.0 0.85 0.64]","")
  SHADER_PARAM(SPECULAR,SHADER_PARAM_TYPE_FLOAT,"0.6","")
 END_SHADER_PARAMS
 SHADER_INIT_PARAMS()
 {
  SET_FLAGS(MATERIAL_VAR_TRANSLUCENT);
  SET_FLAGS2(MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE);
 }
 SHADER_FALLBACK { if(!g_pHardwareConfig->SupportsShaderModel_3_0()) return "UnlitGeneric"; return 0; }
 SHADER_INIT
 {
  LoadTexture(BASETEXTURE, TEXTUREFLAGS_SRGB);
  LoadBumpMap(NORMALMAP); LoadBumpMap(RAINMAP);
  if(params[ENVMAP]->IsDefined()) LoadCubeMap(ENVMAP, TEXTUREFLAGS_SRGB);
 }
 SHADER_DRAW
 {
  SHADOW_STATE
  {
   SetInitialShadowState();
   pShaderShadow->EnableDepthWrites(true);
   pShaderShadow->EnableBlending(false);
   pShaderShadow->EnableAlphaWrites(false);
   pShaderShadow->EnableSRGBWrite(true);
   pShaderShadow->VertexShaderVertexFormat(VERTEX_POSITION,1,NULL,0);
   for(int i=0;i<5;++i) pShaderShadow->EnableTexture((Sampler_t)i,true);
   pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0,true);
   pShaderShadow->EnableSRGBRead(SHADER_SAMPLER2,true);
   pShaderShadow->EnableSRGBRead(SHADER_SAMPLER3,true);
   pShaderShadow->SetVertexShader("bs2_river_vs30",0);
   pShaderShadow->SetPixelShader("bs2_river_ps30",0);
   DefaultFog();
  }
  DYNAMIC_STATE
  {
   pShaderAPI->SetDefaultState();
   BindTexture(SHADER_SAMPLER0,BASETEXTURE,FRAME);
   BindTexture(SHADER_SAMPLER1,NORMALMAP);
   if(params[ENVMAP]->IsTexture()) BindTexture(SHADER_SAMPLER2,ENVMAP);
   else pShaderAPI->BindStandardTexture(SHADER_SAMPLER2,TEXTURE_BLACK);
   pShaderAPI->BindStandardTexture(SHADER_SAMPLER3,TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0);
   BindTexture(SHADER_SAMPLER4,RAINMAP,RAINFRAME);
   float eye[4]={0,0,0,0}; pShaderAPI->GetWorldSpaceCameraPosition(eye);
   pShaderAPI->SetPixelShaderConstant(0,eye);
   float tint[4]={0,0,0,0}; params[WATERTINT]->GetVecValue(tint,3); tint[3]=params[TRANSMISSION]->GetFloatValue();
   pShaderAPI->SetPixelShaderConstant(1,tint);
   float flow[4]={(float)pShaderAPI->CurrentTime(),params[FLOWSPEED]->GetFloatValue(),params[NORMALSTRENGTH]->GetFloatValue(),params[RAINSTRENGTH]->GetFloatValue()};
   pShaderAPI->SetPixelShaderConstant(2,flow);
   float optics[4]={params[REFLECTION]->GetFloatValue(),params[REFRACTION]->GetFloatValue(),params[FOAM]->GetFloatValue(),params[SPECULAR]->GetFloatValue()};
   pShaderAPI->SetPixelShaderConstant(3,optics);
   float sun[4]={0,0,1,0}; params[SUNDIRECTION]->GetVecValue(sun,3); pShaderAPI->SetPixelShaderConstant(4,sun);
   float light[4]={1,1,1,0}; params[SUNCOLOR]->GetVecValue(light,3); pShaderAPI->SetPixelShaderConstant(5,light);
   pShaderAPI->SetVertexShaderIndex(0); pShaderAPI->SetPixelShaderIndex(0);
  }
  Draw();
 }
END_SHADER
