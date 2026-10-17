//========= Black Stasis 2 - motion-reactive fog ==============================
//
// First-person fog that reacts to how the player moves through it.
//
//  * Moving:  wisps spawn ahead of the camera and stay in world space, so
//             they stream past. Air near the body is pushed aside (parting)
//             and partly dragged along (entrainment).
//  * Stopping / handbrake: the air you were dragging keeps its momentum,
//             overtakes the camera and rolls into eddies ahead of you, then
//             settles. Extra "brake" wisps add a rolling vortex.
//  * Turning: the dragged air carries on straight and sheds off the outside
//             of the turn with a co-rotating swirl.
//
// One system serves both modes: the buggy (any IClientVehicle) and on foot,
// with separate tuning. Maps opt in like bs2_tiltshift: point_clientcommand
// "bs2_fogmotion_enable 1", and fog-zone triggers set bs2_fogmotion_intensity
// (it blends over bs2_fogmotion_fade seconds).
//
// Material: particle/bs2_fog/fog_wisps_motion (UnlitGeneric on the bs2_fog
// wisp sheet, written by tools/gen_bs2_fog.py). Frames are cut from the 4x4
// sheet the same way CHeliBlastCloud cuts its flipbook.
//=============================================================================

#include "cbase.h"
#include "particles_simple.h"
#include "particlemgr.h"
#include "particle_util.h"
#include "c_baseplayer.h"
#include "iclientvehicle.h"
#include "view.h"
#include "igamesystem.h"
#include "tier0/memdbgon.h"

#define BS2FM_FLAGS ( FCVAR_CLIENTDLL | FCVAR_SERVER_CAN_EXECUTE )

// Not archived: a map opts in explicitly (same convention as bs2_tiltshift).
static ConVar bs2_fogmotion_enable( "bs2_fogmotion_enable", "0", BS2FM_FLAGS, "Motion-reactive fog around the camera", true, 0, true, 1 );
static ConVar bs2_fogmotion_intensity( "bs2_fogmotion_intensity", "1", BS2FM_FLAGS, "Fog density at the camera; 0 = clear air. Set by fog-zone triggers", true, 0, true, 2 );
static ConVar bs2_fogmotion_fade( "bs2_fogmotion_fade", "1.5", BS2FM_FLAGS, "Seconds to blend to a new intensity", true, 0.05f, true, 10 );
static ConVar bs2_fogmotion_vehicle( "bs2_fogmotion_vehicle", "1", BS2FM_FLAGS, "Strength while driving", true, 0, true, 2 );
static ConVar bs2_fogmotion_foot( "bs2_fogmotion_foot", "0.35", BS2FM_FLAGS, "Strength on foot", true, 0, true, 2 );
static ConVar bs2_fogmotion_brake( "bs2_fogmotion_brake", "1", BS2FM_FLAGS, "Stop / handbrake swirl strength", true, 0, true, 3 );
static ConVar bs2_fogmotion_turn( "bs2_fogmotion_turn", "1", BS2FM_FLAGS, "Turn shear strength", true, 0, true, 3 );
static ConVar bs2_fogmotion_color( "bs2_fogmotion_color", "150 160 178", BS2FM_FLAGS, "Wisp colour R G B (a little brighter than the fog colour so it reads against it)" );
static ConVar bs2_fogmotion_alpha( "bs2_fogmotion_alpha", "110", BS2FM_FLAGS, "Peak wisp alpha 0-255 (the wisp texture is soft, so this is much fainter on screen)", true, 0, true, 255 );
static ConVar bs2_fogmotion_max( "bs2_fogmotion_max", "220", BS2FM_FLAGS, "Particle budget", true, 0, true, 600 );
// Thresholds, exposed so they can be tuned live with bs2_fogmotion_debug 1.
static ConVar bs2_fogmotion_stop_vehicle( "bs2_fogmotion_stop_vehicle", "420", BS2FM_FLAGS, "Buggy deceleration (u/s^2) that counts as a hard stop / handbrake" );
static ConVar bs2_fogmotion_stop_foot( "bs2_fogmotion_stop_foot", "550", BS2FM_FLAGS, "On-foot deceleration (u/s^2) that counts as stopping" );
static ConVar bs2_fogmotion_turnrate_vehicle( "bs2_fogmotion_turnrate_vehicle", "45", BS2FM_FLAGS, "Buggy yaw rate (deg/s) where turn shear starts" );
static ConVar bs2_fogmotion_turnrate_foot( "bs2_fogmotion_turnrate_foot", "240", BS2FM_FLAGS, "View yaw rate (deg/s) where the on-foot twirl starts" );
static ConVar bs2_fogmotion_debug( "bs2_fogmotion_debug", "0", FCVAR_CLIENTDLL, "1 = on-screen readout, 2 = log stop/turn events to the console (for captures), 3 = both" );

