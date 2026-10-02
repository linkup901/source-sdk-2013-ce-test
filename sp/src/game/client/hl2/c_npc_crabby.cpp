//=============================================================================//
//
// Purpose: Client side of Crabby, the BS2 companion headcrab (npc_crabby).
//
// While Crabby rides the local player's shoulder in first person, it is drawn
// view-locked in the lower-left corner of the screen (like a viewmodel, but in
// the world pass), so it never lags behind fast mouse turns. In third person,
// mirrors seen by others, or when not riding it renders normally at its
// networked position.
//
//=============================================================================//

#include "cbase.h"
#include "c_ai_basenpc.h"
#include "view.h"
#include "c_baseplayer.h"
#include "clientleafsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Tunables for the first-person shoulder seat, relative to the eyes.
static ConVar cl_crabby_ride_forward( "cl_crabby_ride_forward", "24", FCVAR_ARCHIVE, "Crabby shoulder seat: distance in front of the eyes." );
static ConVar cl_crabby_ride_right( "cl_crabby_ride_right", "-15", FCVAR_ARCHIVE, "Crabby shoulder seat: sideways offset (negative is left)." );
static ConVar cl_crabby_ride_up( "cl_crabby_ride_up", "-17", FCVAR_ARCHIVE, "Crabby shoulder seat: height offset (negative is down)." );
static ConVar cl_crabby_ride_yaw( "cl_crabby_ride_yaw", "-28", FCVAR_ARCHIVE, "Crabby shoulder seat: turn toward the screen centre (degrees)." );
static ConVar cl_crabby_ride_pitch( "cl_crabby_ride_pitch", "10", FCVAR_ARCHIVE, "Crabby shoulder seat: tilt so its back and head face the camera (degrees)." );
static ConVar cl_crabby_ride_bob( "cl_crabby_ride_bob", "0.5", FCVAR_ARCHIVE, "Crabby shoulder seat: breathing bob amount (units)." );

class C_NPC_Crabby : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_NPC_Crabby, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_NPC_Crabby() : m_bRiding( false ), m_bWasRiding( false ) {}

	virtual void OnDataChanged( DataUpdateType_t type )
	{
		BaseClass::OnDataChanged( type );
		if ( m_bRiding != m_bWasRiding )
		{
			m_bWasRiding = m_bRiding;
			// Render bounds change size while riding; re-place in the leaf system.
			if ( RenderHandle() != INVALID_CLIENT_RENDER_HANDLE )
				ClientLeafSystem()->RenderableChanged( RenderHandle() );
		}
	}

	virtual const Vector &GetRenderOrigin( void )
	{
		if ( ViewLocked() )
		{
			UpdateSeat();
			return m_vecSeatOrigin;
		}
		return BaseClass::GetRenderOrigin();
	}

	virtual const QAngle &GetRenderAngles( void )
	{
		if ( ViewLocked() )
		{
			UpdateSeat();
			return m_angSeat;
		}
		return BaseClass::GetRenderAngles();
	}

	virtual void GetRenderBounds( Vector &mins, Vector &maxs )
	{
		BaseClass::GetRenderBounds( mins, maxs );
		if ( m_bRiding )
		{
			// The drawn seat is up to ~40 units from the networked shoulder.
			mins -= Vector( 48, 48, 48 );
			maxs += Vector( 48, 48, 48 );
		}
	}

	virtual void GetRenderBoundsWorldspace( Vector &absMins, Vector &absMaxs )
	{
		if ( ViewLocked() )
		{
			UpdateSeat();
			absMins = m_vecSeatOrigin - Vector( 40, 40, 40 );
			absMaxs = m_vecSeatOrigin + Vector( 40, 40, 40 );
			return;
		}
		BaseClass::GetRenderBoundsWorldspace( absMins, absMaxs );
	}

	virtual ShadowType_t ShadowCastType()
	{
		return m_bRiding ? SHADOWS_NONE : BaseClass::ShadowCastType();
	}

private:
	bool ViewLocked()
	{
		if ( !m_bRiding ) return false;
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		if ( !pLocal || GetMoveParent() != pLocal ) return false;
		return !C_BasePlayer::ShouldDrawLocalPlayer() && !pLocal->IsInAVehicle();
	}

	void UpdateSeat()
	{
		const Vector &eye = MainViewOrigin();
		const QAngle &view = MainViewAngles();
		Vector forward, right, up;
		AngleVectors( view, &forward, &right, &up );

		float bob = sinf( gpGlobals->curtime * 1.7f ) * cl_crabby_ride_bob.GetFloat();
		m_vecSeatOrigin = eye + forward * cl_crabby_ride_forward.GetFloat()
							  + right * cl_crabby_ride_right.GetFloat()
							  + up * ( cl_crabby_ride_up.GetFloat() + bob );
		m_angSeat = view;
		m_angSeat.x += cl_crabby_ride_pitch.GetFloat();
		m_angSeat.y += cl_crabby_ride_yaw.GetFloat();
		m_angSeat.z = 0;
	}

	bool m_bRiding;
	bool m_bWasRiding;
	Vector m_vecSeatOrigin;
	QAngle m_angSeat;
};

IMPLEMENT_CLIENTCLASS_DT( C_NPC_Crabby, DT_NPC_Crabby, CNPC_Crabby )
	RecvPropBool( RECVINFO( m_bRiding ) ),
END_RECV_TABLE()
