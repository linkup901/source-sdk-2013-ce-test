//========= Black Stasis 2, Phase 2 A1: the bs2_titlecard HUD element =========//
//
// Purpose: title cards (the prelude's dictionary card, the forest intro's "Black Stasis", the end titles) and the small bottom-right hint.
//
// Cards come from point_bs2_titlecard (server\bs2\point_bs2_titlecard.cpp) as "BS2Card" user messages (shared\bs2\bs2_ui_messages.h):
// BLOCK messages first, then SHOW. A card that arrives while another one is showing waits in a queue. The hint lane is separate and draws on top.
//
// Fonts: resource/Anton-Regular.ttf ("Anton"), else resource/BebasNeue-Regular.ttf ("Bebas Neue"), else the scheme's ClientTitleFont. The dictionary card
// (BS2FONT_SERIF) uses Georgia, the hint (BS2FONT_SMALL) Trebuchet MS. Fonts are created on demand at the screen height / 1080 scale.
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "filesystem.h"
#include "bs2/bs2_ui_messages.h"
#include "bs2_hud.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define BS2CARD_MAX_BLOCKS	8
#define BS2CARD_MAX_TEXT	160
#define BS2CARD_MAX_LINES	12

struct BS2CardBlock_t
{
	char	szText[BS2CARD_MAX_TEXT];
	int		nFont;
	int		nSize;
	float	flX;
	float	flY;
	int		nAlign;
	float	flDelay;
	float	flFadeIn;
	int		r, g, b, a;
};

struct BS2Card_t
{
	int				nId;
	int				nBlocks;
	BS2CardBlock_t	blocks[BS2CARD_MAX_BLOCKS];
	float			flHold;
	float			flFadeOut;
	float			flBgFade;
	int				nFlags;
	float			flStart;		// curtime when it became the card that is showing
	float			flHideStart;	// curtime of Hide / Skip, or -1
	float			flHideLen;		// how long that fade takes
};

struct BS2FontEntry_t
{
	int			nFamily;
	int			nPixels;
	vgui::HFont	hFont;
};

static void BS2Card_Reset( BS2Card_t &card )
{
	memset( &card, 0, sizeof( card ) );
	card.nId = -1;
	card.flHideStart = -1.0f;
}

//-----------------------------------------------------------------------------
class CHudBS2TitleCard : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBS2TitleCard, vgui::Panel );

public:
	CHudBS2TitleCard( const char *pElementName );
	virtual ~CHudBS2TitleCard();

	virtual void	Init( void );
	virtual void	LevelInit( void );
	virtual void	LevelShutdown( void );
	virtual bool	ShouldDraw( void );

	void			MsgFunc_BS2Card( bf_read &msg );

	void			ShowHint( const char *pszText, float flDelay );
	void			HideHint( void );
	vgui::HFont		GetFont( int nFamily, int nPixels );

protected:
	virtual void	Paint( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	void			Clear( void );
	void			StartNext( void );
	void			AddBlock( int nId, const BS2CardBlock_t &block );
	BS2Card_t		*FindBuilding( int nId, bool bCreate );
	float			CardOpacity( const BS2Card_t &card, float flNow, float flBuilt ) const;
	static float	BuiltTime( const BS2Card_t &card );
	void			ExpandText( const char *pszIn, char *pszOut, int nOutSize ) const;
	void			DrawBlock( const BS2CardBlock_t &block, float flAlpha, int nScreenW, int nScreenH, bool bHint );
	void			DrawHint( float flNow, int nScreenW, int nScreenH );

	CUtlVector<BS2Card_t>			m_Building;		// blocks that have arrived, waiting for their Show
	CUtlVector<BS2Card_t>			m_Queue;
	CUtlVector<BS2FontEntry_t>		m_Fonts;

	BS2Card_t		m_Current;
	bool			m_bShowing;

	BS2Card_t		m_Hint;
	bool			m_bHintOn;

	bool			m_bHaveAnton;
	bool			m_bHaveBebas;
	vgui::HFont		m_hSchemeTitleFont;
};