//-----------------------------------------------------------------------------
// Tuning per mode. Units are Hammer units, seconds and degrees.
//-----------------------------------------------------------------------------
struct BS2FogMotionTuning
{
	float wakeSpacing;			// units travelled per wake wisp (at strength 1)
	float aheadMin, aheadMax;	// wake spawn distance ahead of the eye
	float sideMax;				// lateral spread
	float downMin, upMax;		// vertical spread relative to the eye
	float sizeMin, sizeMax;		// sprite half-size at spawn
	float pushRadius;			// how far the moving body disturbs the air
	float entrain;				// share of body velocity given to nearby air
	float part;					// sideways parting speed, share of body speed
	float stopMinSpeed;			// speed needed before a stop counts
	float stopDecel;			// deceleration (u/s^2) that counts as a hard stop
	float turnRate;				// yaw rate (deg/s) where turn shear starts
	float nearMin, nearMax;		// fade sprites this close to the eye
	float swirlScale;			// eddy speed multiplier
	int   burstCount;			// wisps in the initial stop burst
	float lifeMin, lifeMax;
};

static const BS2FogMotionTuning s_TuneVehicle =
{
	70.0f,  260.0f, 720.0f,  300.0f,  -50.0f, 90.0f,  70.0f, 150.0f,
	240.0f, 0.55f, 0.35f,  260.0f, 420.0f, 45.0f,  28.0f, 120.0f,  1.0f, 26,  2.4f, 3.8f
};

static const BS2FogMotionTuning s_TuneFoot =
{
	46.0f,  90.0f, 340.0f,  170.0f,  -50.0f, 30.0f,  34.0f, 80.0f,
	90.0f,  0.35f, 0.25f,  160.0f, 550.0f, 240.0f,  14.0f, 64.0f,  0.5f, 10,  3.0f, 5.0f
};

enum BS2FogWispKind { WISP_WAKE = 0, WISP_BURST, WISP_SHEAR };

//-----------------------------------------------------------------------------
// The particle. Sizes are floats (SimpleParticle's bytes cap at 255).
//
// IMPORTANT: CParticleEffectBinding::AddParticle silently returns NULL for any
// particle struct larger than PARTICLE_SIZE (96 bytes, particlemgr.cpp); the
// Assert only fires in debug builds. The first version was 124 bytes, so every
// wisp was rejected and nothing ever drew. Keep this packed and <= 96.
//-----------------------------------------------------------------------------
#define BS2FM_PARTICLE_SIZE_LIMIT 96	// must match PARTICLE_SIZE in particlemgr.cpp

struct BS2FogWisp : public Particle
{
	Vector	m_vecVelocity;
	Vector	m_vecSwirlCenter;
	Vector	m_vecSwirlCenterVel;
	float	m_flSwirl;			// tangential eddy speed, decays
	float	m_flLifetime;
	float	m_flDieTime;
	float	m_flSize0, m_flSize1;
	unsigned char m_uchAlpha;		// peak alpha 0-255
	unsigned char m_uchDrag;		// drag in 1/100 s^-1
	signed char   m_chRollDelta;	// deg/s
	unsigned char m_uchRoll0;		// start roll, 256 = 360 deg
	unsigned char m_uchNearMin, m_uchNearMax;	// fade sprites this close to the eye (units)
	unsigned char m_uchFadeIn;		// fade-in time in 1/100 s
	unsigned char m_uchColor[3];
	unsigned char m_uchFrame;		// 0-15 on the 4x4 wisp sheet
	signed char   m_chSwirlAxis[3];	// unit swirl axis * 127

