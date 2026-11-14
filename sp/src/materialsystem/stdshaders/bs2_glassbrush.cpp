// BS2_GlassBrush: lightmapped window / vitrine glass for world brushes.
// Fresnel env_cubemap reflection, lightmap sheen, a highlight from $lightdir (brushes have no model
// lights), optional dirt/frost base texture (alpha = coverage), tint/absorption, framebuffer
// refraction with frost blur, fog. Falls back to LightmappedGeneric ($translucent) below SM3.
#include "BaseVSShader.h"
#include "shaderapi/ishaderapi.h"
#include "tier0/memdbgon.h"

BEGIN_VS_SHADER( BS2_GlassBrush, "BS2 lightmapped glass (brushes, SM3)" )
	BEGIN_SHADER_PARAMS
		SHADER_PARAM( ENVMAP, SHADER_PARAM_TYPE_TEXTURE, "env_cubemap", "Reflection cubemap" )
		SHADER_PARAM( ENVMAPTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Reflection tint" )
		SHADER_PARAM( FRESNELMIN, SHADER_PARAM_TYPE_FLOAT, "0.04", "Reflectance head-on" )
		SHADER_PARAM( FRESNELMAX, SHADER_PARAM_TYPE_FLOAT, "1.0", "Reflectance at grazing angles" )
		SHADER_PARAM( FRESNELPOWER, SHADER_PARAM_TYPE_FLOAT, "5.0", "Schlick exponent" )
		SHADER_PARAM( REFLECTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "1.0", "Overall reflection strength" )
		SHADER_PARAM( SPECEXPONENT, SHADER_PARAM_TYPE_FLOAT, "1024", "Highlight sharpness" )
		SHADER_PARAM( SPECBOOST, SHADER_PARAM_TYPE_FLOAT, "1.0", "Highlight brightness" )
		SHADER_PARAM( LIGHTDIR, SHADER_PARAM_TYPE_VEC3, "[0 0 0]", "World direction TO the light for the highlight; [0 0 0] = off" )
		SHADER_PARAM( LIGHTCOLOR, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Highlight light color (linear)" )
		SHADER_PARAM( LIGHTMAPSHEEN, SHADER_PARAM_TYPE_FLOAT, "0.3", "Adds the baked lightmap to the reflection" )
		SHADER_PARAM( GLASSTINT, SHADER_PARAM_TYPE_COLOR, "[1 1 1]", "Transmission tint (refraction mode)" )
		SHADER_PARAM( ABSORPTION, SHADER_PARAM_TYPE_FLOAT, "0.0", "0 clear .. 1 opaque" )
		SHADER_PARAM( REFRACTAMOUNT, SHADER_PARAM_TYPE_FLOAT, "0.0", ">0 = framebuffer refraction" )
		SHADER_PARAM( FROST, SHADER_PARAM_TYPE_FLOAT, "0.0", "Frost blur radius in screen UV (needs refraction), e.g. 0.004" )
		SHADER_PARAM( BASEOPACITY, SHADER_PARAM_TYPE_FLOAT, "1.0", "Scales the base texture alpha (dirt coverage)" )
		SHADER_PARAM( GLASSPRESET, SHADER_PARAM_TYPE_INTEGER, "0", "0 default, 1 sharp, 2 glossy" )
		SHADER_PARAM( ENVMAPLDR, SHADER_PARAM_TYPE_INTEGER, "-1", "-1 auto, 1 LDR cube, 0 HDR cube" )
	END_SHADER_PARAMS

	SHADER_INIT_PARAMS()
	{
		SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
		SET_FLAGS2( MATERIAL_VAR2_LIGHTING_LIGHTMAP );
		int preset = params[GLASSPRESET]->IsDefined() ? params[GLASSPRESET]->GetIntValue() : 0;
		if ( preset == 1 )
		{
			if ( !params[SPECEXPONENT]->IsDefined() ) params[SPECEXPONENT]->SetFloatValue( 4096.0f );
			if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 6.0f );
		}
		else if ( preset == 2 )
		{
			if ( !params[FRESNELMIN]->IsDefined() ) params[FRESNELMIN]->SetFloatValue( 0.10f );
			if ( !params[FRESNELPOWER]->IsDefined() ) params[FRESNELPOWER]->SetFloatValue( 3.0f );
			if ( !params[REFLECTAMOUNT]->IsDefined() ) params[REFLECTAMOUNT]->SetFloatValue( 1.25f );
			if ( !params[LIGHTMAPSHEEN]->IsDefined() ) params[LIGHTMAPSHEEN]->SetFloatValue( 0.2f );
			if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 2.5f );
		}
		if ( !params[ENVMAP]->IsDefined() ) params[ENVMAP]->SetStringValue( "env_cubemap" );
		if ( !params[ENVMAPTINT]->IsDefined() ) params[ENVMAPTINT]->SetVecValue( 1, 1, 1 );
		if ( !params[GLASSTINT]->IsDefined() ) params[GLASSTINT]->SetVecValue( 1, 1, 1 );
		if ( !params[LIGHTCOLOR]->IsDefined() ) params[LIGHTCOLOR]->SetVecValue( 1, 1, 1 );
		if ( !params[LIGHTDIR]->IsDefined() ) params[LIGHTDIR]->SetVecValue( 0, 0, 0 );
		if ( !params[FRESNELMIN]->IsDefined() ) params[FRESNELMIN]->SetFloatValue( 0.04f );
		if ( !params[FRESNELMAX]->IsDefined() ) params[FRESNELMAX]->SetFloatValue( 1.0f );
		if ( !params[FRESNELPOWER]->IsDefined() ) params[FRESNELPOWER]->SetFloatValue( 5.0f );
		if ( !params[REFLECTAMOUNT]->IsDefined() ) params[REFLECTAMOUNT]->SetFloatValue( 1.0f );
		if ( !params[SPECEXPONENT]->IsDefined() ) params[SPECEXPONENT]->SetFloatValue( 1024.0f );
		if ( !params[SPECBOOST]->IsDefined() ) params[SPECBOOST]->SetFloatValue( 1.0f );
		if ( !params[LIGHTMAPSHEEN]->IsDefined() ) params[LIGHTMAPSHEEN]->SetFloatValue( 0.3f );
		if ( !params[ABSORPTION]->IsDefined() ) params[ABSORPTION]->SetFloatValue( 0.0f );
		if ( !params[REFRACTAMOUNT]->IsDefined() ) params[REFRACTAMOUNT]->SetFloatValue( 0.0f );
		if ( !params[FROST]->IsDefined() ) params[FROST]->SetFloatValue( 0.0f );
		if ( !params[BASEOPACITY]->IsDefined() ) params[BASEOPACITY]->SetFloatValue( 1.0f );
		if ( !params[ENVMAPLDR]->IsDefined() ) params[ENVMAPLDR]->SetIntValue( -1 );
		if ( params[REFRACTAMOUNT]->GetFloatValue() > 0.0f || params[FROST]->GetFloatValue() > 0.0f )
			SET_FLAGS2( MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE );
	}

	SHADER_FALLBACK
	{
		if ( !g_pHardwareConfig->SupportsShaderModel_3_0() )
		{
			SET_FLAGS( MATERIAL_VAR_TRANSLUCENT );
			return "LightmappedGeneric";
		}
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
		bool bRefract = params[REFRACTAMOUNT]->GetFloatValue() > 0.0f || params[FROST]->GetFloatValue() > 0.0f;
		bool bBase = params[BASETEXTURE]->IsTexture();
		SHADOW_STATE
		{
			SetInitialShadowState();
			pShaderShadow->EnableDepthWrites( false );
			pShaderShadow->EnableAlphaWrites( false );
			if ( bRefract ) pShaderShadow->EnableBlending( false );
			else
			{
				pShaderShadow->EnableBlending( true );
				pShaderShadow->BlendFunc( SHADER_BLEND_ONE, SHADER_BLEND_ONE_MINUS_SRC_ALPHA );
			}
			pShaderShadow->EnableSRGBWrite( true );
			int dims[2] = { 2, 2 };
			pShaderShadow->VertexShaderVertexFormat( VERTEX_POSITION | VERTEX_NORMAL, 2, dims, 0 );
			bool bHDROn = g_pHardwareConfig->GetHDRType() != HDR_TYPE_NONE;
			pShaderShadow->EnableTexture( SHADER_SAMPLER0, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER0, !bHDROn || EnvIsLDR( params ) );
			pShaderShadow->EnableTexture( SHADER_SAMPLER1, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER1, false );
			pShaderShadow->EnableTexture( SHADER_SAMPLER2, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER2, true );
			pShaderShadow->EnableTexture( SHADER_SAMPLER3, true );
			pShaderShadow->EnableSRGBRead( SHADER_SAMPLER3, false );
			pShaderShadow->SetVertexShader( "bs2_glassbrush_vs30", 0 );
			pShaderShadow->SetPixelShader( "bs2_glassbrush_ps30", 0 );
		}
		DYNAMIC_STATE
		{
			pShaderAPI->SetDefaultState();
			if ( params[ENVMAP]->IsTexture() ) BindTexture( SHADER_SAMPLER0, ENVMAP );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER0, TEXTURE_BLACK );
			if ( bRefract ) pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0 );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER1, TEXTURE_BLACK );
			if ( bBase ) BindTexture( SHADER_SAMPLER2, BASETEXTURE, FRAME );
			else pShaderAPI->BindStandardTexture( SHADER_SAMPLER2, TEXTURE_BLACK );
			pShaderAPI->BindStandardTexture( SHADER_SAMPLER3, TEXTURE_LIGHTMAP );

			float eye[4] = { 0, 0, 0, 0 }; pShaderAPI->GetWorldSpaceCameraPosition( eye );
			pShaderAPI->SetPixelShaderConstant( 0, eye );
			float fres[4] = { params[FRESNELMIN]->GetFloatValue(), params[FRESNELMAX]->GetFloatValue(),
				params[FRESNELPOWER]->GetFloatValue(), params[REFLECTAMOUNT]->GetFloatValue() };
			pShaderAPI->SetPixelShaderConstant( 1, fres );
			float spec[4] = { params[SPECEXPONENT]->GetFloatValue(), params[SPECBOOST]->GetFloatValue(),
				params[LIGHTMAPSHEEN]->GetFloatValue(), bBase ? 1.0f : 0.0f };
			pShaderAPI->SetPixelShaderConstant( 2, spec );
			float tint[4] = { 1, 1, 1, 0 }; params[GLASSTINT]->GetVecValue( tint, 3 ); tint[3] = params[ABSORPTION]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 3, tint );
			float ld[4] = { 0, 0, 0, 0 }; params[LIGHTDIR]->GetVecValue( ld, 3 );
			ld[3] = ( ld[0] * ld[0] + ld[1] * ld[1] + ld[2] * ld[2] ) > 0.0001f ? 1.0f : 0.0f;
			pShaderAPI->SetPixelShaderConstant( 4, ld );
			float lc[4] = { 1, 1, 1, 0 }; params[LIGHTCOLOR]->GetVecValue( lc, 3 ); lc[3] = params[FROST]->GetFloatValue();
			pShaderAPI->SetPixelShaderConstant( 5, lc );
			float env[4] = { 1, 1, 1, 1 }; params[ENVMAPTINT]->GetVecValue( env, 3 ); env[3] = EnvIsLDR( params ) ? 0.0f : 1.0f;
			pShaderAPI->SetPixelShaderConstant( 10, env );
			pShaderAPI->SetPixelShaderFogParams( 12 );
			float refr[4] = { params[REFRACTAMOUNT]->GetFloatValue(), bRefract ? 1.0f : 0.0f, params[BASEOPACITY]->GetFloatValue(), 0 };
			pShaderAPI->SetPixelShaderConstant( 13, refr );
			pShaderAPI->SetVertexShaderIndex( 0 );
			pShaderAPI->SetPixelShaderIndex( pShaderAPI->GetPixelFogCombo() );
		}
		Draw();
	}
END_SHADER
