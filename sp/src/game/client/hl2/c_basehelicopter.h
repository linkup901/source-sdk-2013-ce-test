//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_BASEHELICOPTER_H
#define C_BASEHELICOPTER_H
#ifdef _WIN32
#pragma once
#endif


#include "c_ai_basenpc.h"
#include "materialsystem/MaterialSystemUtil.h"


class C_BaseHelicopter : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_BaseHelicopter, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

	C_BaseHelicopter();

	virtual int DrawModel( int flags );
	virtual void GetRenderBounds( Vector &mins, Vector &maxs );
	float StartupTime() const { return m_flStartupTime; }

private:
	C_BaseHelicopter( const C_BaseHelicopter &other ) {}
	float m_flStartupTime;
	struct SpeedPoint { Vector pos[2]; float time; float strength; };
	CUtlVector<SpeedPoint> m_SpeedPoints;
	Vector m_LastSpeedOrigin;
	float m_LastSpeedTime, m_SmoothedSpeed, m_LastSpeedLog;
	int m_LastSpeedFrame;
	// BS2: wing-tip ribbons are opt-in per helicopter (cinematic helicopters only).
	// Set by the server; stock npc_helicopters used for gameplay never draw them.
	bool m_bBS2SpeedTrails;
	// Holds a reference so the material system cannot unload the ribbon material
	// while it is still being drawn ("bad reference count 0 when being bound").
	CMaterialReference m_SpeedRibbonMaterial;
	bool WantsSpeedTrail();
	void UpdateSpeedTrail();
	void DrawSpeedTrail();
};


#endif // C_BASEHELICOPTER_H