	void SetSwirlAxis( const Vector &a )
	{
		for ( int i = 0; i < 3; ++i )
			m_chSwirlAxis[i] = (signed char)clamp( (int)( a[i] * 127.0f ), -127, 127 );
	}
	Vector SwirlAxis() const { return Vector( m_chSwirlAxis[0], m_chSwirlAxis[1], m_chSwirlAxis[2] ) * ( 1.0f / 127.0f ); }
	void SetDrag( float d )		{ m_uchDrag = (unsigned char)clamp( (int)( d * 100.0f + 0.5f ), 0, 255 ); }
	float Drag() const			{ return m_uchDrag * 0.01f; }
	void SetFadeIn( float f )	{ m_uchFadeIn = (unsigned char)clamp( (int)( f * 100.0f + 0.5f ), 1, 255 ); }
	float FadeIn() const		{ return m_uchFadeIn * 0.01f; }
	void SetAlpha( float a )	{ m_uchAlpha = (unsigned char)clamp( (int)( a * 255.0f + 0.5f ), 0, 255 ); }
	float Alpha() const			{ return m_uchAlpha * ( 1.0f / 255.0f ); }
	float Roll() const			{ return m_uchRoll0 * ( 360.0f / 256.0f ) + m_chRollDelta * m_flLifetime; }
};
COMPILE_TIME_ASSERT( sizeof( BS2FogWisp ) <= BS2FM_PARTICLE_SIZE_LIMIT );

//-----------------------------------------------------------------------------
// What the camera's body is doing this frame (shared with the emitter).
//-----------------------------------------------------------------------------
struct BS2FogMotionState
{
	Vector	eye;
	Vector	vel;			// smoothed body velocity
	Vector	dir;			// direction of travel (or view forward when slow)
	Vector	right;
	float	speed;
	float	strength;		// intensity * mode strength (drives how many wisps)
	float	intensity;		// zone intensity alone (drives how dense each wisp is)
	const BS2FogMotionTuning *tune;
	bool	active;
};
static BS2FogMotionState g_FogMotion;

//-----------------------------------------------------------------------------
// Emitter
//-----------------------------------------------------------------------------
class CBS2FogMotionEmitter : public CSimpleEmitter
{
public:
	CBS2FogMotionEmitter( const char *name ) : CSimpleEmitter( name )
	{
		for ( int f = 0; f < 16; ++f )
		{
			// 2048 sheet, 512 cells; half-texel inset avoids neighbour bleed.
			m_Frames[f].m_tCoordMins[0] = ( ( f % 4 ) * 512 + 0.5f ) / 2048.0f;
			m_Frames[f].m_tCoordMins[1] = ( ( f / 4 ) * 512 + 0.5f ) / 2048.0f;
			m_Frames[f].m_tCoordMaxs[0] = ( ( f % 4 + 1 ) * 512 - 0.5f ) / 2048.0f;
			m_Frames[f].m_tCoordMaxs[1] = ( ( f / 4 + 1 ) * 512 - 0.5f ) / 2048.0f;
		}
	}

	static CSmartPtr<CBS2FogMotionEmitter> Create( const char *name )
	{
		CBS2FogMotionEmitter *p = new CBS2FogMotionEmitter( name );
		p->SetDynamicallyAllocated( true );
		return p;
	}

	BS2FogWisp *AddWisp( const Vector &pos )
	{
		if ( m_hMaterial == INVALID_MATERIAL_HANDLE )
			m_hMaterial = GetPMaterial( "particle/bs2_fog/fog_wisps_motion" );
		BS2FogWisp *p = (BS2FogWisp *)AddParticle( sizeof( BS2FogWisp ), m_hMaterial, pos );
		if ( !p )
			return NULL;
		p->m_vecVelocity.Init();
		p->m_vecSwirlCenter = pos;
		p->m_vecSwirlCenterVel.Init();
		p->SetSwirlAxis( Vector( 0, 0, 1 ) );
		p->m_flSwirl = 0;
		p->SetDrag( 1.0f );
		p->m_flLifetime = 0;
		p->m_flDieTime = 3.0f;
		p->m_flSize0 = p->m_flSize1 = 64;
		p->SetAlpha( 0.1f );
		p->m_uchRoll0 = (unsigned char)random->RandomInt( 0, 255 );
		p->m_chRollDelta = (signed char)random->RandomInt( -6, 6 );
		p->m_uchNearMin = 16;
		p->m_uchNearMax = 64;
		p->SetFadeIn( 0.45f );
		p->m_uchColor[0] = p->m_uchColor[1] = p->m_uchColor[2] = 110;
		p->m_uchFrame = 0;
		return p;
	}

