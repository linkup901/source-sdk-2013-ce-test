//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "c_weapon__stubs.h"
#include "basehlcombatweapon_shared.h"
#include "c_basehlcombatweapon.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

STUB_WEAPON_CLASS( cycler_weapon, WeaponCycler, C_BaseCombatWeapon );

STUB_WEAPON_CLASS( weapon_binoculars, WeaponBinoculars, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_bugbait, WeaponBugBait, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_flaregun, Flaregun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_annabelle, WeaponAnnabelle, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_gauss, WeaponGaussGun, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap, WeaponCubemap, C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_alyxgun, WeaponAlyxGun, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_citizenpackage, WeaponCitizenPackage, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_citizensuitcase, WeaponCitizenSuitcase, C_WeaponCitizenPackage );

#ifndef HL2MP
STUB_WEAPON_CLASS( weapon_ar2, WeaponAR2, C_HLMachineGun );
STUB_WEAPON_CLASS( weapon_frag, WeaponFrag, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_rpg, WeaponRPG, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_pistol, WeaponPistol, C_BaseHLCombatWeapon );
// Procedural whole-viewmodel movement for the stock HL2 shotgun prototype.
class C_WeaponShotgun : public C_BaseHLCombatWeapon
{
    DECLARE_CLASS(C_WeaponShotgun, C_BaseHLCombatWeapon);
public:
    DECLARE_PREDICTABLE();
    DECLARE_CLIENTCLASS();
    C_WeaponShotgun() : m_flBS2MotionTime(0), m_nBS2Motion(0) {}
    virtual void AddViewmodelBob(CBaseViewModel *vm, Vector &origin, QAngle &angles)
    {
        BaseClass::AddViewmodelBob(vm, origin, angles);
        const float age = gpGlobals->curtime - m_flBS2MotionTime;
        if (age < 0 || !m_nBS2Motion) return;
        Vector forward, right, up;
        AngleVectors(angles, &forward, &right, &up);
        // Independent hand/gun/shell animation is authored in the review model.
        if (m_nBS2Motion == 2 || m_nBS2Motion == 4) return;
        const bool heavy = m_nBS2Motion == 3;
        const float duration = heavy ? 1.1f : 0.30f;
        if (age >= duration) return;
        const float t = age / duration;
        const float attack = heavy ? 0.10f : 0.16f;
        float kick = t < attack ? t / attack : (1.0f-t)/(1.0f-attack);
        kick = kick*kick*(3.0f-2.0f*kick);
        // The stock fire sequence already includes muzzle rise. Add mostly rearward travel.
        origin -= forward * ((heavy ? 36.0f : 22.0f) * kick);
        origin += up * ((heavy ? 1.2f : 0.6f) * kick);
        angles.x -= (heavy ? 7.0f : 4.0f) * kick;
        angles.z += (heavy ? -3.0f : 1.5f) * kick;
    }
private:
    float m_flBS2MotionTime;
    int m_nBS2Motion;
};
STUB_WEAPON_CLASS_IMPLEMENT(weapon_shotgun, C_WeaponShotgun);
IMPLEMENT_CLIENTCLASS_DT(C_WeaponShotgun, DT_WeaponShotgun, CWeaponShotgun)
    RecvPropFloat(RECVINFO(m_flBS2MotionTime)),
    RecvPropInt(RECVINFO(m_nBS2Motion)),
END_RECV_TABLE()
STUB_WEAPON_CLASS( weapon_smg1, WeaponSMG1, C_HLSelectFireMachineGun );
STUB_WEAPON_CLASS( weapon_357, Weapon357, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_crossbow, WeaponCrossbow, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_slam, Weapon_SLAM, C_BaseHLCombatWeapon );
STUB_WEAPON_CLASS( weapon_crowbar, WeaponCrowbar, C_BaseHLBludgeonWeapon );
#ifdef HL2_EPISODIC
STUB_WEAPON_CLASS( weapon_hopwire, WeaponHopwire, C_BaseHLCombatWeapon );
//STUB_WEAPON_CLASS( weapon_proto1, WeaponProto1, C_BaseHLCombatWeapon );
#endif
#ifdef HL2_LOSTCOAST
STUB_WEAPON_CLASS( weapon_oldmanharpoon, WeaponOldManHarpoon, C_WeaponCitizenPackage );
#endif
#endif


