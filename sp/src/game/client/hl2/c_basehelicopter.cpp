//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_basehelicopter.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "KeyValues.h"
#include "tier2/beamsegdraw.h"
#include "materialsystem/imaterialsystem.h"
#include "view.h"
#include "model_types.h"
#include "engine/ivmodelinfo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_BaseHelicopter, DT_BaseHelicopter, CBaseHelicopter )
	RecvPropTime( RECVINFO( m_flStartupTime ) ),
END_RECV_TABLE()


C_BaseHelicopter::C_BaseHelicopter()
{
	m_LastSpeedTime = -1.0f;
	m_SmoothedSpeed = 0.0f;
	m_LastSpeedLog = 0.0f;
	m_LastSpeedFrame = -1;
}


//-----------------------------------------------------------------------------
// Chopper blade fade-in time
//-----------------------------------------------------------------------------
#define FADE_IN_TIME	2.0f


//-----------------------------------------------------------------------------
// Sets the fade of the blades when the chopper starts up
//-----------------------------------------------------------------------------
class CHeliBladeMaterialProxy : public CEntityMaterialProxy
{
public:
	CHeliBladeMaterialProxy() { m_AlphaVar = NULL; }
	virtual ~CHeliBladeMaterialProxy() {}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( C_BaseEntity *pEntity );
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar *m_AlphaVar;
	bool m_bFadeOut;
};

bool CHeliBladeMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool foundVar;
	m_AlphaVar = pMaterial->FindVar( "$alpha", &foundVar, false );
	m_bFadeOut = pKeyValues->GetInt( "$fadeout", 0 ) != 0;
	return foundVar;
}

void CHeliBladeMaterialProxy::OnBind( C_BaseEntity *pEnt )
{
	if (!m_AlphaVar)
		return;

	C_BaseHelicopter *pHeli = dynamic_cast<C_BaseHelicopter*>( pEnt );
	if ( pHeli )
	{
		float dt = gpGlobals->curtime  - pHeli->StartupTime();
		dt /= FADE_IN_TIME;
		dt = clamp( dt, 0.0f, 1.0f );
		if ( m_bFadeOut ) 
		{
			dt = 1.0f - dt;
		}

		m_AlphaVar->SetFloatValue( dt );
	}
	else
	{
		m_AlphaVar->SetFloatValue( 1.0f );
	}
}

IMaterial *CHeliBladeMaterialProxy::GetMaterial()
{
	if ( !m_AlphaVar )
		return NULL;

	return m_AlphaVar->GetOwningMaterial();
}

EXPOSE_INTERFACE( CHeliBladeMaterialProxy, IMaterialProxy, "HeliBlade" IMATERIAL_PROXY_INTERFACE_VERSION );


// Isolated helicopter ribbon review. No server or campaign changes required.
static ConVar bs2_heli_trails("bs2_heli_trails", "1", FCVAR_ARCHIVE);
static ConVar bs2_heli_trail_start("bs2_heli_trail_start", "550", FCVAR_ARCHIVE);
static ConVar bs2_heli_trail_full("bs2_heli_trail_full", "850", FCVAR_ARCHIVE);
static ConVar bs2_heli_trail_life("bs2_heli_trail_life", "0.65", FCVAR_ARCHIVE);
static ConVar bs2_heli_trail_debug("bs2_heli_trail_debug", "0", FCVAR_CHEAT);

void C_BaseHelicopter::GetRenderBounds(Vector &mins, Vector &maxs)
{
	BaseClass::GetRenderBounds(mins, maxs);
	// Keep history visible when the helicopter itself approaches the screen edge.
	const char *name=GetModel() ? modelinfo->GetModelName(GetModel()) : NULL;
	if (bs2_heli_trails.GetBool() && name && Q_stristr(name,"helicopter"))
	{ mins -= Vector(1600,1600,1600); maxs += Vector(1600,1600,1600); }
}

