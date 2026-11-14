// BS2_Puddle (Claude, 14 Nov 2026): shallow rain puddles on models (flat decal meshes), built from BS2_Glass.
// Fresnel env_cubemap reflection, procedural rain-drop rings + wind swell (world space), damp dark rim,
// $basetexture alpha = puddle shape. SM3 only; falls back to VertexLitGeneric on older hardware.
#include "BaseVSShader.h"
#include "shaderapi/ishaderapi.h"
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( BS2_Puddle, "BS2 rain puddle (models, SM3)" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "env_cubemap", "Reflection cubemap" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Reflection tint" )
		SHADER_PARAM( FRESNELMIN, SHADER_PARAM_TYPE_FLOAT, "0.02", "Reflectance head-on (water ~0.02)" )
		SHADER_PARAM( FRESNELMAX, SHADER_PARAM_TYPE_FLOAT, "1.0", "Reflectance at grazing angles" )
		SHADER_PARAM( FRESNELPOWER, SHADER_PARAM_TYPE_FLOAT, "5.0", "Schlick exponent" )
		SHADER_PARAM( REFLECTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "1.0", "Overall reflection strength" )
		SHADER_PARAM( SPECEXPONENT, SHADER_PARAM_TYPE_FLOAT, "512", "Highlight sharpness (lamps on the water)" )
		SHADER_PARAM( SPECBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Highlight brightness" )
		SHADER_PARAM( AMBIENTSHEEN, SHADER_PARAM_TYPE_FLOAT, "0.3", "Adds the ambient cube to the reflection" )
		SHADER_PARAM( WATERCOLOR, SHADER_PARAM_TYPE_COLOR, "[0.35 0.37 0.36]", "Water body colour (times the ambient light)" )
		SHADER_PARAM( DARKEN, SHADER_PARAM_TYPE_FLOAT, "0.55", "How much the water hides/darkens the ground (0..1)" )
		SHADER_PARAM( RAININTENSITY, SHADER_PARAM_TYPE_FLOAT, "0.6", "0 = still mirror, 1 = heavy rain (drop count)" )
		SHADER_PARAM( RIPPLESIZE, SHADER_PARAM_TYPE_FLOAT, "18", "World units per drop cell" )
		SHADER_PARAM( RIPPLESPEED, SHADER_PARAM_TYPE_FLOAT, "0.9", "Drop rings per second per cell" )
		SHADER_PARAM( RIPPLESTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.35", "How much the rings bend the reflection" )
		SHADER_PARAM( WINDSTRENGTH, SHADER_PARAM_TYPE_FLOAT, "0.15", "Gentle swell (0 = none)" )
		SHADER_PARAM( EDGEREFLECT, SHADER_PARAM_TYPE_FLOAT, "0.35", "Coverage where the mirror starts (below = damp rim only)" )
		SHADER_PARAM( BASEOPACITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "Scales the $basetexture alpha (puddle size/depth)" )
		SHADER_PARAM( ENVMAPLDR, SHADER_PARAM_TYPE_INTEGER, "-1", "-1 auto, 1 = cubemap is LDR, 0 = HDR" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
		if ( !params[ENVMAP]->IsDefined() ) params[ENVMAP]->SetStringValue( "env_cubemap" );
		if ( !params[ENVMAPTINT]->IsDefined() ) params[ENVMAPTINT]->SetVecValue( 1, 1, 1 );
		if ( !params[WATERCOLOR]->IsDefined() ) params[WATERCOLOR]->SetVecValue( 0.35f, 0.37f, 0.36f );
		if ( !params[FRESNELMIN]->IsDefined() ) params[FRESNELMIN]->SetFloatValue( 0.02f );
		if ( !params[FRESNELMAX]->IsDefined() ) params[FRESNELMAX]->SetFloatValue( 1.0f );
		if ( !params[FRESNELPOWER]->IsDefined() ) params[FRESNELPOWER]->SetFloatValue( 5.0f );
		if ( !params[REFLECTAMOUNT]->IsDefined() ) params[REFLECTAMOUNT]->SetFloatValue( 1.0f );
		if ( !params[SPECEXPONENT]->IsDefined() ) params[SPECEXPONENT]->SetFloatValue( 512.0f );
		if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 1.0f );
		if ( !params[AMBIENTSHEEN]->IsDefined() ) params[AMBIENTSHEEN]->SetFloatValue( 0.3f );
		if ( !params[DARKEN]->IsDefined() ) params[DARKEN]->SetFloatValue( 0.55f );
		if ( !params[RAININTENSITY]->IsDefined() ) params[RAININTENSITY]->SetFloatValue( 0.6f );
		if ( !params[RIPPLESIZE]->IsDefined() ) params[RIPPLESIZE]->SetFloatValue( 18.0f );
		if ( !params[RIPPLESPEED]->IsDefined() ) params[RIPPLESPEED]->SetFloatValue( 0.9f );
		if ( !params[RIPPLESTRENGTH]->IsDefined() ) params[RIPPLESTRENGTH]->SetFloatValue( 0.35f );
		if ( !params[WINDSTRENGTH]->IsDefined() ) params[WINDSTRENGTH]->SetFloatValue( 0.15f );
		if ( !params[EDGEREFLECT]->IsDefined() ) params[EDGEREFLECT]->SetFloatValue( 0.35f );
		if ( !params[BASEOPACITY]->IsDefined() ) params[BASEOPACITY]->SetFloatValue( 1.0f );
		if ( !params[ENVMAPLDR]->IsDefined() ) params[ENVMAPLDR]->SetIntValue( -1 );
	}

	SHADER_FALLBACK
	{
		if ( !g_pHardwareConfig->SupportsShaderModel_3_0() )
			return "VertexLitGeneric";
		return 0;
	}

	SHADER_INIT
	{
		if ( params[BASETEXTURE]->IsDefined() ) LoadTexture( BASETEXTURE, TEXTUREFLAGS_SRGB );
		if ( params[ENVMAP]->IsDefined() )
			LoadCubeMap( ENVMAP, g_pHardwareConfig->GetHDRType() == HDR_TYPE_NONE ? TEXTUREFLAGS_SRGB : 0 );
	}

	bool EnvIsLDR( IMaterialVar **params )
	{
		int forced = params[ENVMAPLDR]->GetIntValue();
		if ( forced >= 0 ) return forced != 0;
		if ( !params[ENVMAP]->IsTexture() ) return true;
		ImageFormat f = params[ENVMAP]->GetTextureValue()->GetImageFormat();
		return !( f == IMAGE_FORMAT_RGBA16161616F || f == IMAGE_FORMAT_RGBA16161616 );
	}

	SHADER_DRAW
	{
		SHADOW_STATE
		{
			SetInitialShadowState();
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			pShaderShadow->EnableBlending( true );
			pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			pShaderShadow->EnablePolyOffset( SHADER_POLYOFFSET_DECAL );   // sits on the ground without z-fighting
			pShaderShadow->EnableSRGBWrite( true );
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			pShaderShadow->VertexShaderVertexFormat( flags, 1, NULL, 0 );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			bool bHDROn = g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bHDROn || EnvIsLDR( params ) );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );
			pShaderShadow->SetVertexShader( "bs2_puddle_vs30", 0 );
			pShaderShadow->SetPixelShader( "bs2_puddle_ps30", 0 );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();
			if ( params[ENVMAP]->IsTexture() ) BindTexture( SHADER_SAMPLER0, ENVMAP );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BLACK );
			bool bBase = params[BASETEXTURE]->IsTexture();
			if ( bBase ) BindTexture( SHADER_SAMPLER2, BASETEXTURE, FRAME );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER2, TEXTURE_WHITE );
			LightState_t lightState = { 0, false, false };
			pShaderAPI->GetDX9LightState( &lightState );
			pShaderAPI->SetPixelShaderStateAmbientLightCube( 4 );
			pShaderAPI->CommitPixelShaderLighting( 20 );

			float t = pShaderAPI->CurrentTime();
			float eye[4] = { 0, 0, 0, 0 }; pShaderAPI->GetWorldSpaceCameraPosition( eye );
			pShaderAPI->SetPixelShaderConstant( 0, eye );
			float fres[4] = { params[FRESNELMIN]->GetFloatValue(), params[FRESNELMAX]->GetFloatValue(),
				params[FRESNELPOWER]->GetFloatValue(), params[REFLECTAMOUNT]->GetFloatValue() };
			pShaderAPI->SetPixelShaderConstant( 1, fres );
			float spec[4] = { params[SPECEXPONENT]->GetFloatValue(), params[SPECBOOST]->GetFloatValue(),
				params[AMBIENTSHEEN]->GetFloatValue(), (float)lightState.m_nNumLights };
			pShaderAPI->SetPixelShaderConstant( 2, spec );
			float water[4] = { 0.35f, 0.37f, 0.36f, 0 }; params[WATERCOLOR]->GetVecValue( water, 3 ); water[3] = params[DARKEN]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 3, water );
			float env[4] = { 1, 1, 1, 1 }; params[ENVMAPTINT]->GetVecValue( env, 3 ); env[3] = EnvIsLDR( params ) ? 0.0f : 1.0f;
			pShaderAPI->SetPixelShaderConstant( 10, env );
			float size = params[RIPPLESIZE]->GetFloatValue(); if ( size < 1.0f ) size = 1.0f;
			float rain[4] = { params[RAININTENSITY]->GetFloatValue(), 1.0f / size, fmodf( t * params[RIPPLESPEED]->GetFloatValue(), 1000.0f ), params[RIPPLESTRENGTH]->GetFloatValue() };
			pShaderAPI->SetPixelShaderConstant( 11, rain );
			float misc[4] = { params[BASEOPACITY]->GetFloatValue(), params[EDGEREFLECT]->GetFloatValue(), params[WINDSTRENGTH]->GetFloatValue(), bBase ? 1.0f : 0.0f };
			pShaderAPI->SetPixelShaderConstant( 13, misc );
			float misc2[4] = { fmodf( t, 6283.0f ), 0, 0, 0 };
			pShaderAPI->SetPixelShaderConstant( 14, misc2 );

			int skin = pShaderAPI->GetCurrentNumBones() > 0 ? 1 : 0;
			pShaderAPI->SetVertexShaderIndex( (int)vertexCompression + 2 * skin );
			pShaderAPI->SetPixelShaderFogParams( 12 );
			pShaderAPI->SetPixelShaderIndex( pShaderAPI->GetPixelFogCombo() );
		}
		Draw();
	}
END_SHADER
