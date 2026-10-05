//========= Black Stasis 2, Phase 2: hooks between the BS2 HUD elements =========//
#ifndef BS2_HUD_H
#define BS2_HUD_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>

// The small bottom-right hint ("Press E to skip"). %use% in the text becomes the key bound to +use. flDelay seconds after the call it fades in.
void BS2_Hint_Show( const char *pszText, float flDelay );
void BS2_Hint_Hide( void );

// The display fonts (BS2FONT_* in shared\bs2\bs2_ui_messages.h) at nPixels as at 1080 lines, scaled to the screen; 0 before the HUD exists
vgui::HFont BS2_GetFont( int nFamily, int nPixels );

// The vein overlay (client\bs2\bs2_hud_veins.cpp)
void BS2_Veins_Pulse( float flStrength, float flDuration );

#endif // BS2_HUD_H