void C_BaseHelicopter::UpdateSpeedTrail()
{
	if (m_LastSpeedFrame == gpGlobals->framecount) return;
	m_LastSpeedFrame = gpGlobals->framecount;
	const float now = gpGlobals->curtime;
	const Vector origin = GetAbsOrigin();
	const float dt = now - m_LastSpeedTime;
	const float life = clamp(bs2_heli_trail_life.GetFloat(),0.05f,2.0f);
	while (m_SpeedPoints.Count() && now-m_SpeedPoints[0].time > life)
		m_SpeedPoints.Remove(0);
	if (m_LastSpeedTime < 0 || dt > 0.2f || (origin-m_LastSpeedOrigin).Length() > 800)
	{
		m_SpeedPoints.RemoveAll(); m_SmoothedSpeed=0;
		m_LastSpeedOrigin=origin; m_LastSpeedTime=now; return;
	}
	if (dt <= 0) return;
	const float speed = (origin-m_LastSpeedOrigin).Length()/dt;
	m_SmoothedSpeed += (speed-m_SmoothedSpeed)*(1.0f-expf(-dt/0.18f));
	m_LastSpeedOrigin=origin; m_LastSpeedTime=now;
	const float low=bs2_heli_trail_start.GetFloat();
	const float high=MAX(low+1.0f,bs2_heli_trail_full.GetFloat());
	float strength=clamp((m_SmoothedSpeed-low)/(high-low),0.0f,1.0f);
	strength=strength*strength*(3.0f-2.0f*strength);
	// Also record zero-strength points; these let the old trail drain naturally.
	bool hasVisibleHistory = strength>0;
	for(int i=0;i<m_SpeedPoints.Count() && !hasVisibleHistory;i++)
		hasVisibleHistory=m_SpeedPoints[i].strength>0;
	if (!hasVisibleHistory) m_SpeedPoints.RemoveAll();
	if (hasVisibleHistory)
	{
		SpeedPoint point; point.time=now; point.strength=strength;
		// Stock navigation-light attachments sit on the two stub-wing tips.
		// Follow the animated model's bones rather than approximate origin offsets.
		if (!GetAttachment("Light_Red0",point.pos[0]) || !GetAttachment("Light_Red1",point.pos[1]))
		{ m_SpeedPoints.RemoveAll(); return; }
		m_SpeedPoints.AddToTail(point);
		if(m_SpeedPoints.Count()>180) m_SpeedPoints.Remove(0);
	}
	if(bs2_heli_trail_debug.GetBool() && now-m_LastSpeedLog>=0.5f)
	{
		DevMsg("HELI_TRAIL time=%.2f speed=%.1f strength=%.3f samples=%d\n",now,m_SmoothedSpeed,strength,m_SpeedPoints.Count());
		m_LastSpeedLog=now;
	}
}

void C_BaseHelicopter::DrawSpeedTrail()
{
	const int n=m_SpeedPoints.Count(); if(n<2) return;
	IMaterial *mat=materials->FindMaterial("bs2/heli/speed_ribbon",TEXTURE_GROUP_OTHER);
	if(mat->IsErrorMaterial()) return;
	CMatRenderContextPtr ctx(materials);
	const float life=clamp(bs2_heli_trail_life.GetFloat(),0.05f,2.0f);
	for(int k=0;k<2;k++)
	{
		CBeamSegDraw beam; beam.Start(ctx,(n-1)*3+1,mat);
		for(int i=0;i<n-1;i++)
		{
			const SpeedPoint &a=m_SpeedPoints[i],&b=m_SpeedPoints[i+1];
			const Vector &p0=m_SpeedPoints[MAX(0,i-1)].pos[k];
			const Vector &p3=m_SpeedPoints[MIN(n-1,i+2)].pos[k];
			for(int sub=0;sub<3;sub++)
			{
				const float u=sub/3.0f, u2=u*u, u3=u2*u;
				BeamSeg_t seg;
				seg.m_vPos=0.5f*((2.0f*a.pos[k])+(-p0+b.pos[k])*u+(2.0f*p0-5.0f*a.pos[k]+4.0f*b.pos[k]-p3)*u2+(-p0+3.0f*a.pos[k]-3.0f*b.pos[k]+p3)*u3);
				const float age=gpGlobals->curtime-(a.time+(b.time-a.time)*u);
				const float remain=clamp(1.0f-age/life,0.0f,1.0f);
				const float strength=a.strength+(b.strength-a.strength)*u;
				seg.m_flWidth=4.5f*remain;
				seg.m_flAlpha=0.72f*strength*remain*remain;
				// Fade the oldest endpoint even before history has filled its lifetime.
				if(i==0) seg.m_flAlpha*=u;
				seg.m_vColor.Init(1,1,1); seg.m_flTexCoord=0.5f;
				beam.NextSeg(&seg);
			}
		}
		BeamSeg_t head; head.m_vPos=m_SpeedPoints[n-1].pos[k];
		head.m_vColor.Init(1,1,1); head.m_flWidth=4.5f;
		head.m_flAlpha=0.72f*m_SpeedPoints[n-1].strength; head.m_flTexCoord=0.5f;
		beam.NextSeg(&head); beam.End();
	}
}

int C_BaseHelicopter::DrawModel(int flags)
{
	int result=BaseClass::DrawModel(flags);
	if(flags & (STUDIO_SHADOWDEPTHTEXTURE | STUDIO_SSAODEPTHTEXTURE)) return result;
	// Gunships inherit this class too; restrict the review to the HL2 helicopter.
	const char *name=GetModel() ? modelinfo->GetModelName(GetModel()) : NULL;
	if(!bs2_heli_trails.GetBool() || !name || !Q_stristr(name,"helicopter"))
	{ m_SpeedPoints.RemoveAll(); m_LastSpeedTime=-1; return result; }
	UpdateSpeedTrail(); DrawSpeedTrail(); return result;
}