	virtual void SimulateParticles( CParticleSimulateIterator *it )
	{
		const float dt = it->GetTimeDelta();
		const BS2FogMotionState &s = g_FogMotion;
		const bool push = s.active && s.tune && s.speed > 15.0f;
		const float radius = push ? s.tune->pushRadius : 1.0f;

		BS2FogWisp *p = (BS2FogWisp *)it->GetFirst();
		while ( p )
		{
			// The moving body pushes nearby air aside and drags some of it along.
			// This is what makes a sudden stop throw the fog forward past you.
			if ( push )
			{
				Vector rel = p->m_Pos - s.eye;
				const float dist = rel.Length();
				if ( dist < radius )
				{
					float f = 1.0f - dist / radius;
					f *= f;
					Vector lateral = rel - s.dir * DotProduct( rel, s.dir );
					if ( lateral.LengthSqr() < 1.0f )
						lateral = s.right * ( ( p->m_uchFrame & 1 ) ? 1.0f : -1.0f );
					VectorNormalize( lateral );
					const Vector target = s.vel * s.tune->entrain + lateral * ( s.speed * s.tune->part );
					p->m_vecVelocity += ( target - p->m_vecVelocity ) * MIN( 1.0f, f * dt * 3.5f );
				}
			}

			// Still air: momentum bleeds away. Vertical motion settles faster so
			// fog stays low instead of floating up like smoke.
			const float drag = expf( -dt * p->Drag() );
			p->m_vecVelocity *= drag;
			p->m_vecVelocity.z *= expf( -dt * 0.6f );
			p->m_Pos += p->m_vecVelocity * dt;

			// Eddy: rotate about a drifting centre; slowly spirals out and dies.
			if ( p->m_flSwirl > 1.0f )
			{
				p->m_vecSwirlCenterVel *= drag;
				p->m_vecSwirlCenter += p->m_vecSwirlCenterVel * dt;
				Vector r = p->m_Pos - p->m_vecSwirlCenter;
				Vector tangent = CrossProduct( p->SwirlAxis(), r );
				const float len = tangent.Length();
				if ( len > 1.0f )
					p->m_Pos += tangent * ( p->m_flSwirl * dt / len );
				p->m_flSwirl *= expf( -dt * 0.8f );
			}

			p->m_flLifetime += dt;
			if ( p->m_flLifetime >= p->m_flDieTime )
				it->RemoveParticle( p );

			p = (BS2FogWisp *)it->GetNext();
		}
	}

	virtual void RenderParticles( CParticleRenderIterator *it )
	{
		const BS2FogWisp *p = (const BS2FogWisp *)it->GetFirst();
		while ( p )
		{
			Vector tPos;
			TransformParticle( ParticleMgr()->GetModelView(), p->m_Pos, tPos );

			const float age = clamp( p->m_flLifetime / p->m_flDieTime, 0.0f, 1.0f );
			const float fadeIn = clamp( p->m_flLifetime / p->FadeIn(), 0.0f, 1.0f );
			const float fadeOut = 1.0f - age * age;
			const float alpha = p->Alpha() * fadeIn * fadeOut *
				GetAlphaDistanceFade( tPos, p->m_uchNearMin, p->m_uchNearMax );

			if ( alpha > 0.002f )
			{
				ParticleDraw *draw = it->GetParticleDraw();
				CParticleSubTexture *original = draw->m_pSubTexture;
				draw->m_pSubTexture = &m_Frames[p->m_uchFrame & 15];
				const Vector color( p->m_uchColor[0] / 255.0f, p->m_uchColor[1] / 255.0f, p->m_uchColor[2] / 255.0f );
				RenderParticle_ColorSizeAngle( draw, tPos, color, alpha,
					p->m_flSize0 + ( p->m_flSize1 - p->m_flSize0 ) * age, DEG2RAD( p->Roll() ) );
				draw->m_pSubTexture = original;
			}
			p = (const BS2FogWisp *)it->GetNext( tPos.z );
		}
	}

private:
	PMaterialHandle m_hMaterial = INVALID_MATERIAL_HANDLE;
	CParticleSubTexture m_Frames[16];
};

//-----------------------------------------------------------------------------
// Per-frame tracker: measures motion, detects stops and turns, spawns wisps.
//-----------------------------------------------------------------------------
class CBS2FogMotionSystem : public CAutoGameSystemPerFrame
{
public:
	CBS2FogMotionSystem() : CAutoGameSystemPerFrame( "CBS2FogMotionSystem" ) { Reset(); m_flIntensity = 0; }

	virtual void LevelInitPostEntity() { Reset(); m_flIntensity = 0; }

	virtual void LevelShutdownPreEntity()
	{
		// Must let go before ParticleMgr()->RemoveAllEffects() runs.
		m_pEmitter = NULL;
		g_FogMotion.active = false;
		Reset();
	}

