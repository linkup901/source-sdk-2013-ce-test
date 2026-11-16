//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "c_physicsprop.h"
#include "c_physbox.h"
#include "c_props.h"
#include "view.h"

#define CPhysBox C_PhysBox
#define CPhysicsProp C_PhysicsProp


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( DynamicProp, DT_DynamicProp )

BEGIN_NETWORK_TABLE( CDynamicProp, DT_DynamicProp )
	RecvPropBool(RECVINFO(m_bUseHitboxesForRenderBox)),
END_NETWORK_TABLE()

C_DynamicProp::C_DynamicProp( void )
{
	m_iCachedFrameCount = -1;
	m_flBs2LastBoneTime = -1.0f;
}

//-----------------------------------------------------------------------------
// BS2 (Claude, 15 Nov 2026): animation LOD for prop_dynamic (the animated autumn trees).
// Bone setup is recomputed every frame near the view, then at r_bs2_animlod_hz_mid / _far Hz further away
// (the last pose is reused in between: trees sway slowly, so it is not visible). Only for props that have no
// move parent and are not moving. r_bs2_animlod 0 = stock behaviour.
//-----------------------------------------------------------------------------
static ConVar r_bs2_animlod( "r_bs2_animlod", "1", FCVAR_ARCHIVE, "BS2: update far animated prop_dynamic bones less often" );
static ConVar r_bs2_animlod_near( "r_bs2_animlod_near", "1024", FCVAR_ARCHIVE, "BS2: full-rate bone updates inside this distance" );
static ConVar r_bs2_animlod_hz_mid( "r_bs2_animlod_hz_mid", "30", FCVAR_ARCHIVE, "BS2: bone updates per second from near to 3x near" );
static ConVar r_bs2_animlod_hz_far( "r_bs2_animlod_hz_far", "12", FCVAR_ARCHIVE, "BS2: bone updates per second beyond 3x near" );

float C_DynamicProp::LastBoneChangedTime()
{
	if ( !r_bs2_animlod.GetBool() || GetMoveParent() || GetAbsVelocity().LengthSqr() > 1.0f || IsDormant() )
		return FLT_MAX;
	float flNear = r_bs2_animlod_near.GetFloat();
	float d2 = ( GetAbsOrigin() - MainViewOrigin() ).LengthSqr();
	if ( d2 < flNear * flNear )
		return FLT_MAX;
	float hz = ( d2 < 9.0f * flNear * flNear ) ? r_bs2_animlod_hz_mid.GetFloat() : r_bs2_animlod_hz_far.GetFloat();
	float now = gpGlobals->curtime;
	if ( hz <= 0.0f || m_flBs2LastBoneTime < 0.0f || now < m_flBs2LastBoneTime || now - m_flBs2LastBoneTime >= 1.0f / hz )
	{
		m_flBs2LastBoneTime = now;
		return FLT_MAX;	// recompute this frame
	}
	return -FLT_MAX;	// keep last frame's bones
}

C_DynamicProp::~C_DynamicProp( void )
{
}

bool C_DynamicProp::TestBoneFollowers( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	// UNDONE: There is no list of the bone followers that is networked to the client
	// so instead we do a search for solid stuff here.  This is not really great - a list would be
	// preferable.
	CBaseEntity	*pList[128];
	Vector mins, maxs;
	CollisionProp()->WorldSpaceAABB( &mins, &maxs );
	int count = UTIL_EntitiesInBox( pList, ARRAYSIZE(pList), mins, maxs, 0, PARTITION_CLIENT_SOLID_EDICTS );
	for ( int i = 0; i < count; i++ )
	{
		if ( pList[i]->GetOwnerEntity() == this )
		{
			if ( pList[i]->TestCollision(ray, fContentsMask, tr) )
			{
				return true;
			}
		}
	}
	return false;
}

bool C_DynamicProp::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( IsSolidFlagSet(FSOLID_NOT_SOLID) )
	{
		// if this entity is marked non-solid and custom test it must have bone followers
		if ( IsSolidFlagSet( FSOLID_CUSTOMBOXTEST ) && IsSolidFlagSet( FSOLID_CUSTOMRAYTEST ))
		{
			return TestBoneFollowers( ray, fContentsMask, tr );
		}
	}
	return BaseClass::TestCollision( ray, fContentsMask, tr );
}

//-----------------------------------------------------------------------------
// implements these so ragdolls can handle frustum culling & leaf visibility
//-----------------------------------------------------------------------------
void C_DynamicProp::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( m_bUseHitboxesForRenderBox )
	{
		if ( GetModel() )
		{
			studiohdr_t *pStudioHdr = modelinfo->GetStudiomodel( GetModel() );
			if ( !pStudioHdr || GetSequence() == -1 )
			{
				theMins = vec3_origin;
				theMaxs = vec3_origin;
				return;
			}

			// Only recompute if it's a new frame
			if ( gpGlobals->framecount != m_iCachedFrameCount )
			{
				ComputeEntitySpaceHitboxSurroundingBox( &m_vecCachedRenderMins, &m_vecCachedRenderMaxs );
				m_iCachedFrameCount = gpGlobals->framecount;
			}

			theMins = m_vecCachedRenderMins;
			theMaxs = m_vecCachedRenderMaxs;
			return;
		}
	}

	BaseClass::GetRenderBounds( theMins, theMaxs );
}

