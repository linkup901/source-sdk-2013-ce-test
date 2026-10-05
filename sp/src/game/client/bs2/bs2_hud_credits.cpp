//========= Black Stasis 2, Phase 2 A2: the bs2_credits HUD element =========//
//
// Purpose: credits.txt rolling up over black. Started / stopped by point_bs2_credits ("BS2Credits" user message).
//
// credits.txt (in the mod folder, written in Phase 1) is plain text:
//   - no indent: a heading ("Sound effects"); the very first line is the title
//   - 4+ spaces: an entry; 8+ spaces: a detail line (smaller, dimmer)
//   - blank lines are space; lines starting with '(' or '#' are notes and are not shown; lines starting with "Per " (file lists) are not shown
//   - a line that is exactly [roll] makes the roll use only what follows it (a short form of the list); without it the whole file is used
// Lines wider than 62 % of the screen are wrapped. The roll ends when the last line has left the screen, or comes to rest in the middle (stopatend);
// after the hold time the element runs "bs2_creditsdone" (-> OnFinished of the entity).
//
//=============================================================================//

#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "filesystem.h"
#include "utlbuffer.h"
#include "bs2/bs2_ui_messages.h"
#include "bs2_hud.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define BS2CRED_MAX_LINES	512
#define BS2CRED_MAX_CHARS	200

enum
{
	CRED_TITLE = 0,
	CRED_HEADING,
	CRED_ENTRY,
	CRED_DETAIL,
	CRED_BLANK,
};

struct BS2CreditLine_t
{
	char	szText[BS2CRED_MAX_CHARS];
	int		nStyle;
	int		nY;			// top of the line, relative to the first line (pixels, filled in by Layout)
};

class CHudBS2Credits : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudBS2Credits, vgui::Panel );

public:
	CHudBS2Credits( const char *pElementName );

	virtual void	Init( void );
	virtual void	LevelInit( void );
	virtual void	LevelShutdown( void );
	virtual bool	ShouldDraw( void );

	void			MsgFunc_BS2Credits( bf_read &msg );

protected:
	virtual void	Paint( void );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

private:
	void			Stop( void );
	bool			Load( const char *pszFile );
	void			AddLine( const char *pszText, int nStyle );
	void			Layout( int nScreenW, int nScreenH );
	void			StyleFont( int nStyle, vgui::HFont &hFont, Color &color ) const;

	CUtlVector<BS2CreditLine_t>		m_Lines;

	bool			m_bRunning;
	float			m_flStart;
	float			m_flSpeed;
	float			m_flFadeIn;
	float			m_flHoldAfter;
	bool			m_bStopAtEnd;
	bool			m_bDoneSent;
	float			m_flDoneTime;			// when the roll has run out (curtime), 0 while it still rolls

	int				m_nLayoutW;
	int				m_nLayoutH;
	int				m_nTotalHeight;
};

DECLARE_HUDELEMENT( CHudBS2Credits );
DECLARE_HUD_MESSAGE( CHudBS2Credits, BS2Credits );

//-----------------------------------------------------------------------------
CHudBS2Credits::CHudBS2Credits( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudBS2Credits" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_bRunning = false;
	m_flStart = 0.0f;
	m_flSpeed = 55.0f;
	m_flFadeIn = 2.0f;
	m_flHoldAfter = 3.0f;
	m_bStopAtEnd = false;
	m_bDoneSent = false;
	m_flDoneTime = 0.0f;
	m_nLayoutW = 0;
	m_nLayoutH = 0;
	m_nTotalHeight = 0;
}

void CHudBS2Credits::Init( void )
{
	HOOK_HUD_MESSAGE( CHudBS2Credits, BS2Credits );
	Stop();
}

void CHudBS2Credits::LevelInit( void )
{
	Stop();
}

void CHudBS2Credits::LevelShutdown( void )
{
	Stop();
}

void CHudBS2Credits::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetVisible( ShouldDraw() );
	SetBgColor( Color( 0, 0, 0, 0 ) );

	m_nLayoutW = 0;		// the fonts are made again, lay out again
}

bool CHudBS2Credits::ShouldDraw( void )
{
	return m_bRunning;
}

void CHudBS2Credits::Stop( void )
{
	m_bRunning = false;
	m_Lines.RemoveAll();
	m_nLayoutW = 0;
}

//-----------------------------------------------------------------------------
void CHudBS2Credits::AddLine( const char *pszText, int nStyle )
{
	if ( m_Lines.Count() >= BS2CRED_MAX_LINES )
		return;

	BS2CreditLine_t line;
	memset( &line, 0, sizeof( line ) );
	Q_strncpy( line.szText, pszText, sizeof( line.szText ) );
	line.nStyle = nStyle;
	m_Lines.AddToTail( line );
}