	virtual void Update( float frametime )
	{
		g_FogMotion.active = false;
		const float dt = frametime;
		if ( dt <= 0.0f || dt > 0.25f || !engine->IsInGame() )
			return;

		// Blend towards the zone intensity (0 when disabled) so entering and
		// leaving fog never pops.
		const float target = bs2_fogmotion_enable.GetBool() ? bs2_fogmotion_intensity.GetFloat() : 0.0f;
		m_flIntensity = Approach( target, m_flIntensity, dt / bs2_fogmotion_fade.GetFloat() * MAX( 1.0f, target ) );
		if ( m_flIntensity <= 0.001f )
		{
			Reset();
			return;
		}

		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		if ( !player || !player->IsAlive() )
		{
			Reset();
			return;
		}

		IClientVehicle *vehicle = player->GetVehicle();
		C_BaseEntity *body = vehicle ? vehicle->GetVehicleEnt() : NULL;
		const bool inVehicle = body != NULL;
		if ( !body )
			body = player;

		m_Tune = inVehicle ? s_TuneVehicle : s_TuneFoot;
		m_Tune.stopDecel = MAX( 50.0f, inVehicle ? bs2_fogmotion_stop_vehicle.GetFloat() : bs2_fogmotion_stop_foot.GetFloat() );
		m_Tune.turnRate = MAX( 10.0f, inVehicle ? bs2_fogmotion_turnrate_vehicle.GetFloat() : bs2_fogmotion_turnrate_foot.GetFloat() );
		const BS2FogMotionTuning &tune = m_Tune;
		const float modeStrength = inVehicle ? bs2_fogmotion_vehicle.GetFloat() : bs2_fogmotion_foot.GetFloat();

		// Velocity from position change (client vehicle velocity can jitter).
		const Vector pos = body->GetAbsOrigin();
		const float yaw = inVehicle ? body->GetAbsAngles()[YAW] : MainViewAngles()[YAW];
		if ( !m_bValid || inVehicle != m_bInVehicle )
		{
			Reset();
			m_bValid = true;
			m_bInVehicle = inVehicle;
			m_vecLastPos = pos;
			m_flLastYaw = yaw;
			m_flSettle = 0.5f;		// ignore events right after a mode change
			return;
		}

		Vector raw = ( pos - m_vecLastPos ) / dt;
		m_vecLastPos = pos;
		if ( raw.Length() > 4000.0f )	// teleport, level transition, respawn
		{
			m_vecVel.Init();
			m_flSettle = 0.5f;
			return;
		}

		const Vector prevVel = m_vecVel;
		m_vecVel += ( raw - m_vecVel ) * ( 1.0f - expf( -dt * 10.0f ) );
		const float prevSpeed = prevVel.Length();
		const float speed = m_vecVel.Length();

		// Acceleration along the previous direction of travel. Negative = slowing.
		float accelAlong = 0.0f;
		if ( prevSpeed > 1.0f )
			accelAlong = DotProduct( ( m_vecVel - prevVel ) / dt, prevVel / prevSpeed );
		m_flDecel += ( MAX( -accelAlong, 0.0f ) - m_flDecel ) * ( 1.0f - expf( -dt * 8.0f ) );

		// Yaw rate (deg/s). The vehicle's model yaw offset cancels in the difference.
		const float rawYawRate = AngleDiff( yaw, m_flLastYaw ) / dt;
		m_flLastYaw = yaw;
		m_flYawRate += ( rawYawRate - m_flYawRate ) * ( 1.0f - expf( -dt * 8.0f ) );

		m_flSettle = MAX( 0.0f, m_flSettle - dt );

		// Shared state for the emitter's push/entrainment.
		BS2FogMotionState &s = g_FogMotion;
		s.eye = MainViewOrigin();
		s.vel = m_vecVel;
		s.speed = speed;
		s.tune = &tune;
		s.strength = m_flIntensity * modeStrength;
		s.intensity = m_flIntensity;
		if ( speed > 30.0f )
			s.dir = m_vecVel / speed;
		else
		{
			s.dir = MainViewForward();
			s.dir.z = 0;
			if ( s.dir.LengthSqr() < 0.01f )
				s.dir.Init( 1, 0, 0 );
			VectorNormalize( s.dir );
		}
		s.right = CrossProduct( s.dir, Vector( 0, 0, 1 ) );
		if ( s.right.LengthSqr() < 0.01f )
			s.right = MainViewRight();
		VectorNormalize( s.right );
		s.active = s.strength > 0.001f;

		if ( !s.active )
			return;

		if ( !m_pEmitter )
		{
			m_pEmitter = CBS2FogMotionEmitter::Create( "bs2_fogmotion" );
			if ( !m_pEmitter )
				return;
			m_pEmitter->SetParticleCullRadius( 400.0f );
		}
		m_pEmitter->SetSortOrigin( s.eye );

		const int budget = bs2_fogmotion_max.GetInt();
		float r = 100, g = 112, b = 132;
		sscanf( bs2_fogmotion_color.GetString(), "%f %f %f", &r, &g, &b );
		m_vecColor.Init( r, g, b );

		// 1) Wake: one wisp per wakeSpacing units travelled.
		if ( speed > 40.0f )
		{
			m_flWakeAccum = MIN( m_flWakeAccum + speed * dt * s.strength / tune.wakeSpacing, 6.0f );
			while ( m_flWakeAccum >= 1.0f && Live() < budget )
			{
				SpawnWake( s );
				m_flWakeAccum -= 1.0f;
			}
		}

		// 2) Stop / handbrake: a burst on onset, then a stream while the hard
		//    deceleration lasts (a long handbrake slide keeps churning).
		const bool hardStop = m_flSettle <= 0.0f && prevSpeed > tune.stopMinSpeed * 0.6f && m_flDecel > tune.stopDecel;
		if ( hardStop && bs2_fogmotion_brake.GetFloat() > 0.0f )
		{
			const float strength = clamp( m_flDecel / tune.stopDecel - 0.6f, 0.3f, 2.0f ) *
				clamp( MAX( prevSpeed, m_flStopEntrySpeed ) / ( tune.stopMinSpeed * 2.0f ), 0.4f, 1.6f ) *
				bs2_fogmotion_brake.GetFloat() * s.strength;
			if ( !m_bStopping )
			{
				if ( prevSpeed > tune.stopMinSpeed )
				{
					m_bStopping = true;
					m_flStopEntrySpeed = prevSpeed;
					m_vecStopVel = prevVel;
					const int n = (int)( tune.burstCount * strength );
					for ( int i = 0; i < n && Live() < budget; ++i )
						SpawnBurst( s, m_vecStopVel, strength );
					if ( bs2_fogmotion_debug.GetInt() & 2 )
						Msg( "BS2_FOGMOTION stop: speed %.0f decel %.0f strength %.2f wisps %d\n", prevSpeed, m_flDecel, strength, n );
				}
			}
			else
			{
				m_flBurstAccum = MIN( m_flBurstAccum + dt * tune.burstCount * 1.5f * strength, 6.0f );
				while ( m_flBurstAccum >= 1.0f && Live() < budget )
				{
					SpawnBurst( s, m_vecStopVel, strength * 0.7f );
					m_flBurstAccum -= 1.0f;
				}
			}
		}
		else if ( m_flDecel < tune.stopDecel * 0.5f )
		{
			m_bStopping = false;
			m_flStopEntrySpeed = 0;
			m_flBurstAccum = 0;
		}

		// 3) Turn shear: air dragged along keeps going straight and peels off
		//    the outside of the turn. On foot this is a gentle twirl when
		//    spinning the view.
		const float turnExcess = ( fabsf( m_flYawRate ) - tune.turnRate ) / tune.turnRate;
		if ( m_flSettle <= 0.0f && turnExcess > 0.0f && bs2_fogmotion_turn.GetFloat() > 0.0f )
		{
			const float speedFactor = inVehicle ? clamp( speed / 400.0f, 0.0f, 1.5f ) : 1.0f;
			const float rate = MIN( turnExcess, 2.0f ) * tune.burstCount * 1.2f * speedFactor *
				bs2_fogmotion_turn.GetFloat() * s.strength;
			m_flShearAccum = MIN( m_flShearAccum + rate * dt, 6.0f );
			while ( m_flShearAccum >= 1.0f && Live() < budget )
			{
				SpawnShear( s, m_flYawRate );
				m_flShearAccum -= 1.0f;
			}
		}

		if ( ( bs2_fogmotion_debug.GetInt() & 2 ) && turnExcess > 0.0f && !m_bTurnLogged )
			Msg( "BS2_FOGMOTION turn: yaw %.0f deg/s speed %.0f %s\n", m_flYawRate, speed, inVehicle ? "vehicle" : "foot" );
		m_bTurnLogged = turnExcess > 0.0f;

		if ( bs2_fogmotion_debug.GetInt() & 1 )
		{
			engine->Con_NPrintf( 0, "fogmotion %s  intensity %.2f  strength %.2f", inVehicle ? "VEHICLE" : "FOOT", m_flIntensity, s.strength );
			engine->Con_NPrintf( 1, "speed %4.0f  decel %5.0f (stop at %.0f)  yaw %5.0f deg/s (turn at %.0f)", speed, m_flDecel, tune.stopDecel, m_flYawRate, tune.turnRate );
			engine->Con_NPrintf( 2, "wisps %d / %d  rejected %d  %s", Live(), budget, m_nRejected, m_bStopping ? "STOP SWIRL" : ( turnExcess > 0 ? "TURN SHEAR" : "" ) );
		}
	}

private:
	void Reset()
	{
		m_bValid = false;
		m_bInVehicle = false;
		m_vecVel.Init();
		m_vecStopVel.Init();
		m_flDecel = m_flYawRate = 0;
		m_flWakeAccum = m_flBurstAccum = m_flShearAccum = 0;
		m_bStopping = false;
		m_bTurnLogged = false;
		m_flStopEntrySpeed = 0;
		m_flSettle = 0;
	}

