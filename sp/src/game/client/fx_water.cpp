//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "clienteffectprecachesystem.h"
#include "fx_sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "tier0/vprof.h"
#include "fx.h"
#include "fx_water.h"
#include "particles_new.h"
#include "soundenvelope.h"
#include "c_recipientfilter.h"
#include "cliententitylist.h"
#include "view.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheEffectSplash )
CLIENTEFFECT_MATERIAL( "effects/splash1" )
CLIENTEFFECT_MATERIAL( "effects/splash2" )
CLIENTEFFECT_MATERIAL( "effects/splash4" )
CLIENTEFFECT_MATERIAL( "effects/slime1" )
CLIENTEFFECT_REGISTER_END()


#define	SPLASH_MIN_SPEED	50.0f
#define	SPLASH_MAX_SPEED	100.0f

ConVar	cl_show_splashes( "cl_show_splashes", "1" );

static Vector s_vecSlimeColor( 46.0f/255.0f, 90.0f/255.0f, 36.0f/255.0f );

// Each channel does not contribute to the luminosity equally, as represented here
#define	RED_CHANNEL_CONTRIBUTION	0.30f
#define GREEN_CHANNEL_CONTRIBUTION	0.59f
#define	BLUE_CHANNEL_CONTRIBUTION	0.11f