unsigned int C_DynamicProp::ComputeClientSideAnimationFlags()
{
	if ( GetSequence() != -1 )
	{
		CStudioHdr *pStudioHdr = GetModelPtr();
		if ( GetSequenceCycleRate(pStudioHdr, GetSequence()) != 0.0f )
		{
			return BaseClass::ComputeClientSideAnimationFlags();
		}
	}

	// no sequence or no cycle rate, don't do any per-frame calcs
	return 0;
}

// ------------------------------------------------------------------------------------------ //
// ------------------------------------------------------------------------------------------ //
class C_BasePropDoor : public C_DynamicProp
{
	DECLARE_CLASS( C_BasePropDoor, C_DynamicProp );
public:
	DECLARE_CLIENTCLASS();

	// constructor, destructor
	C_BasePropDoor( void );
	virtual ~C_BasePropDoor( void );

	virtual void OnDataChanged( DataUpdateType_t type );

	virtual bool TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );

private:
	C_BasePropDoor( const C_BasePropDoor & );
};

IMPLEMENT_CLIENTCLASS_DT(C_BasePropDoor, DT_BasePropDoor, CBasePropDoor)
END_RECV_TABLE()

C_BasePropDoor::C_BasePropDoor( void )
{
}

C_BasePropDoor::~C_BasePropDoor( void )
{
}

void C_BasePropDoor::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetSolid(SOLID_VPHYSICS);
		VPhysicsInitShadow( false, false );
	}
	else if ( VPhysicsGetObject() )
	{
		VPhysicsGetObject()->UpdateShadow( GetAbsOrigin(), GetAbsAngles(), false, TICK_INTERVAL );
	}
}

bool C_BasePropDoor::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	if ( !VPhysicsGetObject() )
		return false;

	CStudioHdr *pStudioHdr = GetModelPtr( );
	if (!pStudioHdr)
		return false;

	physcollision->TraceBox( ray, VPhysicsGetObject()->GetCollide(), GetAbsOrigin(), GetAbsAngles(), &trace );

	if ( trace.DidHit() )
	{
		trace.surface.surfaceProps = VPhysicsGetObject()->GetMaterialIndex();
		return true;
	}

	return false;
}

// ------------------------------------------------------------------------------------------ //
// Special version of func_physbox.
// ------------------------------------------------------------------------------------------ //
#ifndef _XBOX
class CPhysBoxMultiplayer : public CPhysBox, public IMultiplayerPhysics
{
public:
	DECLARE_CLASS( CPhysBoxMultiplayer, CPhysBox );

	virtual int GetMultiplayerPhysicsMode()
	{
		return m_iPhysicsMode;
	}

	virtual float GetMass()
	{
		return m_fMass;
	}

	virtual bool IsAsleep()
	{
		Assert ( 0 );
		return true;
	}

	CNetworkVar( int, m_iPhysicsMode );	// One of the PHYSICS_MULTIPLAYER_ defines.	
	CNetworkVar( float, m_fMass );

	DECLARE_CLIENTCLASS();
};

IMPLEMENT_CLIENTCLASS_DT( CPhysBoxMultiplayer, DT_PhysBoxMultiplayer, CPhysBoxMultiplayer )
	RecvPropInt( RECVINFO( m_iPhysicsMode ) ),
	RecvPropFloat( RECVINFO( m_fMass ) ),
END_RECV_TABLE()


class CPhysicsPropMultiplayer : public CPhysicsProp, public IMultiplayerPhysics
{
	DECLARE_CLASS( CPhysicsPropMultiplayer, CPhysicsProp );

	virtual int GetMultiplayerPhysicsMode()
	{
		Assert( m_iPhysicsMode != PHYSICS_MULTIPLAYER_CLIENTSIDE );
		Assert( m_iPhysicsMode != PHYSICS_MULTIPLAYER_AUTODETECT );
		return m_iPhysicsMode;
	}

	virtual float GetMass()
	{
		return m_fMass;
	}

	virtual bool IsAsleep()
	{
		return !m_bAwake;
	}

	virtual void ComputeWorldSpaceSurroundingBox( Vector *mins, Vector *maxs )
	{
		Assert( mins != NULL && maxs != NULL );
		if ( !mins || !maxs )
			return;

		// Take our saved collision bounds, and transform into world space
		TransformAABB( EntityToWorldTransform(), m_collisionMins, m_collisionMaxs, *mins, *maxs );
	}

	CNetworkVar( int, m_iPhysicsMode );	// One of the PHYSICS_MULTIPLAYER_ defines.	
	CNetworkVar( float, m_fMass );
	CNetworkVector( m_collisionMins );
	CNetworkVector( m_collisionMaxs );

	DECLARE_CLIENTCLASS();
};

IMPLEMENT_CLIENTCLASS_DT( CPhysicsPropMultiplayer, DT_PhysicsPropMultiplayer, CPhysicsPropMultiplayer )
	RecvPropInt( RECVINFO( m_iPhysicsMode ) ),
	RecvPropFloat( RECVINFO( m_fMass ) ),
	RecvPropVector( RECVINFO( m_collisionMins ) ),
	RecvPropVector( RECVINFO( m_collisionMaxs ) ),
END_RECV_TABLE()
#endif