DECLARE_HUDELEMENT( CHudBS2TitleCard );
DECLARE_HUD_MESSAGE( CHudBS2TitleCard, BS2Card );

static CHudBS2TitleCard *s_pTitleCard = NULL;

//-----------------------------------------------------------------------------
CHudBS2TitleCard::CHudBS2TitleCard( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudBS2TitleCard" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	BS2Card_Reset( m_Current );
	BS2Card_Reset( m_Hint );
	m_bShowing = false;
	m_bHintOn = false;
	m_bHaveAnton = false;
	m_bHaveBebas = false;
	m_hSchemeTitleFont = 0;

	s_pTitleCard = this;
}

CHudBS2TitleCard::~CHudBS2TitleCard()
{
	if ( s_pTitleCard == this )
		s_pTitleCard = NULL;
}

void CHudBS2TitleCard::Init( void )
{
	HOOK_HUD_MESSAGE( CHudBS2TitleCard, BS2Card );

	// the display fonts, when the files have been dropped into the mod's resource folder
	if ( g_pFullFileSystem->FileExists( "resource/Anton-Regular.ttf", "GAME" ) )
	{
		m_bHaveAnton = surface()->AddCustomFontFile( "Anton", "resource/Anton-Regular.ttf" );
	}
	if ( g_pFullFileSystem->FileExists( "resource/BebasNeue-Regular.ttf", "GAME" ) )
	{
		m_bHaveBebas = surface()->AddCustomFontFile( "Bebas Neue", "resource/BebasNeue-Regular.ttf" );
	}

	Clear();
}

void CHudBS2TitleCard::LevelInit( void )
{
	Clear();
}

void CHudBS2TitleCard::LevelShutdown( void )
{
	Clear();
}

void CHudBS2TitleCard::Clear( void )
{
	m_Building.RemoveAll();
	m_Queue.RemoveAll();
	BS2Card_Reset( m_Current );
	BS2Card_Reset( m_Hint );
	m_bShowing = false;
	m_bHintOn = false;
}

void CHudBS2TitleCard::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetVisible( ShouldDraw() );
	SetBgColor( Color( 0, 0, 0, 0 ) );

	m_hSchemeTitleFont = pScheme->GetFont( "ClientTitleFont" );
	if ( !m_hSchemeTitleFont )
	{
		m_hSchemeTitleFont = pScheme->GetFont( "Default" );
	}

	// the screen size may have changed: drop the cached fonts, they are made again on demand
	m_Fonts.RemoveAll();
}

bool CHudBS2TitleCard::ShouldDraw( void )
{
	// like CHudCredits: a card shows whatever hidehud says (the cinematics hide the HUD)
	return ( m_bShowing || m_bHintOn );
}

//-----------------------------------------------------------------------------
// Purpose: the card being built for this id (the entity index of the point_bs2_titlecard)
//-----------------------------------------------------------------------------
BS2Card_t *CHudBS2TitleCard::FindBuilding( int nId, bool bCreate )
{
	for ( int i = 0; i < m_Building.Count(); i++ )
	{
		if ( m_Building[i].nId == nId )
			return &m_Building[i];
	}

	if ( !bCreate )
		return NULL;

	BS2Card_t card;
	BS2Card_Reset( card );
	card.nId = nId;
	int nIndex = m_Building.AddToTail( card );
	return &m_Building[nIndex];
}

void CHudBS2TitleCard::AddBlock( int nId, const BS2CardBlock_t &block )
{
	BS2Card_t *pCard = FindBuilding( nId, true );
	if ( pCard && pCard->nBlocks < BS2CARD_MAX_BLOCKS )
	{
		pCard->blocks[pCard->nBlocks] = block;
		pCard->nBlocks++;
	}
}

