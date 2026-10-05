//========= BS2 Phase 2: user message layout shared by the server entities and the client HUD elements =========//
//
// "BS2Card"   title cards and the small bottom-right hint (point_bs2_titlecard, logic_skipkey)
// "BS2Skip"   arms / disarms the use-key skip detector (logic_skipkey)
// "BS2Credits" start / stop the credits roll (point_bs2_credits)
// "BS2Veins"  vein overlay: pulse, steady intensity, clear (env_bs2_veins)
//
// A user message may not exceed 255 bytes, so a card is sent block by block: BLOCK x n, then SHOW.
//=============================================================================//
#ifndef BS2_UI_MESSAGES_H
#define BS2_UI_MESSAGES_H
#ifdef _WIN32
#pragma once
#endif

// ---- BS2Card: byte cmd, then ...
enum
{
	BS2CARD_CLEAR = 0,		// short id (-1 = every card)
	BS2CARD_BLOCK = 1,		// short id, string text, byte font, short size, short x1000, short y1000, byte align, short delay100, short fadein100, byte r, byte g, byte b, byte a
	BS2CARD_SHOW = 2,		// short id, short hold100, short fadeout100, short flags, short bgfade100
	BS2CARD_HIDE = 3,		// short id
	BS2CARD_SKIP = 4,		// short id (-1 = whatever is showing): fade it out fast, the queue goes on
};

enum	// BS2CARD_SHOW flags
{
	BS2CARDF_HINT = (1 << 0),		// the small bottom-right hint lane (does not queue, shows on top of a card)
	BS2CARDF_BLACKBG = (1 << 1),	// full-screen black behind the card (fades in over bgfade)
	BS2CARDF_FOREVER = (1 << 2),	// no hold time: stays until Hide / Skip
};

enum	// block fonts
{
	BS2FONT_TITLE = 0,		// Anton, else Bebas Neue, else the scheme's title font
	BS2FONT_SERIF = 1,		// the dictionary card
	BS2FONT_SMALL = 2,		// the hint
};

enum	// block alignment (about the x position)
{
	BS2ALIGN_LEFT = 0,
	BS2ALIGN_CENTER = 1,
	BS2ALIGN_RIGHT = 2,
};

// ---- BS2Skip: byte cmd
enum
{
	BS2SKIP_DISARM = 0,
	BS2SKIP_ARM = 1,		// float hint delay (<0: no hint), short flags (1 = only while a point_viewcontrol is the view)
};

// ---- BS2Credits: byte cmd
enum
{
	BS2CREDITS_STOP = 0,
	BS2CREDITS_START = 1,	// string file, float speed (px/s at 1080p), float fadein, float hold-after, short flags (1 = fade the roll out at the end)
};

// ---- BS2Veins: byte cmd
enum
{
	BS2VEINS_PULSE = 0,		// float strength 0..1, float duration
	BS2VEINS_LEVEL = 1,		// float level 0..1 (steady), float seconds to get there
	BS2VEINS_CLEAR = 2,
};

#endif // BS2_UI_MESSAGES_H