//-----------------------------------------------------------------------------
// Purpose: read and classify the text file (see the header comment for the format)
//-----------------------------------------------------------------------------
bool CHudBS2Credits::Load( const char *pszFile )
{
	m_Lines.RemoveAll();

	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pszFile, "GAME", buf ) )
	{
		Warning( "bs2_credits: cannot read %s\n", pszFile );
		return false;
	}

	const int nLen = buf.TellPut();
	const char *pData = (const char *)buf.Base();
	if ( !pData || nLen <= 0 )
		return false;

	// is there a [roll] marker? then only the lines after it are used
	bool bHasRoll = false;
	{
		int nLineStart = 0;
		for ( int i = 0; i <= nLen; i++ )
		{
			if ( i == nLen || pData[i] == '\n' )
			{
				char szLine[BS2CRED_MAX_CHARS];
				int nCount = MIN( i - nLineStart, (int)sizeof( szLine ) - 1 );
				memcpy( szLine, pData + nLineStart, nCount );
				szLine[nCount] = '\0';
				Q_StripPrecedingAndTrailingWhitespace( szLine );
				if ( Q_stricmp( szLine, "[roll]" ) == 0 )
					bHasRoll = true;
				nLineStart = i + 1;
			}
		}
	}

	bool bActive = !bHasRoll;
	bool bFirst = true;
	int nLineStart = 0;

	for ( int i = 0; i <= nLen; i++ )
	{
		if ( i != nLen && pData[i] != '\n' )
			continue;

		char szRaw[BS2CRED_MAX_CHARS];
		int nCount = MIN( i - nLineStart, (int)sizeof( szRaw ) - 1 );
		memcpy( szRaw, pData + nLineStart, nCount );
		szRaw[nCount] = '\0';
		nLineStart = i + 1;

		// drop the '\r', tabs count as 4 spaces of indent
		int nIndent = 0;
		const char *p = szRaw;
		while ( *p == ' ' || *p == '\t' )
		{
			nIndent += ( *p == '\t' ) ? 4 : 1;
			p++;
		}

		char szText[BS2CRED_MAX_CHARS];
		Q_strncpy( szText, p, sizeof( szText ) );
		Q_StripPrecedingAndTrailingWhitespace( szText );

		if ( !bActive )
		{
			if ( Q_stricmp( szText, "[roll]" ) == 0 )
				bActive = true;
			continue;
		}

		if ( Q_stricmp( szText, "[roll]" ) == 0 )
			continue;

		if ( szText[0] == '\0' )
		{
			AddLine( "", CRED_BLANK );
			continue;
		}

		if ( szText[0] == '(' || szText[0] == '#' || Q_strnicmp( szText, "Per ", 4 ) == 0 )
			continue;

		int nStyle = CRED_HEADING;
		if ( bFirst )
			nStyle = CRED_TITLE;
		else if ( nIndent >= 8 )
			nStyle = CRED_DETAIL;
		else if ( nIndent >= 4 )
			nStyle = CRED_ENTRY;

		bFirst = false;
		AddLine( szText, nStyle );
	}

	// no more than one blank line in a row, none at the ends
	for ( int i = m_Lines.Count() - 1; i >= 0; i-- )
	{
		const bool bBlank = ( m_Lines[i].nStyle == CRED_BLANK );
		const bool bPrevBlank = ( i > 0 && m_Lines[i - 1].nStyle == CRED_BLANK );
		if ( bBlank && ( bPrevBlank || i == 0 || i == m_Lines.Count() - 1 ) )
		{
			m_Lines.Remove( i );
		}
	}

	return m_Lines.Count() > 0;
}

