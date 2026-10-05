//========= Black Stasis 2, Phase 2 A1: point_bs2_titlecard =========//
//
// Purpose: a title card / hint for the bs2_titlecard HUD element (client\bs2\bs2_hud_titlecard.cpp).
//
// A card is up to 8 text blocks. Block n is set with the keyvalues  text<n>  font<n>  size<n>  x<n>  y<n>  align<n>  delay<n>  fadein<n>  color<n>  alpha<n>
// (see sp\game\mod_episodic\bs2.fgd). The card as a whole has holdtime, fadeouttime, bgfade, style, blackbackground, forever.
// The text may hold '|' (line break) and %use% (the key bound to +use). A user message is limited to 255 bytes, so every block is sent on its own and then the
// Show message starts the card; the client queues cards that arrive while one is showing.
//
//=============================================================================//

#include "cbase.h"
#include "bs2/bs2_ui_messages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BS2CARD_MAX_BLOCKS	8
#define BS2CARD_MAX_TEXT	150		// characters sent per block

//-----------------------------------------------------------------------------
// Purpose: true when pszKey is pszPrefix followed by a number 1..BS2CARD_MAX_BLOCKS; nIndex is that number - 1
//-----------------------------------------------------------------------------
static bool BS2Card_ParseIndexed( const char *pszKey, const char *pszPrefix, int &nIndex )
{
	const int nLen = Q_strlen( pszPrefix );
	if ( Q_strnicmp( pszKey, pszPrefix, nLen ) != 0 )
		return false;

	const char *p = pszKey + nLen;
	if ( *p == '\0' )
		return false;

	for ( const char *q = p; *q != '\0'; ++q )
	{
		if ( *q < '0' || *q > '9' )
			return false;
	}

	nIndex = atoi( p ) - 1;
	return ( nIndex >= 0 && nIndex < BS2CARD_MAX_BLOCKS );
}

//-----------------------------------------------------------------------------
class CBS2TitleCard : public CPointEntity
{
public:
	DECLARE_CLASS( CBS2TitleCard, CPointEntity );
	DECLARE_DATADESC();

	CBS2TitleCard();

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

	void InputShow( inputdata_t &inputdata );
	void InputHide( inputdata_t &inputdata );
	void InputSkip( inputdata_t &inputdata );

private:
	void SendCommand( int nCmd );
	void ShowCard();

	string_t	m_iszText[BS2CARD_MAX_BLOCKS];
	int			m_iFont[BS2CARD_MAX_BLOCKS];
	int			m_iSize[BS2CARD_MAX_BLOCKS];
	float		m_flX[BS2CARD_MAX_BLOCKS];
	float		m_flY[BS2CARD_MAX_BLOCKS];
	int			m_iAlign[BS2CARD_MAX_BLOCKS];
	float		m_flDelay[BS2CARD_MAX_BLOCKS];
	float		m_flFadeIn[BS2CARD_MAX_BLOCKS];
	int			m_iRed[BS2CARD_MAX_BLOCKS];
	int			m_iGreen[BS2CARD_MAX_BLOCKS];
	int			m_iBlue[BS2CARD_MAX_BLOCKS];
	int			m_iAlpha[BS2CARD_MAX_BLOCKS];

	float		m_flHold;
	float		m_flFadeOut;
	float		m_flBgFade;
	int			m_iStyle;			// 0 = title card, 1 = the small bottom-right hint
	bool		m_bBlackBackground;
	bool		m_bForever;

	COutputEvent	m_OnShown;
};

LINK_ENTITY_TO_CLASS( point_bs2_titlecard, CBS2TitleCard );