	int Live()
	{
		return !m_pEmitter ? 0 : m_pEmitter->GetBinding().GetNumActiveParticles();
	}

	// Keep wisps out of walls and off the ground (UnlitGeneric sprites would
	// show hard lines where they cut through brushes).
	bool PlaceWisp( const Vector &eye, Vector &pos, float size )
	{
		trace_t tr;
		UTIL_TraceLine( eye, pos, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.startsolid )
			return false;
		if ( tr.fraction < 1.0f )
		{
			if ( tr.fraction < 0.2f )
				return false;
			Vector back = eye - pos;
			VectorNormalize( back );
			pos = tr.endpos + back * ( size * 0.5f );
		}
		UTIL_TraceLine( pos, pos - Vector( 0, 0, 160 ), MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction < 1.0f && pos.z - tr.endpos.z < size * 0.35f )
			pos.z = tr.endpos.z + size * 0.35f;
		return true;
	}

	BS2FogWisp *NewWisp( const BS2FogMotionState &s, Vector pos, float sizeScale, float lifeScale, float alphaScale, int frameMin, int frameMax )
	{
		const BS2FogMotionTuning &t = *s.tune;
		const float size = random->RandomFloat( t.sizeMin, t.sizeMax ) * sizeScale;
		if ( !PlaceWisp( s.eye, pos, size ) )
			return NULL;
		BS2FogWisp *p = m_pEmitter->AddWisp( pos );
		if ( !p )
		{
			++m_nRejected;		// engine refused the particle (size limit / global particle cap)
			return NULL;
		}
		p->m_flSize0 = size;
		p->m_flSize1 = size * random->RandomFloat( 1.35f, 1.7f );
		p->m_flDieTime = random->RandomFloat( t.lifeMin, t.lifeMax ) * lifeScale;
		p->SetAlpha( clamp( bs2_fogmotion_alpha.GetFloat() / 255.0f * alphaScale * MIN( s.intensity, 1.5f ), 0.0f, 1.0f ) );
		p->m_uchNearMin = (unsigned char)clamp( t.nearMin, 0.0f, 254.0f );
		p->m_uchNearMax = (unsigned char)clamp( t.nearMax, t.nearMin + 1.0f, 255.0f );
		p->m_uchFrame = (unsigned char)random->RandomInt( frameMin, frameMax );
		const float tint = random->RandomFloat( 0.88f, 1.1f );
		p->m_uchColor[0] = (unsigned char)clamp( m_vecColor.x * tint, 0.0f, 255.0f );
		p->m_uchColor[1] = (unsigned char)clamp( m_vecColor.y * tint, 0.0f, 255.0f );
		p->m_uchColor[2] = (unsigned char)clamp( m_vecColor.z * tint, 0.0f, 255.0f );
		return p;
	}

