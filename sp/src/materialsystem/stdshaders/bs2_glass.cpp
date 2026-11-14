// BS2_Glass: pristine glass for models (watch crystals, vitrines, lamp lenses, windows on props).
// Schlick Fresnel env_cubemap reflection + sharp highlights from the model's dynamic lights and
// ambient cube, optional tint/absorption and framebuffer refraction. SM3 only; falls back to an
// additive VertexLitGeneric (like the stock stopgap material) on older hardware.
#include "BaseVSShader.h"
#include "shaderapi/ishaderapi.h"
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( BS2_Glass, "BS2 pristine glass (models, SM3)" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "env_cubemap", "Reflection cubemap (env_cubemap or a static cube)" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Reflection tint/brightness" )
		SHADER_PARAM( FRESNELMIN, SHADER_PARAM_TYPE_FLOAT, "0.04", "Reflectance head-on (F0). Glass ~0.04" )
		SHADER_PARAM( FRESNELMAX, SHADER_PARAM_TYPE_FLOAT, "1.0", "Reflectance at grazing angles" )
		SHADER_PARAM( FRESNELPOWER, SHADER_PARAM_TYPE_FLOAT, "5.0", "Schlick exponent (5 = physical)" )
		SHADER_PARAM( REFLECTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "1.0", "Overall Fresnel/reflection strength" )
		SHADER_PARAM( SPECEXPONENT, SHADER_PARAM_TYPE_FLOAT, "1024", "Highlight sharpness" )
		SHADER_PARAM( SPECBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Highlight brightness" )
		SHADER_PARAM( AMBIENTSHEEN, SHADER_PARAM_TYPE_FLOAT, "0.5", "Adds the model's ambient cube to the reflection (room light)" )
		SHADER_PARAM( GLASSTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Transmission tint (refraction mode)" )
		SHADER_PARAM( ABSORPTION, SHADER_PARAM_TYPE_FLOAT, "0.0", "0 = clear, 1 = nothing shows through" )
		SHADER_PARAM( REFRACTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0.0", ">0 = framebuffer refraction (thick glass), screen-UV offset" )
		SHADER_PARAM( GLASSPRESET, SHADER_PARAM_TYPE_INTEGER, "0", "0 = default, 1 = sharp highlights, 2 = glossy mirror sheen (only fills params you did not set)" )
		SHADER_PARAM( ENVMAPLDR, SHADER_PARAM_TYPE_INTEGER, "-1", "-1 auto, 1 = cubemap is LDR (skip HDR env scale), 0 = HDR" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		SET_FLAGS2( MATERIAL_VAR2_SUPPORTS_HW_SKINNING );
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_VERTEX_LIT );
		int preset = params[GLASSPRESET]->IsDefined() ? params[GLASSPRESET]->GetIntValue() : 0;
		if ( preset == 1 ) // sharp: tiny, very hot highlights
		{
			if ( !params[SPECEXPONENT]->IsDefined() ) params[SPECEXPONENT]->SetFloatValue( 4096.0f );
			if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 6.0f );
		}
		else if ( preset == 2 ) // glossy: strong Fresnel + room sheen, mirror-like
		{
			if ( !params[FRESNELMIN]->IsDefined() ) params[FRESNELMIN]->SetFloatValue( 0.10f );
			if ( !params[FRESNELPOWER]->IsDefined() ) params[FRESNELPOWER]->SetFloatValue( 3.0f );
			if ( !params[REFLECTAMOUNT]->IsDefined() ) params[REFLECTAMOUNT]->SetFloatValue( 1.6f );
			if ( !params[AMBIENTSHEEN]->IsDefined() ) params[AMBIENTSHEEN]->SetFloatValue( 1.2f );
			if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 2.5f );
		}
		if ( !params[ENVMAP]->IsDefined() ) params[ENVMAP]->SetStringValue( "env_cubemap" );
		if ( !params[ENVMAPTINT]->IsDefined() ) params[ENVMAPTINT]->SetVecValue( 1, 1, 1 );
		if ( !params[GLASSTINT]->IsDefined() ) params[GLASSTINT]->SetVecValue( 1, 1, 1 );
		if ( !params[FRESNELMIN]->IsDefined() ) params[FRESNELMIN]->SetFloatValue( 0.04f );
		if ( !params[FRESNELMAX]->IsDefined() ) params[FRESNELMAX]->SetFloatValue( 1.0f );
		if ( !params[FRESNELPOWER]->IsDefined() ) params[FRESNELPOWER]->SetFloatValue( 5.0f );
		if ( !params[REFLECTAMOUNT]->IsDefined() ) params[REFLECTAMOUNT]->SetFloatValue( 1.0f );
		if ( !params[SPECEXPONENT]->IsDefined() ) params[SPECEXPONENT]->SetFloatValue( 1024.0f );
		if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 1.0f );
		if ( !params[AMBIENTSHEEN]->IsDefined() ) params[AMBIENTSHEEN]->SetFloatValue( 0.5f );
		if ( !params[ABSORPTION]->IsDefined() ) params[ABSORPTION]->SetFloatValue( 0.0f );
		if ( !params[REFRACTAMOUNT]->IsDefined() ) params[REFRACTAMOUNT]->SetFloatValue( 0.0f );
		if ( !params[ENVMAPLDR]->IsDefined() ) params[ENVMAPLDR]->SetIntValue( -1 );
		if ( params[REFRACTAMOUNT]->GetFloatValue() > 0.0f )
			SET_FLAGS2( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
	}

	SHADER_FALLBACK
	{
		if ( !g_pHardwareConfig->SupportsShaderModel_3_0() )
		{
			// Same look as the stock stopgap: black base, additive cubemap.
			params[BASETEXTURE]->SetStringValue( "black" );
			SET_FLAGS( MATERIAL_VAR_ADDITIVE );
			return "VertexLitGeneric";
		}
		return 0;
	}

	SHADER_INIT
	{
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
		bool bRefract = params[REFRACTAMOUNT]->GetFloatValue() > 0.0f;
		SHADOW_STATE
		{
			SetInitialShadowState();
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			if ( bRefract )
				pShaderShadow->EnableBlending( false );
			else
			{
				pShaderShadow->EnableBlending( true );
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			pShaderShadow->EnableSRGBWrite( true );
			unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
			pShaderShadow->VertexShaderVertexFormat( flags, 1, NULL, 0 );
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			bool bHDROn = g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bHDROn || EnvIsLDR( params ) );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );
			pShaderShadow->SetVertexShader( "bs2_glass_vs30", 0 );
			pShaderShadow->SetPixelShader( "bs2_glass_ps30", 0 );
			pShaderShadow->EnableAlphaToCoverage( false );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();
			if ( params[ENVMAP]->IsTexture() ) BindTexture( SHADER_SAMPLER0, ENVMAP );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BLACK );
			if ( bRefract ) pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_BLACK );

			LightState_t lightState = { 0, false, false };
			pShaderAPI->GetDX9LightState( &lightState );
			pShaderAPI->SetPixelShaderStateAmbientLightCube( 4 );
			pShaderAPI->CommitPixelShaderLighting( 20 );

			float eye[4] = { 0, 0, 0, 0 }; pShaderAPI->GetWorldSpaceCameraPosition( eye );
			pShaderAPI->SetPixelShaderConstant( 0, eye );
			float fres[4] = { params[FRESNELMIN]->GetFloatValue(), params[FRESNELMAX]->GetFloatValue(),
				params[FRESNELPOWER]->GetFloatValue(), params[REFLECTAMOUNT]->GetFloatValue() };
			pShaderAPI->SetPixelShaderConstant( 1, fres );
			float spec[4] = { params[SPECEXPONENT]->GetFloatValue(), params[SPECBOOST]->GetFloatValue(),
				params[AMBIENTSHEEN]->GetFloatValue(), (float)lightState.m_nNumLights };
			pShaderAPI->SetPixelShaderConstant( 2, spec );
			float tint[4] = { 1, 1, 1, 0 }; params[GLASSTINT]->GetVecValue( tint, 3 ); tint[3] = params[ABSORPTION]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 3, tint );
			float env[4] = { 1, 1, 1, 1 }; params[ENVMAPTINT]->GetVecValue( env, 3 ); env[3] = EnvIsLDR( params ) ? 0.0f : 1.0f;
			pShaderAPI->SetPixelShaderConstant( 10, env );
			float refr[4] = { params[REFRACTAMOUNT]->GetFloatValue(), bRefract ? 1.0f : 0.0f, 0, 0 };
			pShaderAPI->SetPixelShaderConstant( 13, refr );

			int skin = pShaderAPI->GetCurrentNumBones() > 0 ? 1 : 0;
			pShaderAPI->SetVertexShaderIndex( (int)vertexCompression + 2 * skin );
			pShaderAPI->SetPixelShaderFogParams( 12 );
			pShaderAPI->SetPixelShaderIndex( pShaderAPI->GetPixelFogCombo() );
		}
		Draw();
	}
END_SHADER