void CHudBS2TitleCard::StartNext( void )
{
	if ( m_Queue.Count() == 0 )
	{
		m_bShowing = false;
		BS2Card_Reset( m_Current );
		return;
	}

	m_Current = m_Queue[0];
	m_Queue.Remove( 0 );
	m_Current.flStart = gpGlobals->curtime;
	m_Current.flHideStart = -1.0f;
	m_bShowing = true;
}

//-----------------------------------------------------------------------------
void CHudBS2TitleCard::MsgFunc_BS2Card( bf_read &msg )
{
	int nCmd = msg.ReadByte();

	switch ( nCmd )
	{
	case BS2CARD_BLOCK:
		{
			int nId = msg.ReadShort();

			BS2CardBlock_t block;
			memset( &block, 0, sizeof( block ) );
			msg.ReadString( block.szText, sizeof( block.szText ) );
			block.nFont = msg.ReadByte();
			block.nSize = msg.ReadShort();
			block.flX = msg.ReadShort() / 1000.0f;
			block.flY = msg.ReadShort() / 1000.0f;
			block.nAlign = msg.ReadByte();
			block.flDelay = msg.ReadShort() / 100.0f;
			block.flFadeIn = msg.ReadShort() / 100.0f;
			block.r = msg.ReadByte();
			block.g = msg.ReadByte();
			block.b = msg.ReadByte();
			block.a = msg.ReadByte();

			AddBlock( nId, block );
		}
		break;

	case BS2CARD_SHOW:
		{
			int nId = msg.ReadShort();
			float flHold = msg.ReadShort() / 100.0f;
			float flFadeOut = msg.ReadShort() / 100.0f;
			int nFlags = msg.ReadShort();
			float flBgFade = msg.ReadShort() / 100.0f;

			BS2Card_t *pBuilt = FindBuilding( nId, false );
			if ( !pBuilt )
				break;		// no text blocks were sent: nothing to show

			BS2Card_t card = *pBuilt;
			card.flHold = flHold;
			card.flFadeOut = flFadeOut;
			card.nFlags = nFlags;
			card.flBgFade = flBgFade;
			card.flStart = gpGlobals->curtime;
			card.flHideStart = -1.0f;

			// the blocks are used again if the entity is shown a second time, they are sent again each time
			for ( int i = 0; i < m_Building.Count(); i++ )
			{
				if ( m_Building[i].nId == nId )
				{
					m_Building.Remove( i );
					break;
				}
			}

			if ( nFlags & BS2CARDF_HINT )
			{
				m_Hint = card;
				m_bHintOn = true;
			}
			else
			{
				m_Queue.AddToTail( card );
				if ( !m_bShowing )
				{
					StartNext();
				}
			}
		}
		break;

	case BS2CARD_HIDE:
	case BS2CARD_SKIP:
		{
			int nId = msg.ReadShort();
			const float flLen = ( nCmd == BS2CARD_SKIP ) ? 0.3f : MAX( 0.3f, m_Current.flFadeOut );

			if ( m_bHintOn && ( nId == m_Hint.nId || nId == -1 ) && m_Hint.flHideStart < 0.0f )
			{
				m_Hint.flHideStart = gpGlobals->curtime;
				m_Hint.flHideLen = 0.4f;
			}

			if ( m_bShowing && ( nId == m_Current.nId || nId == -1 ) && m_Current.flHideStart < 0.0f )
			{
				m_Current.flHideStart = gpGlobals->curtime;
				m_Current.flHideLen = flLen;
			}

			// a queued card of that id is simply dropped
			for ( int i = m_Queue.Count() - 1; i >= 0; i-- )
			{
				if ( nId == -1 || m_Queue[i].nId == nId )
				{
					m_Queue.Remove( i );
				}
			}
		}
		break;

	case BS2CARD_CLEAR:
		{
			msg.ReadShort();
			Clear();
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: when the last block has finished fading in
//-----------------------------------------------------------------------------
float CHudBS2TitleCard::BuiltTime( const BS2Card_t &card )
{
	float flBuilt = 0.0f;
	for ( int i = 0; i < card.nBlocks; i++ )
	{
		flBuilt = MAX( flBuilt, card.blocks[i].flDelay + card.blocks[i].flFadeIn );
	}
	return flBuilt;
}

//-----------------------------------------------------------------------------
// Purpose: the card's overall opacity 0..1 (the fade out after the hold time, or after Hide / Skip); <= 0 means it is over
//-----------------------------------------------------------------------------
float CHudBS2TitleCard::CardOpacity( const BS2Card_t &card, float flNow, float flBuilt ) const
{
	float flOpacity = 1.0f;
	const float flT = flNow - card.flStart;

	if ( !( card.nFlags & BS2CARDF_FOREVER ) )
	{
		const float flHoldEnd = flBuilt + card.flHold;
		if ( flT > flHoldEnd )
		{
			const float flLen = MAX( card.flFadeOut, 0.05f );
			flOpacity = MIN( flOpacity, 1.0f - ( flT - flHoldEnd ) / flLen );
		}
	}

	if ( card.flHideStart >= 0.0f )
	{
		const float flLen = MAX( card.flHideLen, 0.05f );
		flOpacity = MIN( flOpacity, 1.0f - ( flNow - card.flHideStart ) / flLen );
	}

	return flOpacity;
}

//-----------------------------------------------------------------------------
// Purpose: the font for a family at nPixels (as at 1080 lines, scaled to the screen)
//-----------------------------------------------------------------------------
vgui::HFont CHudBS2TitleCard::GetFont( int nFamily, int nPixels )
{
	int nScreenW, nScreenH;
	GetHudSize( nScreenW, nScreenH );

	const int nTall = MAX( 8, (int)( nPixels * ( (float)nScreenH / 1080.0f ) + 0.5f ) );

	for ( int i = 0; i < m_Fonts.Count(); i++ )
	{
		if ( m_Fonts[i].nFamily == nFamily && m_Fonts[i].nPixels == nTall )
			return m_Fonts[i].hFont;
	}

	const char *pszName = NULL;
	int nWeight = 400;

	switch ( nFamily )
	{
	case BS2FONT_SERIF:
		pszName = "Georgia";
		break;

	case BS2FONT_SMALL:
		pszName = "Trebuchet MS";
		nWeight = 500;
		break;

	case BS2FONT_TITLE:
	default:
		if ( m_bHaveAnton )
			pszName = "Anton";
		else if ( m_bHaveBebas )
			pszName = "Bebas Neue";
		break;
	}

	vgui::HFont hFont = 0;

	if ( pszName )
	{
		hFont = surface()->CreateFont();
		if ( !surface()->SetFontGlyphSet( hFont, pszName, nTall, nWeight, 0, 0, vgui::ISurface::FONTFLAG_ANTIALIAS ) )
		{
			hFont = 0;
		}
	}

	if ( !hFont )
	{
		hFont = m_hSchemeTitleFont;		// "then the current font"
	}

	BS2FontEntry_t entry;
	entry.nFamily = nFamily;
	entry.nPixels = nTall;
	entry.hFont = hFont;
	m_Fonts.AddToTail( entry );

	return hFont;
}

//-----------------------------------------------------------------------------
// Purpose: %use% -> the key bound to +use, '|' and "\n" -> line breaks (a real '\n')
//-----------------------------------------------------------------------------
void CHudBS2TitleCard::ExpandText( const char *pszIn, char *pszOut, int nOutSize ) const
{
	char szKey[32];
	const char *pBinding = engine->Key_LookupBinding( "use" );
	if ( pBinding && pBinding[0] )
	{
		Q_strncpy( szKey, pBinding, sizeof( szKey ) );
		Q_strupr( szKey );
	}
	else
	{
		Q_strncpy( szKey, "E", sizeof( szKey ) );
	}

	int nOut = 0;
	for ( const char *p = pszIn; *p != '\0' && nOut < nOutSize - 1; )
	{
		if ( Q_strnicmp( p, "%use%", 5 ) == 0 )
		{
			for ( const char *k = szKey; *k != '\0' && nOut < nOutSize - 1; ++k )
			{
				pszOut[nOut++] = *k;
			}
			p += 5;
		}
		else if ( p[0] == '|' || ( p[0] == '\\' && p[1] == 'n' ) )
		{
			pszOut[nOut++] = '\n';
			p += ( p[0] == '|' ) ? 1 : 2;
		}
		else
		{
			pszOut[nOut++] = *p++;
		}
	}
	pszOut[nOut] = '\0';
}

//-----------------------------------------------------------------------------
// Purpose: one text block, line by line, at the block's anchor
//-----------------------------------------------------------------------------
void CHudBS2TitleCard::DrawBlock( const BS2CardBlock_t &block, float flAlpha, int nScreenW, int nScreenH, bool bHint )
{
	if ( flAlpha <= 0.0f )
		return;

	char szText[BS2CARD_MAX_TEXT * 2];
	ExpandText( block.szText, szText, sizeof( szText ) );

	int nPixels = block.nSize;
	if ( bHint )
	{
		nPixels = MIN( nPixels, 30 );
	}

	vgui::HFont hFont = GetFont( bHint ? BS2FONT_SMALL : block.nFont, nPixels );
	if ( !hFont )
		return;

	// split into lines
	wchar_t wszLines[BS2CARD_MAX_LINES][BS2CARD_MAX_TEXT];
	int nLines = 0;
	{
		char *pLine = szText;
		while ( pLine && nLines < BS2CARD_MAX_LINES )
		{
			char *pBreak = strchr( pLine, '\n' );
			if ( pBreak )
				*pBreak = '\0';

			Q_UTF8ToUnicode( pLine, wszLines[nLines], sizeof( wszLines[nLines] ) );
			nLines++;

			pLine = pBreak ? pBreak + 1 : NULL;
		}
	}

	const int nLineTall = surface()->GetFontTall( hFont );
	const int nLineStep = nLineTall + nLineTall / 6;
	const int nBlockTall = nLines * nLineStep;

	const int a = clamp( (int)( block.a * flAlpha ), 0, 255 );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( block.r, block.g, block.b, a );

	int y = (int)( block.flY * nScreenH ) - nBlockTall / 2;
	for ( int i = 0; i < nLines; i++ )
	{
		int nW, nH;
		surface()->GetTextSize( hFont, wszLines[i], nW, nH );

		int x = (int)( block.flX * nScreenW );
		if ( block.nAlign == BS2ALIGN_CENTER )
			x -= nW / 2;
		else if ( block.nAlign == BS2ALIGN_RIGHT )
			x -= nW;

		surface()->DrawSetTextPos( x, y );
		surface()->DrawUnicodeString( wszLines[i] );

		y += nLineStep;
	}
}

//-----------------------------------------------------------------------------
// Purpose: the bottom-right hint: the first block's text, a slow pulse, fades in after its delay
//-----------------------------------------------------------------------------
void CHudBS2TitleCard::DrawHint( float flNow, int nScreenW, int nScreenH )
{
	if ( !m_bHintOn || m_Hint.nBlocks == 0 )
		return;

	const BS2CardBlock_t &src = m_Hint.blocks[0];
	const float flBuilt = src.flDelay + src.flFadeIn;
	const float flT = flNow - m_Hint.flStart;
	const float flOpacity = CardOpacity( m_Hint, flNow, flBuilt );

	if ( flOpacity <= 0.0f )
	{
		m_bHintOn = false;
		return;
	}

	float flFade = ( src.flFadeIn > 0.0f ) ? clamp( ( flT - src.flDelay ) / src.flFadeIn, 0.0f, 1.0f ) : ( flT >= src.flDelay ? 1.0f : 0.0f );
	const float flPulse = 0.78f + 0.22f * sinf( flNow * 2.2f );

	BS2CardBlock_t block = src;
	block.flX = 0.975f;
	block.flY = 0.945f;
	block.nAlign = BS2ALIGN_RIGHT;

	DrawBlock( block, flFade * flOpacity * flPulse, nScreenW, nScreenH, true );
}

//-----------------------------------------------------------------------------
void CHudBS2TitleCard::Paint( void )
{
	int nScreenW, nScreenH;
	GetHudSize( nScreenW, nScreenH );
	SetSize( nScreenW, nScreenH );
	SetPos( 0, 0 );

	const float flNow = gpGlobals->curtime;

	// the card that is showing
	if ( m_bShowing )
	{
		const float flBuilt = BuiltTime( m_Current );
		const float flOpacity = CardOpacity( m_Current, flNow, flBuilt );

		if ( flOpacity <= 0.0f )
		{
			StartNext();
		}
		else
		{
			const float flT = flNow - m_Current.flStart;

			if ( m_Current.nFlags & BS2CARDF_BLACKBG )
			{
				const float flBg = ( m_Current.flBgFade > 0.0f ) ? clamp( flT / m_Current.flBgFade, 0.0f, 1.0f ) : 1.0f;
				surface()->DrawSetColor( 0, 0, 0, (int)( 255.0f * flBg * flOpacity ) );
				surface()->DrawFilledRect( 0, 0, nScreenW, nScreenH );
			}

			for ( int i = 0; i < m_Current.nBlocks; i++ )
			{
				const BS2CardBlock_t &block = m_Current.blocks[i];
				float flFade = 0.0f;

				if ( block.flFadeIn > 0.0f )
					flFade = clamp( ( flT - block.flDelay ) / block.flFadeIn, 0.0f, 1.0f );
				else if ( flT >= block.flDelay )
					flFade = 1.0f;

				DrawBlock( block, flFade * flOpacity, nScreenW, nScreenH, false );
			}
		}
	}

	DrawHint( flNow, nScreenW, nScreenH );
}

//-----------------------------------------------------------------------------
// Purpose: the hint without a map entity (logic_skipkey's BS2Skip message ends up here)
//-----------------------------------------------------------------------------
void CHudBS2TitleCard::ShowHint( const char *pszText, float flDelay )
{
	BS2Card_Reset( m_Hint );
	m_Hint.nId = -2;
	m_Hint.nBlocks = 1;
	m_Hint.nFlags = BS2CARDF_HINT | BS2CARDF_FOREVER;
	m_Hint.flStart = gpGlobals->curtime;
	m_Hint.flHideStart = -1.0f;

	BS2CardBlock_t &block = m_Hint.blocks[0];
	Q_strncpy( block.szText, pszText, sizeof( block.szText ) );
	block.nFont = BS2FONT_SMALL;
	block.nSize = 26;
	block.flDelay = MAX( flDelay, 0.0f );
	block.flFadeIn = 1.0f;
	block.r = block.g = block.b = 235;
	block.a = 215;

	m_bHintOn = true;
}

void CHudBS2TitleCard::HideHint( void )
{
	if ( m_bHintOn && m_Hint.flHideStart < 0.0f )
	{
		m_Hint.flHideStart = gpGlobals->curtime;
		m_Hint.flHideLen = 0.4f;
	}
}

void BS2_Hint_Show( const char *pszText, float flDelay )
{
	if ( s_pTitleCard )
		s_pTitleCard->ShowHint( pszText, flDelay );
}

void BS2_Hint_Hide( void )
{
	if ( s_pTitleCard )
		s_pTitleCard->HideHint();
}

vgui::HFont BS2_GetFont( int nFamily, int nPixels )
{
	return s_pTitleCard ? s_pTitleCard->GetFont( nFamily, nPixels ) : 0;
}