	// Ahead of the camera, nearly still in world space: they stream past.
	void SpawnWake( const BS2FogMotionState &s )
	{
		const BS2FogMotionTuning &t = *s.tune;
		const Vector pos = s.eye + s.dir * random->RandomFloat( t.aheadMin, t.aheadMax ) +
			s.right * random->RandomFloat( -t.sideMax, t.sideMax ) +
			Vector( 0, 0, random->RandomFloat( t.downMin, t.upMax ) );
		BS2FogWisp *p = NewWisp( s, pos, 1.0f, 1.0f, 1.0f, 0, 7 );
		if ( !p )
			return;
		p->m_vecVelocity.Init( random->RandomFloat( -6, 6 ), random->RandomFloat( -6, 6 ), random->RandomFloat( -1, 1 ) );
		p->SetDrag( random->RandomFloat( 0.8f, 1.4f ) );
	}

	// Air that was travelling with you: starts behind/around the camera with
	// your old velocity, overtakes you and rolls into an eddy ahead.
	void SpawnBurst( const BS2FogMotionState &s, const Vector &oldVel, float strength )
	{
		const BS2FogMotionTuning &t = *s.tune;
		const float oldSpeed = oldVel.Length();
		Vector dir = oldSpeed > 1.0f ? oldVel / oldSpeed : s.dir;
		Vector right = CrossProduct( dir, Vector( 0, 0, 1 ) );
		if ( right.LengthSqr() < 0.01f )
			right = s.right;
		VectorNormalize( right );

		const Vector pos = s.eye - dir * random->RandomFloat( -40.0f, t.pushRadius ) +
			right * random->RandomFloat( -t.sideMax * 0.6f, t.sideMax * 0.6f ) +
			Vector( 0, 0, random->RandomFloat( t.downMin * 0.6f, t.upMax ) );
		BS2FogWisp *p = NewWisp( s, pos, 1.25f, 1.3f, 1.3f, random->RandomInt( 0, 2 ) ? 12 : 0, random->RandomInt( 0, 2 ) ? 15 : 7 );
		if ( !p )
			return;
		p->m_vecVelocity = oldVel * random->RandomFloat( 0.55f, 1.0f ) +
			Vector( random->RandomFloat( -25, 25 ), random->RandomFloat( -25, 25 ), random->RandomFloat( -8, 15 ) );
		p->SetDrag( random->RandomFloat( 1.0f, 1.7f ) );
		p->SetFadeIn( 0.25f );

		// 60% roll forward over the bonnet (axis = right), the rest swirl about
		// the vertical, following the turn if there is one.
		if ( random->RandomInt( 0, 9 ) < 6 )
			p->SetSwirlAxis( right );
		else
			p->SetSwirlAxis( Vector( 0, 0, ( m_flYawRate >= 0 ? 1.0f : -1.0f ) * ( fabsf( m_flYawRate ) > 20 ? 1.0f : ( random->RandomInt( 0, 1 ) ? 1.0f : -1.0f ) ) ) );
		p->m_vecSwirlCenter = s.eye + dir * random->RandomFloat( 80.0f, t.pushRadius ) + Vector( 0, 0, random->RandomFloat( -10, 40 ) );
		p->m_vecSwirlCenterVel = oldVel * 0.35f;
		p->m_flSwirl = random->RandomFloat( 70, 170 ) * clamp( strength, 0.3f, 2.0f ) * t.swirlScale;
	}

