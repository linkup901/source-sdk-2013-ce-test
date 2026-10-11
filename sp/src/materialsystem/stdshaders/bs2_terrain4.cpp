#include "BaseVSShader.h"
#include "tier0/memdbgon.h"
BEGIN_VS_SHADER(BS2_Terrain4,"Four-layer lightmapped brush/displacement terrain with world-space RGBA weights")
 BEGIN_SHADER_PARAMS
 SHADER_PARAM(BASETEXTURE2,SHADER_PARAM_TYPE_TEXTURE,"","Loam")
 SHADER_PARAM(BASETEXTURE3,SHADER_PARAM_TYPE_TEXTURE,"","Mud")
 SHADER_PARAM(BASETEXTURE4,SHADER_PARAM_TYPE_TEXTURE,"","Rock")
 SHADER_PARAM(BUMPMAP,SHADER_PARAM_TYPE_TEXTURE,"","Gravel normal")
 SHADER_PARAM(BUMPMAP2,SHADER_PARAM_TYPE_TEXTURE,"","Loam normal")
 SHADER_PARAM(BUMPMAP3,SHADER_PARAM_TYPE_TEXTURE,"","Mud normal")
 SHADER_PARAM(BUMPMAP4,SHADER_PARAM_TYPE_TEXTURE,"","Rock normal")
 SHADER_PARAM(BLENDMASK,SHADER_PARAM_TYPE_TEXTURE,"","Linear RGBA weights, not transparency")
 SHADER_PARAM(MASKBOUNDS,SHADER_PARAM_TYPE_VEC4,"[-512 -512 1024 1024]","World XY minimum and extent")
 SHADER_PARAM(LAYERSCALES,SHADER_PARAM_TYPE_VEC4,"[1 1 1 1]","Tiling multipliers")
 SHADER_PARAM(BLENDCONTRAST,SHADER_PARAM_TYPE_FLOAT,"1","Weight contrast")
 SHADER_PARAM(NORMALSTRENGTH,SHADER_PARAM_TYPE_FLOAT,"1","")
 SHADER_PARAM(DEBUGWEIGHTS,SHADER_PARAM_TYPE_INTEGER,"0","")
 END_SHADER_PARAMS
 SHADER_INIT_PARAMS() {
  SET_FLAGS2(MATERIAL_VAR2_LIGHTING_LIGHTMAP);
  SET_FLAGS2(MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP);
  SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_FLASHLIGHT);
 }
 SHADER_FALLBACK { if(!g_pHardwareConfig->SupportsShaderModel_3_0()) return "LightmappedGeneric"; return 0; }
 SHADER_INIT {
  LoadTexture(BASETEXTURE,TEXTUREFLAGS_SRGB); LoadTexture(BASETEXTURE2,TEXTUREFLAGS_SRGB); LoadTexture(BASETEXTURE3,TEXTUREFLAGS_SRGB); LoadTexture(BASETEXTURE4,TEXTUREFLAGS_SRGB);
  LoadTexture(BLENDMASK); LoadBumpMap(BUMPMAP); LoadBumpMap(BUMPMAP2); LoadBumpMap(BUMPMAP3); LoadBumpMap(BUMPMAP4);
 }
 SHADER_DRAW {
  const bool flashlight = UsingFlashlight(params);
  SHADOW_STATE {
   SetInitialShadowState(); pShaderShadow->EnableSRGBWrite(true);
   pShaderShadow->VertexShaderVertexFormat(VERTEX_POSITION | VERTEX_NORMAL,3,NULL,0);
   for(int i=0;i<10;++i) pShaderShadow->EnableTexture((Sampler_t)i,true);
   for(int i=0;i<4;++i) pShaderShadow->EnableSRGBRead((Sampler_t)i,true);
   pShaderShadow->EnableSRGBRead(SHADER_SAMPLER5,true);
   pShaderShadow->EnableTexture(SHADER_SAMPLER10,true);
   pShaderShadow->EnableTexture(SHADER_SAMPLER11,true);
   pShaderShadow->EnableShadowDepthFiltering(SHADER_SAMPLER11);
   pShaderShadow->SetVertexShader("bs2_terrain4_vs30",0);
   pShaderShadow->SetPixelShader("bs2_terrain4_ps30",0);
   if(flashlight) {
    pShaderShadow->EnableDepthWrites(false);
    pShaderShadow->EnableAlphaWrites(false);
    EnableAlphaBlending(SHADER_BLEND_ONE,SHADER_BLEND_ONE);
    FogToBlack();
   } else DefaultFog();
  }
  DYNAMIC_STATE {
   pShaderAPI->SetDefaultState(); BindTexture(SHADER_SAMPLER0,BASETEXTURE); BindTexture(SHADER_SAMPLER1,BASETEXTURE2); BindTexture(SHADER_SAMPLER2,BASETEXTURE3); BindTexture(SHADER_SAMPLER3,BASETEXTURE4);
   BindTexture(SHADER_SAMPLER4,BLENDMASK); pShaderAPI->BindStandardTexture(SHADER_SAMPLER5,TEXTURE_LIGHTMAP);
   BindTexture(SHADER_SAMPLER6,BUMPMAP); BindTexture(SHADER_SAMPLER7,BUMPMAP2); BindTexture(SHADER_SAMPLER8,BUMPMAP3); BindTexture(SHADER_SAMPLER9,BUMPMAP4);
   float bounds[4]; params[MASKBOUNDS]->GetVecValue(bounds,4); bounds[2]=1.0f/MAX(bounds[2],1.0f); bounds[3]=1.0f/MAX(bounds[3],1.0f); pShaderAPI->SetPixelShaderConstant(0,bounds);
   pShaderAPI->SetPixelShaderConstant(1,params[LAYERSCALES]->GetVecValue());
   float control[4]={params[BLENDCONTRAST]->GetFloatValue(),pShaderAPI->GetLightMapScaleFactor(),(float)params[DEBUGWEIGHTS]->GetIntValue(),params[NORMALSTRENGTH]->GetFloatValue()}; pShaderAPI->SetPixelShaderConstant(2,control);
   pShaderAPI->SetPixelShaderFogParams(3); unsigned char rgb[3]; pShaderAPI->GetSceneFogColor(rgb); float fog[4]={rgb[0]/255.0f,rgb[1]/255.0f,rgb[2]/255.0f,0}; pShaderAPI->SetPixelShaderConstant(4,fog);
   float eye[4]={0,0,0,0}; pShaderAPI->GetWorldSpaceCameraPosition(eye);
   eye[3]=(float)pShaderAPI->GetPixelFogCombo1(true);
   pShaderAPI->SetPixelShaderConstant(5,eye);
   float mode[4]={flashlight ? 1.0f : 0.0f,0,0,0};
   if(flashlight) {
    VMatrix worldToTexture; ITexture *depth=NULL;
    FlashlightState_t state=pShaderAPI->GetFlashlightStateEx(worldToTexture,&depth);
    BindTexture(SHADER_SAMPLER10,state.m_pSpotlightTexture,state.m_nSpotlightTextureFrame);
    if(depth && g_pConfig->ShadowDepthTexture() && state.m_bEnableShadows) {
     BindTexture(SHADER_SAMPLER11,depth,0); mode[1]=1;
    } else pShaderAPI->BindStandardTexture(SHADER_SAMPLER11,TEXTURE_WHITE);
    pShaderAPI->SetPixelShaderConstant(7,worldToTexture.Base(),4);
    float origin[4]={state.m_vecLightOrigin.x,state.m_vecLightOrigin.y,state.m_vecLightOrigin.z,state.m_FarZ};
    float attenuation[4]={state.m_fConstantAtten,state.m_fLinearAtten,state.m_fQuadraticAtten,0};
    pShaderAPI->SetPixelShaderConstant(11,origin);
    pShaderAPI->SetPixelShaderConstant(12,state.m_Color);
    pShaderAPI->SetPixelShaderConstant(13,attenuation);
   } else {
    pShaderAPI->BindStandardTexture(SHADER_SAMPLER10,TEXTURE_WHITE);
    pShaderAPI->BindStandardTexture(SHADER_SAMPLER11,TEXTURE_WHITE);
   }
   pShaderAPI->SetPixelShaderConstant(6,mode);
   pShaderAPI->SetVertexShaderIndex(0); pShaderAPI->SetPixelShaderIndex(0);
  } Draw();
 }
END_SHADER
