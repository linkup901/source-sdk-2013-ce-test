//========= Black Stasis 2, Phase 2 A4: black breath mist =========//
//
// Purpose: pure black wisps breathed out at an NPC's head attachment on a ~4 s rhythm (particles/bs2_breath.pcf: bs2_breath_black, bs2_breath_black_large).
// Used by the Black Hunter (attachments top_eye, bottom_eye) and by the corrupted Combine (combine_corrupt.mdl, attachment eyes = the mask).
//
// How: BS2Breath_Start makes a server-only think entity (bs2_breath_emitter) that dispatches one burst of the particle system on the owner's attachment every
// breath period; it removes itself with its owner. The NPCs keep a 'breathmist' keyvalue (default 1: set 0 for scripted scenes) and the inputs EnableBreathMist,
// DisableBreathMist, SetBreathLarge (the foggy-forest close-up), SetBreathSmall.
//
//=============================================================================//
#ifndef BS2_BREATH_H
#define BS2_BREATH_H
#ifdef _WIN32
#pragma once
#endif

class CBaseEntity;
class CBaseAnimating;

void BS2Breath_Precache( void );

// one emitter on one attachment of pOwner (call it twice for two eyes)
void BS2Breath_Start( CBaseAnimating *pOwner, const char *pszAttachment );

// all emitters of pOwner
void BS2Breath_SetEnabled( CBaseEntity *pOwner, bool bEnabled );
void BS2Breath_SetLarge( CBaseEntity *pOwner, bool bLarge );
void BS2Breath_Stop( CBaseEntity *pOwner );

#endif // BS2_BREATH_H