	// Shed off the outside of a turn, co-rotating.
	void SpawnShear( const BS2FogMotionState &s, float yawRate )
	{
		const BS2FogMotionTuning &t = *s.tune;
		// Source yaw increases turning left; the outside of a left turn is right.
		const Vector outside = s.right * ( yawRate > 0 ? 1.0f : -1.0f );
		const Vector pos = s.eye + outside * random->RandomFloat( 40.0f, t.sideMax * 0.8f ) +
			s.dir * random->RandomFloat( -80.0f, t.aheadMin ) +
			Vector( 0, 0, random->RandomFloat( t.downMin, t.upMax * 0.7f ) );
		BS2FogWisp *p = NewWisp( s, pos, 1.1f, 1.0f, 1.1f, 0, 7 );
		if ( !p )
			return;
		p->m_vecVelocity = s.vel * random->RandomFloat( 0.5f, 0.9f ) + outside * random->RandomFloat( 20, 60 );
		p->SetDrag( random->RandomFloat( 0.9f, 1.5f ) );
		p->SetSwirlAxis( Vector( 0, 0, yawRate > 0 ? 1.0f : -1.0f ) );
		p->m_vecSwirlCenter = s.eye + outside * 60.0f;
		p->m_vecSwirlCenterVel = s.vel * 0.3f;
		p->m_flSwirl = random->RandomFloat( 50, 110 ) * t.swirlScale;
	}

	CSmartPtr<CBS2FogMotionEmitter> m_pEmitter;
	BS2FogMotionTuning m_Tune;
	Vector	m_vecLastPos, m_vecVel, m_vecStopVel, m_vecColor;
	float	m_flLastYaw, m_flYawRate, m_flDecel;
	float	m_flWakeAccum, m_flBurstAccum, m_flShearAccum;
	float	m_flStopEntrySpeed, m_flSettle, m_flIntensity;
	bool	m_bValid, m_bInVehicle, m_bStopping, m_bTurnLogged;
	int		m_nRejected = 0;
};

static CBS2FogMotionSystem s_BS2FogMotion;