//-----------------------------------------------------------------------------
void CHudBS2Credits::StyleFont( int nStyle, vgui::HFont &hFont, Color &color ) const
{
	switch ( nStyle )
	{
	case CRED_TITLE:
		hFont = BS2_GetFont( BS2FONT_TITLE, 76 );
		color = Color( 235, 235, 235, 255 );
		break;
	case CRED_HEADING:
		hFont = BS2_GetFont( BS2FONT_TITLE, 40 );
		color = Color( 170, 170, 170, 255 );
		break;
	case CRED_DETAIL:
		hFont = BS2_GetFont( BS2FONT_SERIF, 24 );
		color = Color( 140, 140, 140, 255 );
		break;
	case CRED_ENTRY:
	default:
		hFont = BS2_GetFont( BS2FONT_SERIF, 30 );
		color = Color( 225, 225, 225, 255 );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: wrap the long lines and give every line its y
//-----------------------------------------------------------------------------
void CHudBS2Credits::Layout( int nScreenW, int nScreenH )
{
	const int nMaxWidth = (int)( nScreenW * 0.62f );
	const float flScale = (float)nScreenH / 1080.0f;

	// wrap: rebuild the list with extra lines where a line is too wide
	CUtlVector<BS2CreditLine_t> wrapped;

	for ( int i = 0; i < m_Lines.Count(); i++ )
	{
		BS2CreditLine_t line = m_Lines[i];
		vgui::HFont hFont = 0;
		Color color;
		StyleFont( line.nStyle, hFont, color );

		if ( !hFont || line.nStyle == CRED_BLANK )
		{
			wrapped.AddToTail( line );
			continue;
		}

		wchar_t wszAll[BS2CRED_MAX_CHARS];
		Q_UTF8ToUnicode( line.szText, wszAll, sizeof( wszAll ) );

		int nW, nH;
		surface()->GetTextSize( hFont, wszAll, nW, nH );
		if ( nW <= nMaxWidth )
		{
			wrapped.AddToTail( line );
			continue;
		}

		// break at the last space that still fits
		char szRest[BS2CRED_MAX_CHARS];
		Q_strncpy( szRest, line.szText, sizeof( szRest ) );

		int nGuard = 0;
		while ( szRest[0] != '\0' && nGuard++ < 8 )
		{
			wchar_t wszRest[BS2CRED_MAX_CHARS];
			Q_UTF8ToUnicode( szRest, wszRest, sizeof( wszRest ) );
			surface()->GetTextSize( hFont, wszRest, nW, nH );

			BS2CreditLine_t part = line;
			if ( nW <= nMaxWidth )
			{
				Q_strncpy( part.szText, szRest, sizeof( part.szText ) );
				wrapped.AddToTail( part );
				break;
			}

			// longest prefix ending at a space that fits
			int nCut = -1;
			const int nRestLen = Q_strlen( szRest );
			for ( int c = 1; c < nRestLen; c++ )
			{
				if ( szRest[c] != ' ' )
					continue;

				char szTry[BS2CRED_MAX_CHARS];
				Q_strncpy( szTry, szRest, c + 1 );
				szTry[c] = '\0';
				wchar_t wszTry[BS2CRED_MAX_CHARS];
				Q_UTF8ToUnicode( szTry, wszTry, sizeof( wszTry ) );
				surface()->GetTextSize( hFont, wszTry, nW, nH );
				if ( nW > nMaxWidth )
					break;
				nCut = c;
			}

			if ( nCut <= 0 )
			{
				// no space to break at: show it as it is
				Q_strncpy( part.szText, szRest, sizeof( part.szText ) );
				wrapped.AddToTail( part );
				break;
			}

			Q_strncpy( part.szText, szRest, nCut + 1 );
			part.szText[nCut] = '\0';
			wrapped.AddToTail( part );

			memmove( szRest, szRest + nCut + 1, nRestLen - nCut );		// includes the terminator
		}
	}

	m_Lines.RemoveAll();
	for ( int i = 0; i < wrapped.Count(); i++ )
	{
		m_Lines.AddToTail( wrapped[i] );
	}

	// y positions: a gap before headings, a small one between entries
	int y = 0;
	int nPrevStyle = -1;
	for ( int i = 0; i < m_Lines.Count(); i++ )
	{
		vgui::HFont hFont = 0;
		Color color;
		StyleFont( m_Lines[i].nStyle, hFont, color );
		const int nTall = hFont ? surface()->GetFontTall( hFont ) : (int)( 30 * flScale );

		if ( m_Lines[i].nStyle == CRED_BLANK )
		{
			y += (int)( 34 * flScale );
			nPrevStyle = CRED_BLANK;
			m_Lines[i].nY = y;
			continue;
		}

		if ( m_Lines[i].nStyle == CRED_HEADING && nPrevStyle != -1 && nPrevStyle != CRED_BLANK )
		{
			y += (int)( 46 * flScale );
		}

		m_Lines[i].nY = y;
		y += nTall + (int)( ( m_Lines[i].nStyle == CRED_TITLE ? 18 : 6 ) * flScale );
		nPrevStyle = m_Lines[i].nStyle;
	}

	m_nTotalHeight = y;
	m_nLayoutW = nScreenW;
	m_nLayoutH = nScreenH;
}

//-----------------------------------------------------------------------------
void CHudBS2Credits::Paint( void )
{
	if ( !m_bRunning )
		return;

	int nScreenW, nScreenH;
	GetHudSize( nScreenW, nScreenH );
	SetSize( nScreenW, nScreenH );
	SetPos( 0, 0 );

	if ( m_nLayoutW != nScreenW || m_nLayoutH != nScreenH )
	{
		Layout( nScreenW, nScreenH );
	}

	const float flNow = gpGlobals->curtime;
	const float flT = flNow - m_flStart;
	const float flScale = (float)nScreenH / 1080.0f;

	// black
	const float flBlack = ( m_flFadeIn > 0.0f ) ? clamp( flT / m_flFadeIn, 0.0f, 1.0f ) : 1.0f;
	surface()->DrawSetColor( 0, 0, 0, (int)( 255.0f * flBlack ) );
	surface()->DrawFilledRect( 0, 0, nScreenW, nScreenH );

	// the roll starts when the screen is half black, from just below the bottom edge
	float flScroll = MAX( 0.0f, flT - m_flFadeIn * 0.5f ) * m_flSpeed * flScale;

	const float flTop = nScreenH + 30.0f * flScale;
	float flFirstY = flTop - flScroll;

	// where the roll has run out
	float flEndScroll = flTop + m_nTotalHeight;		// the last line has left the top
	if ( m_bStopAtEnd )
	{
		flEndScroll = flTop + m_nTotalHeight - nScreenH * 0.5f;
	}

	if ( flScroll >= flEndScroll )
	{
		flScroll = flEndScroll;
		flFirstY = flTop - flScroll;

		if ( m_flDoneTime <= 0.0f )
		{
			m_flDoneTime = flNow;
		}
		if ( !m_bDoneSent && flNow - m_flDoneTime >= m_flHoldAfter )
		{
			m_bDoneSent = true;
			engine->ClientCmd_Unrestricted( "bs2_creditsdone" );
		}
	}

	const float flFadeLines = MIN( 1.0f, flT / MAX( m_flFadeIn, 0.01f ) );

	for ( int i = 0; i < m_Lines.Count(); i++ )
	{
		const BS2CreditLine_t &line = m_Lines[i];
		if ( line.nStyle == CRED_BLANK )
			continue;

		vgui::HFont hFont = 0;
		Color color;
		StyleFont( line.nStyle, hFont, color );
		if ( !hFont )
			continue;

		const int nTall = surface()->GetFontTall( hFont );
		const int y = (int)( flFirstY + line.nY );
		if ( y > nScreenH || y + nTall < 0 )
			continue;

		// a soft fade at the top and bottom edges
		float flEdge = 1.0f;
		const float flFromTop = (float)( y + nTall / 2 );
		const float flFromBottom = (float)nScreenH - flFromTop;
		const float flBand = 90.0f * flScale;
		if ( flFromTop < flBand )
			flEdge = clamp( flFromTop / flBand, 0.0f, 1.0f );
		else if ( flFromBottom < flBand )
			flEdge = clamp( flFromBottom / flBand, 0.0f, 1.0f );

		wchar_t wszLine[BS2CRED_MAX_CHARS];
		Q_UTF8ToUnicode( line.szText, wszLine, sizeof( wszLine ) );

		int nW, nH;
		surface()->GetTextSize( hFont, wszLine, nW, nH );

		surface()->DrawSetTextFont( hFont );
		surface()->DrawSetTextColor( color[0], color[1], color[2], (int)( color[3] * flEdge * flFadeLines ) );
		surface()->DrawSetTextPos( ( nScreenW - nW ) / 2, y );
		surface()->DrawUnicodeString( wszLine );
	}
}

//-----------------------------------------------------------------------------
void CHudBS2Credits::MsgFunc_BS2Credits( bf_read &msg )
{
	int nCmd = msg.ReadByte();

	if ( nCmd == BS2CREDITS_START )
	{
		char szFile[128];
		msg.ReadString( szFile, sizeof( szFile ) );

		m_flSpeed = msg.ReadFloat();
		m_flFadeIn = msg.ReadFloat();
		m_flHoldAfter = msg.ReadFloat();
		int nFlags = msg.ReadShort();
		m_bStopAtEnd = ( nFlags & 1 ) != 0;

		if ( Load( szFile ) )
		{
			m_flStart = gpGlobals->curtime;
			m_bDoneSent = false;
			m_flDoneTime = 0.0f;
			m_nLayoutW = 0;
			m_bRunning = true;
		}
		else
		{
			// no file: do not leave the map waiting for OnFinished
			engine->ClientCmd_Unrestricted( "bs2_creditsdone" );
		}
	}
	else
	{
		Stop();
	}
}
