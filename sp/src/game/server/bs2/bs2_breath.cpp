//========= Black Stasis 2, Phase 2 A4: black breath mist =========//
// See bs2_breath.h.
//=============================================================================//

#include "cbase.h"
#include "particle_parse.h"
#include "bs2/bs2_breath.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define BS2_BREATH_PERIOD		4.0f		// seconds between two breaths
#define BS2_BREATH_PERIOD_JIT	0.35f

//-----------------------------------------------------------------------------
class CBS2BreathEmitter : public CServerOnlyEntity
{
public:
	DECLARE_CLASS( CBS2BreathEmitter, CServerOnlyEntity );
	DECLARE_DATADESC();

	CBS2BreathEmitter()
	{
		m_bEnabled = true;
		m_bLarge = false;
		m_iszAttachment = NULL_STRING;
	}

	void Setup( CBaseEntity *pOwner, const char *pszAttachment )
	{
		m_hOwner = pOwner;
		m_iszAttachment = AllocPooledString( pszAttachment );
	}

	CBaseEntity *GetOwner_( void ) { return m_hOwner.Get(); }
	void SetEnabled( bool bEnabled ) { m_bEnabled = bEnabled; }
	void SetLarge( bool bLarge ) { m_bLarge = bLarge; }

	void BreathThink( void )
	{
		CBaseEntity *pOwner = m_hOwner.Get();
		if ( !pOwner || !pOwner->IsAlive() )
		{
			UTIL_Remove( this );
			return;
		}

		if ( m_bEnabled )
		{
			DispatchParticleEffect( m_bLarge ? "bs2_breath_black_large" : "bs2_breath_black", PATTACH_POINT_FOLLOW, pOwner, STRING( m_iszAttachment ) );
		}

		SetNextThink( gpGlobals->curtime + BS2_BREATH_PERIOD + random->RandomFloat( -BS2_BREATH_PERIOD_JIT, BS2_BREATH_PERIOD_JIT ) );
	}

private:
	EHANDLE		m_hOwner;
	string_t	m_iszAttachment;
	bool		m_bEnabled;
	bool		m_bLarge;
};

LINK_ENTITY_TO_CLASS( bs2_breath_emitter, CBS2BreathEmitter );

BEGIN_DATADESC( CBS2BreathEmitter )
	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iszAttachment, FIELD_STRING ),
	DEFINE_FIELD( m_bEnabled, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bLarge, FIELD_BOOLEAN ),
	DEFINE_THINKFUNC( BreathThink ),
END_DATADESC()

//-----------------------------------------------------------------------------
void BS2Breath_Precache( void )
{
	PrecacheParticleSystem( "bs2_breath_black" );
	PrecacheParticleSystem( "bs2_breath_black_large" );
}

void BS2Breath_Start( CBaseAnimating *pOwner, const char *pszAttachment )
{
	if ( !pOwner || !pszAttachment )
		return;

	CBS2BreathEmitter *pEmitter = (CBS2BreathEmitter *)CreateEntityByName( "bs2_breath_emitter" );
	if ( !pEmitter )
		return;

	pEmitter->Setup( pOwner, pszAttachment );
	DispatchSpawn( pEmitter );

	pEmitter->SetThink( &CBS2BreathEmitter::BreathThink );
	pEmitter->SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.6f, 2.4f ) );
}

//-----------------------------------------------------------------------------
// Purpose: run a function on every emitter that belongs to pOwner
//-----------------------------------------------------------------------------
template <class F>
static void BS2Breath_ForEach( CBaseEntity *pOwner, F func )
{
	CBaseEntity *pEntity = gEntList.FindEntityByClassname( NULL, "bs2_breath_emitter" );
	while ( pEntity )
	{
		CBaseEntity *pNext = gEntList.FindEntityByClassname( pEntity, "bs2_breath_emitter" );
		CBS2BreathEmitter *pEmitter = dynamic_cast<CBS2BreathEmitter *>( pEntity );
		if ( pEmitter && pEmitter->GetOwner_() == pOwner )
		{
			func( pEmitter );
		}
		pEntity = pNext;
	}
}

static void BS2Breath_DoEnable( CBS2BreathEmitter *p ) { p->SetEnabled( true ); }
static void BS2Breath_DoDisable( CBS2BreathEmitter *p ) { p->SetEnabled( false ); }
static void BS2Breath_DoLarge( CBS2BreathEmitter *p ) { p->SetLarge( true ); }
static void BS2Breath_DoSmall( CBS2BreathEmitter *p ) { p->SetLarge( false ); }
static void BS2Breath_DoRemove( CBS2BreathEmitter *p ) { UTIL_Remove( p ); }

void BS2Breath_SetEnabled( CBaseEntity *pOwner, bool bEnabled )
{
	BS2Breath_ForEach( pOwner, bEnabled ? BS2Breath_DoEnable : BS2Breath_DoDisable );
}

void BS2Breath_SetLarge( CBaseEntity *pOwner, bool bLarge )
{
	BS2Breath_ForEach( pOwner, bLarge ? BS2Breath_DoLarge : BS2Breath_DoSmall );
}

void BS2Breath_Stop( CBaseEntity *pOwner )
{
	BS2Breath_ForEach( pOwner, BS2Breath_DoRemove );
}