//-----------------------------------------------------------------------------
// Purpose: Returns a normalized tint and luminosity for a specified color
// Input  : &color - normalized input color to extract information from
//			*tint - normalized tint of that color
//			*luminosity - normalized luminosity of that color
//-----------------------------------------------------------------------------
void UTIL_GetNormalizedColorTintAndLuminosity( const Vector &color, Vector *tint, float *luminosity )
{
	// Give luminosity if requested
	if ( luminosity != NULL )
	{
		// Each channel contributes differently than the others
		*luminosity =	( color.x * RED_CHANNEL_CONTRIBUTION ) +
						( color.y * GREEN_CHANNEL_CONTRIBUTION ) +
						( color.z * BLUE_CHANNEL_CONTRIBUTION );
	}

	// Give tint if requested
	if ( tint != NULL )
	{
		if ( color == vec3_origin )
		{
			*tint = vec3_origin;
		}
		else
		{
			float maxComponent = MAX( color.x, MAX( color.y, color.z ) );
			*tint = color / maxComponent;
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_WaterRipple( const Vector &origin, float scale, Vector *pColor, float flLifetime, float flAlpha )
{
	VPROF_BUDGET( "FX_WaterRipple", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	trace_t	tr;

	Vector	color = pColor ? *pColor : Vector( 0.8f, 0.8f, 0.75f );

	Vector startPos = origin + Vector(0,0,8);
	Vector endPos = origin + Vector(0,0,-64);

	UTIL_TraceLine( startPos, endPos, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.fraction < 1.0f )
	{
		//Add a ripple quad to the surface
		FX_AddQuad( tr.endpos + ( tr.plane.normal * 0.5f ), 
					tr.plane.normal, 
					16.0f*scale, 
					128.0f*scale, 
					0.7f,
					flAlpha,	// start alpha
					0.0f,		// end alpha
					0.25f,
					random->RandomFloat( 0, 360 ),
					random->RandomFloat( -16.0f, 16.0f ),
					color, 
					flLifetime, 
					"effects/splashwake1", 
					(FXQUAD_BIAS_SCALE|FXQUAD_BIAS_ALPHA) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//-----------------------------------------------------------------------------
static void FX_WaterSplash( const Vector &origin, const Vector &normal, float scale, float density, bool playSound )
{
	VPROF_BUDGET( "FX_GunshotSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
	if ( cl_show_splashes.GetBool() == false )
		return;

	Vector	color;
	float	luminosity;
	
	// Get our lighting information
	FX_GetSplashLighting( origin + ( normal * scale ), &color, &luminosity );

	float flScale = scale / 8.0f;

	if ( flScale > 4.0f )
	{
		flScale = 4.0f;
	}

	// Setup our trail emitter
	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "splash" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( origin );
	sparkEmitter->m_ParticleCollision.SetGravity( 800.0f );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 2.0f );
	sparkEmitter->GetBinding().SetBBox( origin - Vector( 32, 32, 32 ), origin + Vector( 32, 32, 32 ) );

	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/splash2" );

	TrailParticle	*tParticle;

	Vector	offDir;
	Vector	offset;
	float	colorRamp;

	//Dump out drops
	for ( int i = 0; i < int(16 * density); i++ )
	{
		offset = origin;
		offset[0] += random->RandomFloat( -8.0f, 8.0f ) * flScale;
		offset[1] += random->RandomFloat( -8.0f, 8.0f ) * flScale;

		tParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( tParticle == NULL )
			break;

		tParticle->m_flLifetime	= 0.0f;
		tParticle->m_flDieTime	= random->RandomFloat( 0.25f, 0.5f );

		offDir = normal + RandomVector( -0.8f, 0.8f );

		tParticle->m_vecVelocity = offDir * random->RandomFloat( SPLASH_MIN_SPEED * flScale * 3.0f, SPLASH_MAX_SPEED * flScale * 3.0f );
		tParticle->m_vecVelocity[2] += random->RandomFloat( 32.0f, 64.0f ) * flScale;

		tParticle->m_flWidth		= random->RandomFloat( 1.0f, 3.0f );
		tParticle->m_flLength		= random->RandomFloat( 0.025f, 0.05f );

		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		tParticle->m_color.r = MIN( 1.0f, color[0] * colorRamp ) * 255;
		tParticle->m_color.g = MIN( 1.0f, color[1] * colorRamp ) * 255;
		tParticle->m_color.b = MIN( 1.0f, color[2] * colorRamp ) * 255;
		tParticle->m_color.a = luminosity * 255;
	}

	// Setup the particle emitter
	CSmartPtr<CSplashParticle> pSimple = CSplashParticle::Create( "splish" );
	pSimple->SetSortOrigin( origin );
	pSimple->SetClipHeight( origin.z );
	pSimple->SetParticleCullRadius( scale * 2.0f );
	pSimple->GetBinding().SetBBox( origin - Vector( 32, 32, 32 ), origin + Vector( 32, 32, 32 ) );

	SimpleParticle	*pParticle;

	//Main gout
	for ( int i = 0; i < 8; i++ )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial, origin );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= 2.0f;	//NOTENOTE: We use a clip plane to realistically control our lifespan

		pParticle->m_vecVelocity.Random( -0.2f, 0.2f );
		pParticle->m_vecVelocity += ( normal * random->RandomFloat( 4.0f, 6.0f ) );
		
		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity *= 50 * flScale * (8-i);
		
		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= 24 * flScale * RemapValClamped( i, 7, 0, 1, 0.5f );
		pParticle->m_uchEndSize		= MIN( 255, pParticle->m_uchStartSize * 2 );
		
		pParticle->m_uchStartAlpha	= RemapValClamped( i, 7, 0, 255, 32 ) * luminosity;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
	}

	// Do a ripple
	FX_WaterRipple( origin, flScale, &color, 1.5f, luminosity );

	// Moving wheels reuse the entry visuals without repeating impact sounds.
	if ( !playSound )
		return;

	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName =  "Physics.WaterSplash";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_pOrigin = &origin;


	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

// Preserve the stock entry/impact splash behavior.
void FX_GunshotSplash( const Vector &origin, const Vector &normal, float scale )
{
	FX_WaterSplash( origin, normal, scale, 1.0f, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&normal - 
//			scale - 
//			*pColor - 
//-----------------------------------------------------------------------------
void FX_GunshotSlimeSplash( const Vector &origin, const Vector &normal, float scale )
{
	if ( cl_show_splashes.GetBool() == false )
		return;

	VPROF_BUDGET( "FX_GunshotSlimeSplash", VPROF_BUDGETGROUP_PARTICLE_RENDERING );
	
#if 0

	float	colorRamp;
	float	flScale = MIN( 1.0f, scale / 8.0f );

	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/slime1" );
	PMaterialHandle	hMaterial2 = ParticleMgr()->GetPMaterial( "effects/splash4" );

	Vector	color;
	float	luminosity;
	
	// Get our lighting information
	FX_GetSplashLighting( origin + ( normal * scale ), &color, &luminosity );

	Vector	offDir;
	Vector	offset;

	TrailParticle	*tParticle;

	CSmartPtr<CTrailParticles> sparkEmitter = CTrailParticles::Create( "splash" );

	if ( !sparkEmitter )
		return;

	sparkEmitter->SetSortOrigin( origin );
	sparkEmitter->m_ParticleCollision.SetGravity( 800.0f );
	sparkEmitter->SetFlag( bitsPARTICLE_TRAIL_VELOCITY_DAMPEN );
	sparkEmitter->SetVelocityDampen( 2.0f );
	if ( IsXbox() )
	{
		sparkEmitter->GetBinding().SetBBox( origin - Vector( 32, 32, 64 ), origin + Vector( 32, 32, 64 ) );
	}

	//Dump out drops
	for ( int i = 0; i < 24; i++ )
	{
		offset = origin;
		offset[0] += random->RandomFloat( -16.0f, 16.0f ) * flScale;
		offset[1] += random->RandomFloat( -16.0f, 16.0f ) * flScale;

		tParticle = (TrailParticle *) sparkEmitter->AddParticle( sizeof(TrailParticle), hMaterial, offset );

		if ( tParticle == NULL )
			break;

		tParticle->m_flLifetime	= 0.0f;
		tParticle->m_flDieTime	= random->RandomFloat( 0.25f, 0.5f );

		offDir = normal + RandomVector( -0.6f, 0.6f );

		tParticle->m_vecVelocity = offDir * random->RandomFloat( SPLASH_MIN_SPEED * flScale * 3.0f, SPLASH_MAX_SPEED * flScale * 3.0f );
		tParticle->m_vecVelocity[2] += random->RandomFloat( 32.0f, 64.0f ) * flScale;
   
		tParticle->m_flWidth		= random->RandomFloat( 3.0f, 6.0f ) * flScale;
		tParticle->m_flLength		= random->RandomFloat( 0.025f, 0.05f ) * flScale;

		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		tParticle->m_color.r = MIN( 1.0f, color.x * colorRamp ) * 255;
		tParticle->m_color.g = MIN( 1.0f, color.y * colorRamp ) * 255;
		tParticle->m_color.b = MIN( 1.0f, color.z * colorRamp ) * 255;
		tParticle->m_color.a = 255 * luminosity;
	}

	// Setup splash emitter
	CSmartPtr<CSplashParticle> pSimple = CSplashParticle::Create( "splish" );
	pSimple->SetSortOrigin( origin );
	pSimple->SetClipHeight( origin.z );
	pSimple->SetParticleCullRadius( scale * 2.0f );

	if ( IsXbox() )
	{
		pSimple->GetBinding().SetBBox( origin - Vector( 32, 32, 64 ), origin + Vector( 32, 32, 64 ) );
	}

	SimpleParticle	*pParticle;

	// Tint
	colorRamp = random->RandomFloat( 0.75f, 1.0f );
	color = Vector( 1.0f, 0.8f, 0.0f ) * color * colorRamp;

	//Main gout
	for ( int i = 0; i < 8; i++ )
	{
		pParticle = (SimpleParticle *) pSimple->AddParticle( sizeof( SimpleParticle ), hMaterial2, origin );

		if ( pParticle == NULL )
			break;

		pParticle->m_flLifetime = 0.0f;
		pParticle->m_flDieTime	= 2.0f;	//NOTENOTE: We use a clip plane to realistically control our lifespan

		pParticle->m_vecVelocity.Random( -0.2f, 0.2f );
		pParticle->m_vecVelocity += ( normal * random->RandomFloat( 4.0f, 6.0f ) );
		
		VectorNormalize( pParticle->m_vecVelocity );

		pParticle->m_vecVelocity *= 50 * flScale * (8-i);
		
		colorRamp = random->RandomFloat( 0.75f, 1.25f );

		pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[1]	= MIN( 1.0f, color[1] * colorRamp ) * 255.0f;
		pParticle->m_uchColor[2]	= MIN( 1.0f, color[2] * colorRamp ) * 255.0f;
		
		pParticle->m_uchStartSize	= 24 * flScale * RemapValClamped( i, 7, 0, 1, 0.5f );
		pParticle->m_uchEndSize		= MIN( 255, pParticle->m_uchStartSize * 2 );
		
		pParticle->m_uchStartAlpha	= RemapValClamped( i, 7, 0, 255, 32 ) * luminosity;
		pParticle->m_uchEndAlpha	= 0;
		
		pParticle->m_flRoll			= random->RandomInt( 0, 360 );
		pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
	}
	
#else
	
	QAngle vecAngles;
	VectorAngles( normal, vecAngles );
	if ( scale < 2.0f )
	{
		DispatchParticleEffect( "slime_splash_01", origin, vecAngles );
	}
	else if ( scale < 4.0f )
	{
		DispatchParticleEffect( "slime_splash_02", origin, vecAngles );
	}
	else
	{
		DispatchParticleEffect( "slime_splash_03", origin, vecAngles );
	}

#endif

	//Play a sound
	CLocalPlayerFilter filter;

	EmitSound_t ep;
	ep.m_nChannel = CHAN_VOICE;
	ep.m_pSoundName =  "Physics.WaterSplash";
	ep.m_flVolume = 1.0f;
	ep.m_SoundLevel = SNDLVL_NORM;
	ep.m_pOrigin = &origin;

	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, ep );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool BS2WheelWakeNear( const Vector &pos, float dist );  // below, BS2 buggy wheel wake

void SplashCallback( const CEffectData &data )
{
	if ( BS2WheelWakeNear( data.m_vOrigin, 220.0f ) )
		return;

	Vector	normal;

	AngleVectors( data.m_vAngles, &normal );

	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT( "watersplash", SplashCallback );


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void GunshotSplashCallback( const CEffectData &data )
{
	if ( data.m_fFlags & FX_WATER_IN_SLIME )
	{
		FX_GunshotSlimeSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
	else
	{
		FX_GunshotSplash( data.m_vOrigin, Vector(0,0,1), data.m_flScale );
	}
}

DECLARE_CLIENT_EFFECT( "gunshotsplash", GunshotSplashCallback );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &data - 
//-----------------------------------------------------------------------------
void RippleCallback( const CEffectData &data )
{
	if ( BS2WheelWakeNear( data.m_vOrigin, 96.0f ) )
		return;

	float	flScale = data.m_flScale / 8.0f;

	Vector	color;
	float	luminosity;
	
	// Get our lighting information
	FX_GetSplashLighting( data.m_vOrigin + ( Vector(0,0,1) * 4.0f ), &color, &luminosity );

	FX_WaterRipple( data.m_vOrigin, flScale, &color, 1.5f, luminosity );
}

DECLARE_CLIENT_EFFECT( "waterripple", RippleCallback );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pDebugName - 
// Output : WaterDebrisEffect*
//-----------------------------------------------------------------------------
WaterDebrisEffect* WaterDebrisEffect::Create( const char *pDebugName )
{
	return new WaterDebrisEffect( pDebugName );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float WaterDebrisEffect::UpdateAlpha( const SimpleParticle *pParticle )
{
	return ( ((float)pParticle->m_uchStartAlpha/255.0f) * sin( M_PI * (pParticle->m_flLifetime / pParticle->m_flDieTime) ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
// Output : float
//-----------------------------------------------------------------------------
float CSplashParticle::UpdateRoll( SimpleParticle *pParticle, float timeDelta )
{
	pParticle->m_flRoll += pParticle->m_flRollDelta * timeDelta;
	
	pParticle->m_flRollDelta += pParticle->m_flRollDelta * ( timeDelta * -4.0f );

	//Cap the minimum roll
	if ( fabs( pParticle->m_flRollDelta ) < 0.5f )
	{
		pParticle->m_flRollDelta = ( pParticle->m_flRollDelta > 0.0f ) ? 0.5f : -0.5f;
	}

	return pParticle->m_flRoll;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
//			timeDelta - 
//-----------------------------------------------------------------------------
void CSplashParticle::UpdateVelocity( SimpleParticle *pParticle, float timeDelta )
{
	//Decellerate
	static float dtime;
	static float decay;

	if ( dtime != timeDelta )
	{
		dtime = timeDelta;
		float expected = 3.0f;
		decay = exp( log( 0.0001f ) * dtime / expected );
	}

	pParticle->m_vecVelocity *= decay;
	pParticle->m_vecVelocity[2] -= ( 800.0f * timeDelta );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pParticle - 
// Output : float
//-----------------------------------------------------------------------------
float CSplashParticle::UpdateAlpha( const SimpleParticle *pParticle )
{
	if ( m_bUseClipHeight )
	{
		float flAlpha = pParticle->m_uchStartAlpha / 255.0f;

		return  flAlpha * RemapValClamped(pParticle->m_Pos.z,
								m_flClipHeight,
								m_flClipHeight - ( UpdateScale( pParticle ) * 0.5f ),
								1.0f,
								0.0f );
	}

	return (pParticle->m_uchStartAlpha/255.0f) + ( (float)(pParticle->m_uchEndAlpha/255.0f) - (float)(pParticle->m_uchStartAlpha/255.0f) ) * (pParticle->m_flLifetime / pParticle->m_flDieTime);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &clipPlane - 
//-----------------------------------------------------------------------------
void CSplashParticle::SetClipHeight( float flClipHeight )
{
	m_bUseClipHeight = true;
	m_flClipHeight = flClipHeight;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pIterator - 
//-----------------------------------------------------------------------------
void CSplashParticle::SimulateParticles( CParticleSimulateIterator *pIterator )
{
	float timeDelta = pIterator->GetTimeDelta();

	SimpleParticle *pParticle = (SimpleParticle*)pIterator->GetFirst();
	
	while ( pParticle )
	{
		//Update velocity
		UpdateVelocity( pParticle, timeDelta );
		pParticle->m_Pos += pParticle->m_vecVelocity * timeDelta;

		// Clip by height if requested
		if ( m_bUseClipHeight )
		{
			// See if we're below, and therefore need to clip
			if ( pParticle->m_Pos.z + UpdateScale( pParticle ) < m_flClipHeight )
			{
				pIterator->RemoveParticle( pParticle );
				pParticle = (SimpleParticle*)pIterator->GetNext();
				continue;
			}
		}

		//Should this particle die?
		pParticle->m_flLifetime += timeDelta;
		UpdateRoll( pParticle, timeDelta );

		if ( pParticle->m_flLifetime >= pParticle->m_flDieTime )
			pIterator->RemoveParticle( pParticle );

		pParticle = (SimpleParticle*)pIterator->GetNext();
	}
}


// BS2 buggy wheel water (Nov 2026): a PCF water trail per wheel (particles/bs2_wheelwater.pcf) - two tilted-out spray
// sheets (both sides of the tyre), droplets that arc up and fall, a rooster plume, mist, landing ripple rings, a V-shaped
// foam wake behind the tyre, plus Refract layers (refract blobs, streaks and surface rings).
// The server (vehicle_jeep.cpp HandleWater) dispatches this at the per-wheel ripple cadence (0.1-0.3 s) with
//   m_vOrigin = contact point on the water surface, m_vStart = buggy velocity, m_vNormal = outward side of that wheel,
//   m_flScale = 0.25..1 by speed (20..600 HU/s) * bs2_wheel_splash_scale.
// Control points (all with the orthonormal frame forward = travel, right, up = +Z):
//   CP0 = tyre contact point, CP1 = contact point (plume), CP2 = trail tail (behind the tyre, length grows with speed),
//   CP3 = landing zone of the thrown water (further back). The PCF only uses the frame symmetrically in local Y, so
//   both sides of the tyre get the same sheet / wake / ripples whatever the Y sign convention is.
static ConVar bs2_wheel_splash_density( "bs2_wheel_splash_density", "2", FCVAR_ARCHIVE, "Legacy buggy splash droplet density multiplier (0.5-3), style 0 only." );
static ConVar bs2_wheel_splash_style( "bs2_wheel_splash_style", "1", FCVAR_ARCHIVE, "Buggy wheel water look: 1 = PCF water trail with refraction (bs2_wheelwater), 0 = legacy droplet plume." );

CLIENTEFFECT_REGISTER_BEGIN( PrecacheBS2WheelWater )
CLIENTEFFECT_MATERIAL( "particle/bs2_wheelwater/spray_streak" )
CLIENTEFFECT_MATERIAL( "particle/bs2_wheelwater/spray_sheet" )
CLIENTEFFECT_MATERIAL( "particle/bs2_wheelwater/spray_mist" )
CLIENTEFFECT_MATERIAL( "particle/bs2_wheelwater/spray_drops" )
CLIENTEFFECT_MATERIAL( "particle/bs2_wheelwater/wake_foam" )
CLIENTEFFECT_MATERIAL( "particle/bs2_wheelwater/ripple_ring" )
CLIENTEFFECT_MATERIAL( "particle/water/watersplash_001a_refract" )
CLIENTEFFECT_MATERIAL( "particle/water/waterdrop_001a_refract" )
CLIENTEFFECT_MATERIAL( "particle/particle_ring_refract_01" )
CLIENTEFFECT_REGISTER_END()

static bool BS2WheelWaterTrail( const CEffectData &data, float scale )
{
	const Vector up( 0, 0, 1 );
	Vector travel = data.m_vStart;
	travel.z = 0.0f;
	const float speed = VectorNormalize( travel );
	if ( speed < 1.0f )
	{
		Vector outward = data.m_vNormal;
		outward.z = 0.0f;
		if ( VectorNormalize( outward ) < 0.01f )
			outward = Vector( 0, 1, 0 );
		travel = CrossProduct( up, outward );
		VectorNormalize( travel );
	}
	Vector right = CrossProduct( travel, up );
	VectorNormalize( right );
	const Vector back = -travel;

	const char *pszName = scale < 0.45f ? "bs2_wheelwater_light" : ( scale < 0.8f ? "bs2_wheelwater" : "bs2_wheelwater_heavy" );
	CSmartPtr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, pszName );
	if ( !pEffect.IsValid() || !pEffect->IsValid() )
		return false;
	const Vector tail = data.m_vOrigin + back * MIN( speed * 0.22f, 150.0f );
	const Vector land = data.m_vOrigin + back * MIN( 30.0f + speed * 0.32f, 230.0f );
	pEffect->SetSortOrigin( data.m_vOrigin );
	pEffect->SetControlPoint( 0, data.m_vOrigin );
	pEffect->SetControlPointOrientation( 0, travel, right, up );
	pEffect->SetControlPoint( 1, data.m_vOrigin );
	pEffect->SetControlPointOrientation( 1, travel, right, up );
	pEffect->SetControlPoint( 2, tail );
	pEffect->SetControlPointOrientation( 2, travel, right, up );
	pEffect->SetControlPoint( 3, land );
	pEffect->SetControlPointOrientation( 3, travel, right, up );
	return true;
}

// ---- v6: continuous wake + water drive sound -------------------------------------------------------------------
// One persistent, continuously emitting "bs2_wheelwake" per wheel. Server events only start / keep it alive; the
// control point follows the buggy every frame (contact point kept in the buggy's local space).
// CP0 = tyre contact on the water surface (frame: forward = travel, right, up = +Z), CP1.x = intensity 0..1.
static ConVar bs2_wheel_wake( "bs2_wheel_wake", "1", 0, "Buggy wheel water: 1 = one continuous wake per wheel (bs2_wheelwake), 0 = per-event effects (bs2_wheel_splash_style)." );
static ConVar bs2_wheel_wake_timeout( "bs2_wheel_wake_timeout", "0.45", 0, "Seconds without a wheel water event before that wheel's wake stops emitting." );
static ConVar bs2_wheel_wake_topspeed( "bs2_wheel_wake_topspeed", "560", 0, "Buggy speed (HU/s) treated as top speed for the water sound." );
static ConVar bs2_wheel_wake_brake_decel( "bs2_wheel_wake_brake_decel", "450", 0, "Deceleration (HU/s^2) above which the water sound drops fast (braking)." );
static ConVar bs2_wheel_wake_volume( "bs2_wheel_wake_volume", "1", 0, "Buggy water drive sound volume scale." );
static ConVar bs2_wheel_wake_front_scale( "bs2_wheel_wake_front_scale", "0.45", 0, "Spray strength at the front wheels (rear = 1): the wake builds up toward the back." );
static ConVar bs2_wheel_wake_axle_split( "bs2_wheel_wake_axle_split", "-50", 0, "Vehicle-local Y between the axles (wheels above it count as front)." );
static ConVar bs2_wheel_wake_cam_near( "bs2_wheel_wake_cam_near", "70", 0, "Camera closer than this to a wheel: spray at bs2_wheel_wake_cam_min." );
static ConVar bs2_wheel_wake_cam_far( "bs2_wheel_wake_cam_far", "170", 0, "Camera further than this from a wheel: full spray." );
static ConVar bs2_wheel_wake_cam_min( "bs2_wheel_wake_cam_min", "0.3", 0, "Spray strength when a camera sits right at the wheel (tyre / rear-wheel cams)." );
static ConVar bs2_wheel_wake_debug( "bs2_wheel_wake_debug", "0", 0, "Print buggy wheel wake / water sound state." );

#define BS2_WATER_DRIVE_LOOP	"vehicles/airboat/pontoon_fast_water_loop1.wav"
#define BS2_WATER_HOLD_LOOP		"vehicles/airboat/pontoon_fast_water_loop2.wav"

struct BS2WheelWake_t
{
	CSmartPtr<CNewParticleEffect> m_pEffect;
	EHANDLE m_hVehicle;		// buggy the wheel belongs to (NULL -> dead reckoning)
	Vector m_vecLocal;		// last reported contact point in the buggy's local space
	Vector m_vecLocalCur;	// eased towards m_vecLocal
	float m_flWaterZ;		// water surface height
	Vector m_vecPos;		// control point
	Vector m_vecAnchor;		// last reported contact point (world)
	Vector m_vecVel;		// travel velocity (2D)
	Vector m_vecSide;		// outward side of the wheel
	float m_flAnchorTime;
	float m_flIntensity;
};

static void BS2WakeFrame( const Vector &vel, const Vector &side, Vector &travel, Vector &right )
{
	const Vector up( 0, 0, 1 );
	travel = vel;
	travel.z = 0.0f;
	if ( VectorNormalize( travel ) < 1.0f )
	{
		Vector outward = side;
		outward.z = 0.0f;
		if ( VectorNormalize( outward ) < 0.01f )
			outward = Vector( 0, 1, 0 );
		travel = CrossProduct( up, outward );
		VectorNormalize( travel );
	}
	right = CrossProduct( travel, up );
	VectorNormalize( right );
}

static C_BaseEntity *BS2FindVehicle( const Vector &pos )
{
	C_BaseEntity *pBest = NULL;
	float flBest = 256.0f * 256.0f;
	for ( C_BaseEntity *pEnt = ClientEntityList().FirstBaseEntity(); pEnt; pEnt = ClientEntityList().NextBaseEntity( pEnt ) )
	{
		if ( !pEnt->GetClientVehicle() || pEnt->IsDormant() )
			continue;
		const float d = ( pEnt->GetAbsOrigin() - pos ).LengthSqr();
		if ( d < flBest )
		{
			flBest = d;
			pBest = pEnt;
		}
	}
	return pBest;
}

class CBS2WheelWakeSystem : public CAutoGameSystemPerFrame
{
public:
	CBS2WheelWakeSystem() : CAutoGameSystemPerFrame( "CBS2WheelWakeSystem" )
	{
		m_pDrive = m_pHold = NULL;
		m_flDriveVol = m_flHoldVol = m_flPrevSpeed = m_flDecel = m_flTopTime = m_flSilentTime = m_flBrakeTime = 0.0f;
	}
	virtual void LevelInitPreEntity()
	{
		enginesound->PrecacheSound( BS2_WATER_DRIVE_LOOP, true );
		enginesound->PrecacheSound( BS2_WATER_HOLD_LOOP, true );
		g_pParticleSystemMgr->PrecacheParticleSystem( "bs2_wheelwake" );
	}
	virtual void LevelShutdownPreEntity() { StopAll(); StopSound(); }
	virtual void Update( float frametime )
	{
		const float now = gpGlobals->curtime;
		const float timeout = MAX( 0.1f, bs2_wheel_wake_timeout.GetFloat() );
		C_BaseEntity *pWaterVehicle = NULL;
		for ( int i = m_Wakes.Count() - 1; i >= 0; --i )
		{
			BS2WheelWake_t &w = m_Wakes[i];
			const float age = now - w.m_flAnchorTime;
			C_BaseEntity *pVeh = w.m_hVehicle.Get();
			float speed = w.m_vecVel.Length2D();
			if ( pVeh )
			{
				Vector v;
				pVeh->EstimateAbsVelocity( v );
				v.z = 0.0f;
				speed = v.Length2D();
				if ( speed > 1.0f )
					w.m_vecVel = v;
			}
			if ( !w.m_pEffect.IsValid() || age > timeout || age < -1.0f || ( pVeh && speed < 12.0f ) )
			{
				if ( w.m_pEffect.IsValid() )
					w.m_pEffect->StopEmission();
				m_Wakes.FastRemove( i );
				continue;
			}
			if ( pVeh )
			{
				w.m_vecLocalCur += ( w.m_vecLocal - w.m_vecLocalCur ) * MIN( 1.0f, frametime * 8.0f );
				VectorTransform( w.m_vecLocalCur, pVeh->EntityToWorldTransform(), w.m_vecPos );
				w.m_vecPos.z = w.m_flWaterZ;
				pWaterVehicle = pVeh;
			}
			else
			{
				const Vector target = w.m_vecAnchor + w.m_vecVel * age;
				w.m_vecPos += w.m_vecVel * frametime;
				w.m_vecPos += ( target - w.m_vecPos ) * MIN( 1.0f, frametime * 10.0f );
				w.m_vecPos.z = target.z;
			}
			// the spray thins out for a camera parked at the wheel (tyre / rear-wheel cams) and builds up toward the back axle
			const float camDist = ( MainViewOrigin() - w.m_vecPos ).Length();
			const float camScale = RemapValClamped( camDist, bs2_wheel_wake_cam_near.GetFloat(), bs2_wheel_wake_cam_far.GetFloat(), bs2_wheel_wake_cam_min.GetFloat(), 1.0f );
			const float axleScale = ( w.m_vecLocalCur.y > bs2_wheel_wake_axle_split.GetFloat() ) ? bs2_wheel_wake_front_scale.GetFloat() : 1.0f;
			w.m_flIntensity = RemapValClamped( speed, 20.0f, 600.0f, 0.25f, 1.0f ) * camScale * axleScale;
			Apply( w );
		}
		UpdateSound( pWaterVehicle, frametime );
	}
	bool Event( const CEffectData &data, float scale )
	{
		Vector vel = data.m_vStart;
		vel.z = 0.0f;
		Vector side = data.m_vNormal;
		side.z = 0.0f;
		VectorNormalize( side );
		int best = -1;
		float bestDist = 72.0f;
		for ( int i = 0; i < m_Wakes.Count(); ++i )
		{
			if ( DotProduct( m_Wakes[i].m_vecSide, side ) < 0.3f )
				continue;
			const float dist = ( m_Wakes[i].m_vecPos - data.m_vOrigin ).Length2D();
			if ( dist < bestDist )
			{
				bestDist = dist;
				best = i;
			}
		}
		C_BaseEntity *pVeh = BS2FindVehicle( data.m_vOrigin );
		Vector local( 0, 0, 0 );
		if ( pVeh )
			VectorITransform( data.m_vOrigin, pVeh->EntityToWorldTransform(), local );
		if ( best < 0 )
		{
			CSmartPtr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, "bs2_wheelwake" );
			if ( !pEffect.IsValid() || !pEffect->IsValid() )
			{
				Warning( "BS2 wake: could not create bs2_wheelwake\n" );
				return false;
			}
			best = m_Wakes.AddToTail();
			m_Wakes[best].m_pEffect = pEffect;
			m_Wakes[best].m_vecPos = data.m_vOrigin;
			m_Wakes[best].m_vecLocalCur = local;
			if ( bs2_wheel_wake_debug.GetBool() )
				Msg( "BS2 wake start (%d active) vehicle %d local %.0f %.0f\n", m_Wakes.Count(), pVeh ? pVeh->entindex() : -1, local.x, local.y );
		}
		BS2WheelWake_t &w = m_Wakes[best];
		w.m_hVehicle = pVeh;
		w.m_vecLocal = local;
		w.m_flWaterZ = data.m_vOrigin.z;
		w.m_vecAnchor = data.m_vOrigin;
		if ( vel.Length2D() > 1.0f )
			w.m_vecVel = vel;
		w.m_vecSide = side;
		w.m_flAnchorTime = gpGlobals->curtime;
		Apply( w );
		return true;
	}
	bool IsNear( const Vector &pos, float dist ) const
	{
		for ( int i = 0; i < m_Wakes.Count(); ++i )
		{
			if ( ( m_Wakes[i].m_vecPos - pos ).Length2D() < dist )
				return true;
		}
		return false;
	}
private:
	void Apply( BS2WheelWake_t &w )
	{
		Vector travel, right;
		BS2WakeFrame( w.m_vecVel, w.m_vecSide, travel, right );
		w.m_pEffect->SetSortOrigin( w.m_vecPos );
		w.m_pEffect->SetControlPoint( 0, w.m_vecPos );
		w.m_pEffect->SetControlPointOrientation( 0, travel, right, Vector( 0, 0, 1 ) );
		w.m_pEffect->SetControlPoint( 1, Vector( w.m_flIntensity, 0, 0 ) );
	}
	void StopAll()
	{
		for ( int i = 0; i < m_Wakes.Count(); ++i )
		{
			if ( m_Wakes[i].m_pEffect.IsValid() )
				m_Wakes[i].m_pEffect->StopEmission();
		}
		m_Wakes.RemoveAll();
	}
	// Water drive sound: drive loop rises with speed, eases down when slowing, drops fast when braking;
	// hold loop fades in after holding near top speed.
	void UpdateSound( C_BaseEntity *pWaterVehicle, float frametime )
	{
		if ( pWaterVehicle )
			m_hSoundVehicle = pWaterVehicle;
		C_BaseEntity *pVeh = m_hSoundVehicle.Get();
		if ( !pVeh )
		{
			StopSound();
			return;
		}
		const float dt = MAX( frametime, 0.001f );
		Vector v;
		pVeh->EstimateAbsVelocity( v );
		const float speed = v.Length2D();
		const float decel = ( m_flPrevSpeed - speed ) / dt;
		m_flPrevSpeed = speed;
		m_flDecel += ( decel - m_flDecel ) * MIN( 1.0f, dt * 25.0f );
		const float top = MAX( 100.0f, bs2_wheel_wake_topspeed.GetFloat() );
		const float s01 = RemapValClamped( speed, 30.0f, top, 0.0f, 1.0f );
		const bool inWater = pWaterVehicle != NULL;

		if ( inWater && speed >= top * 0.9f )
			m_flTopTime += dt;
		else
			m_flTopTime = 0.0f;
		const bool holding = m_flTopTime > 0.6f;
		// braking latches for 0.6 s so the drop is one quick fall, not a flicker
		if ( m_flDecel > bs2_wheel_wake_brake_decel.GetFloat() )
			m_flBrakeTime = 0.6f;
		else
			m_flBrakeTime = MAX( 0.0f, m_flBrakeTime - dt );
		const bool braking = m_flBrakeTime > 0.0f;

		float driveTarget = inWater ? ( 0.25f + 0.75f * s01 ) : 0.0f;
		if ( holding )
			driveTarget *= 0.55f;
		if ( braking )
			driveTarget *= 0.2f;              // quick drop when the player brakes
		const float holdTarget = holding ? 0.85f : 0.0f;

		// seconds for a full 0 -> 1 swing
		const float driveRate = 1.0f / ( driveTarget > m_flDriveVol ? ( braking ? 9.0f : 1.0f ) : ( braking ? 0.15f : 0.8f ) );
		const float holdRate = 1.0f / ( holdTarget > m_flHoldVol ? 0.7f : ( braking ? 0.1f : 0.45f ) );
		m_flDriveVol = Approach( driveTarget, m_flDriveVol, driveRate * dt );
		m_flHoldVol = Approach( holdTarget, m_flHoldVol, holdRate * dt );

		const float master = clamp( bs2_wheel_wake_volume.GetFloat(), 0.0f, 2.0f );
		if ( m_flDriveVol > 0.001f || m_flHoldVol > 0.001f )
		{
			m_flSilentTime = 0.0f;
			if ( !m_pDrive )
			{
				CLocalPlayerFilter filter;
				m_pDrive = CSoundEnvelopeController::GetController().SoundCreate( filter, pVeh->entindex(), CHAN_STATIC, BS2_WATER_DRIVE_LOOP, ATTN_NORM );
				m_pHold = CSoundEnvelopeController::GetController().SoundCreate( filter, pVeh->entindex(), CHAN_STATIC, BS2_WATER_HOLD_LOOP, ATTN_NORM );
				if ( m_pDrive )
					CSoundEnvelopeController::GetController().Play( m_pDrive, 0.0f, 100 );
				if ( m_pHold )
					CSoundEnvelopeController::GetController().Play( m_pHold, 0.0f, 100 );
			}
			if ( m_pDrive )
			{
				CSoundEnvelopeController::GetController().SoundChangeVolume( m_pDrive, m_flDriveVol * master, 0.05f );
				CSoundEnvelopeController::GetController().SoundChangePitch( m_pDrive, 85.0f + 30.0f * s01, 0.1f );
			}
			if ( m_pHold )
			{
				CSoundEnvelopeController::GetController().SoundChangeVolume( m_pHold, m_flHoldVol * master, 0.05f );
				CSoundEnvelopeController::GetController().SoundChangePitch( m_pHold, 95.0f + 15.0f * s01, 0.1f );
			}
		}
		else
		{
			m_flSilentTime += dt;
			if ( m_flSilentTime > 1.0f )
				StopSound();
		}
		if ( bs2_wheel_wake_debug.GetBool() )
			Msg( "BS2 water sound: speed %.0f decel %.0f water %d brake %d hold %d drive %.2f hold %.2f\n", speed, m_flDecel, inWater, braking, holding, m_flDriveVol, m_flHoldVol );
	}
	void StopSound()
	{
		if ( m_pDrive )
			CSoundEnvelopeController::GetController().SoundDestroy( m_pDrive );
		if ( m_pHold )
			CSoundEnvelopeController::GetController().SoundDestroy( m_pHold );
		m_pDrive = m_pHold = NULL;
		m_flDriveVol = m_flHoldVol = 0.0f;
		m_flTopTime = 0.0f;
	}
	CUtlVector<BS2WheelWake_t> m_Wakes;
	EHANDLE m_hSoundVehicle;
	CSoundPatch *m_pDrive;
	CSoundPatch *m_pHold;
	float m_flDriveVol, m_flHoldVol, m_flPrevSpeed, m_flDecel, m_flTopTime, m_flSilentTime, m_flBrakeTime;
};
static CBS2WheelWakeSystem g_BS2WheelWakes;

bool BS2WheelWakeNear( const Vector &pos, float dist )
{
	return bs2_wheel_wake.GetBool() && g_BS2WheelWakes.IsNear( pos, dist );
}

void BS2WheelSprayCallback( const CEffectData &data )
{
	if ( !cl_show_splashes.GetBool() || data.m_flScale <= 0.0f )
		return;
	const float scale = clamp( data.m_flScale, 0.0f, 2.0f );
	if ( bs2_wheel_wake.GetBool() && g_BS2WheelWakes.Event( data, scale ) )
		return;
	if ( bs2_wheel_splash_style.GetInt() == 1 && BS2WheelWaterTrail( data, scale ) )
		return;
	const float density = clamp( bs2_wheel_splash_density.GetFloat(), 0.5f, 3.0f );
	// Legacy: stock jeep entry uses scale 10-12. Match it at normal driving speed.
	FX_WaterSplash( data.m_vOrigin, Vector(0,0,1), 11.0f * scale, density, false );
}
DECLARE_CLIENT_EFFECT( "BS2WheelSpray", BS2WheelSprayCallback );