BEGIN_DATADESC( CBS2TitleCard )

	DEFINE_ARRAY( m_iszText, FIELD_STRING, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iFont, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iSize, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_flX, FIELD_FLOAT, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_flY, FIELD_FLOAT, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iAlign, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_flDelay, FIELD_FLOAT, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_flFadeIn, FIELD_FLOAT, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iRed, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iGreen, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iBlue, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),
	DEFINE_ARRAY( m_iAlpha, FIELD_INTEGER, BS2CARD_MAX_BLOCKS ),

	DEFINE_KEYFIELD( m_flHold, FIELD_FLOAT, "holdtime" ),
	DEFINE_KEYFIELD( m_flFadeOut, FIELD_FLOAT, "fadeouttime" ),
	DEFINE_KEYFIELD( m_flBgFade, FIELD_FLOAT, "bgfade" ),
	DEFINE_KEYFIELD( m_iStyle, FIELD_INTEGER, "style" ),
	DEFINE_KEYFIELD( m_bBlackBackground, FIELD_BOOLEAN, "blackbackground" ),
	DEFINE_KEYFIELD( m_bForever, FIELD_BOOLEAN, "forever" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Show", InputShow ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Hide", InputHide ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Skip", InputSkip ),

	DEFINE_OUTPUT( m_OnShown, "OnShown" ),

END_DATADESC()

//-----------------------------------------------------------------------------
CBS2TitleCard::CBS2TitleCard()
{
	for ( int i = 0; i < BS2CARD_MAX_BLOCKS; i++ )
	{
		m_iszText[i] = NULL_STRING;
		m_iFont[i] = BS2FONT_TITLE;
		m_iSize[i] = 72;
		m_flX[i] = 0.5f;
		m_flY[i] = 0.5f;
		m_iAlign[i] = BS2ALIGN_CENTER;
		m_flDelay[i] = 0.0f;
		m_flFadeIn[i] = 1.0f;
		m_iRed[i] = m_iGreen[i] = m_iBlue[i] = 255;
		m_iAlpha[i] = 255;
	}

	m_flHold = 4.0f;
	m_flFadeOut = 1.5f;
	m_flBgFade = 0.0f;
	m_iStyle = 0;
	m_bBlackBackground = false;
	m_bForever = false;
}

//-----------------------------------------------------------------------------
// Purpose: the numbered block keys (text1 .. text8 and so on); everything else is the datadesc's
//-----------------------------------------------------------------------------
bool CBS2TitleCard::KeyValue( const char *szKeyName, const char *szValue )
{
	int i = 0;

	if ( BS2Card_ParseIndexed( szKeyName, "text", i ) )
	{
		m_iszText[i] = AllocPooledString( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "font", i ) )
	{
		m_iFont[i] = atoi( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "size", i ) )
	{
		m_iSize[i] = atoi( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "x", i ) )
	{
		m_flX[i] = atof( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "y", i ) )
	{
		m_flY[i] = atof( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "align", i ) )
	{
		m_iAlign[i] = atoi( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "delay", i ) )
	{
		m_flDelay[i] = atof( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "fadein", i ) )
	{
		m_flFadeIn[i] = atof( szValue );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "color", i ) )
	{
		int r = 255, g = 255, b = 255;
		sscanf( szValue, "%d %d %d", &r, &g, &b );
		m_iRed[i] = clamp( r, 0, 255 );
		m_iGreen[i] = clamp( g, 0, 255 );
		m_iBlue[i] = clamp( b, 0, 255 );
	}
	else if ( BS2Card_ParseIndexed( szKeyName, "alpha", i ) )
	{
		m_iAlpha[i] = clamp( atoi( szValue ), 0, 255 );
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: a header-only message (Clear / Hide / Skip with this entity's id)
//-----------------------------------------------------------------------------
void CBS2TitleCard::SendCommand( int nCmd )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	UserMessageBegin( user, "BS2Card" );
		WRITE_BYTE( nCmd );
		WRITE_SHORT( entindex() );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: every block with text, then the Show message
//-----------------------------------------------------------------------------
void CBS2TitleCard::ShowCard()
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	if ( !pPlayer )
		return;

	CSingleUserRecipientFilter user( pPlayer );
	user.MakeReliable();

	for ( int i = 0; i < BS2CARD_MAX_BLOCKS; i++ )
	{
		if ( m_iszText[i] == NULL_STRING || STRING( m_iszText[i] )[0] == '\0' )
			continue;

		char szText[BS2CARD_MAX_TEXT + 1];
		Q_strncpy( szText, STRING( m_iszText[i] ), sizeof( szText ) );

		UserMessageBegin( user, "BS2Card" );
			WRITE_BYTE( BS2CARD_BLOCK );
			WRITE_SHORT( entindex() );
			WRITE_STRING( szText );
			WRITE_BYTE( m_iFont[i] );
			WRITE_SHORT( m_iSize[i] );
			WRITE_SHORT( (int)( m_flX[i] * 1000.0f ) );
			WRITE_SHORT( (int)( m_flY[i] * 1000.0f ) );
			WRITE_BYTE( m_iAlign[i] );
			WRITE_SHORT( (int)( m_flDelay[i] * 100.0f ) );
			WRITE_SHORT( (int)( m_flFadeIn[i] * 100.0f ) );
			WRITE_BYTE( m_iRed[i] );
			WRITE_BYTE( m_iGreen[i] );
			WRITE_BYTE( m_iBlue[i] );
			WRITE_BYTE( m_iAlpha[i] );
		MessageEnd();
	}

	int nFlags = 0;
	if ( m_iStyle == 1 )
		nFlags |= BS2CARDF_HINT;
	if ( m_bBlackBackground )
		nFlags |= BS2CARDF_BLACKBG;
	if ( m_bForever )
		nFlags |= BS2CARDF_FOREVER;

	UserMessageBegin( user, "BS2Card" );
		WRITE_BYTE( BS2CARD_SHOW );
		WRITE_SHORT( entindex() );
		WRITE_SHORT( (int)( m_flHold * 100.0f ) );
		WRITE_SHORT( (int)( m_flFadeOut * 100.0f ) );
		WRITE_SHORT( nFlags );
		WRITE_SHORT( (int)( m_flBgFade * 100.0f ) );
	MessageEnd();
}

void CBS2TitleCard::InputShow( inputdata_t &inputdata )
{
	ShowCard();
	m_OnShown.FireOutput( inputdata.pActivator, this );
}

void CBS2TitleCard::InputHide( inputdata_t &inputdata )
{
	SendCommand( BS2CARD_HIDE );
}

void CBS2TitleCard::InputSkip( inputdata_t &inputdata )
{
	SendCommand( BS2CARD_SKIP );
}
