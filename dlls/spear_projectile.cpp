#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "gamerules.h"

#ifndef CLIENT_DLL

static const char *const SPEAR_MDL = "models/spear.mdl";
static const char *const TRAIL_SPR = "sprites/streak.spr";
static const char *const IMPACT_SND = "weapons/spear_impact.wav";

//====================================================================
// Aynekko - this is basically crossbow bolt with some changes
// used by monster_mercenary_frogman
//====================================================================

LINK_ENTITY_TO_CLASS( spear_projectile, CSpearProjectile );

CSpearProjectile *CSpearProjectile::SpearCreate()
{
	// Create a new entity with CSpearProjectile private data
	CSpearProjectile *pSpear = GetClassPtr( (CSpearProjectile *)nullptr );
	pSpear->pev->classname = MAKE_STRING( "spear_projectile" );
	pSpear->Spawn();

	return pSpear;
}

void CSpearProjectile::Spawn()
{
	Precache();
	pev->movetype = MOVETYPE_FLY;
	pev->solid = SOLID_BBOX;

	pev->gravity = 0.5;

	SET_MODEL( ENT( pev ), SPEAR_MDL );

	UTIL_SetOrigin( this, pev->origin );
	UTIL_SetSize( pev, Vector( 0, 0, 0 ), Vector( 0, 0, 0 ) );

	SetTouch( &CSpearProjectile::SpearTouch );
	SetThink( &CSpearProjectile::FlyThink );
	SetNextThink( 0.1 );
}

void CSpearProjectile::Precache()
{
	PRECACHE_MODEL( SPEAR_MDL );
	PRECACHE_SOUND( "weapons/xbow_hitbod1.wav" );
	PRECACHE_SOUND( "weapons/xbow_hitbod2.wav" );
//	PRECACHE_SOUND( "weapons/xbow_fly1.wav" );
	PRECACHE_SOUND( IMPACT_SND );
	m_iTrail = PRECACHE_MODEL( TRAIL_SPR );
	bTrailCreated = false;
}


int	CSpearProjectile::Classify()
{
	return	CLASS_NONE;
}

void CSpearProjectile::SpearTouch( CBaseEntity *pOther )
{
	SetTouch( nullptr );
	SetThink( nullptr );

	if( pOther->pev->takedamage )
	{
		TraceResult tr = UTIL_GetGlobalTrace();
		entvars_t *pevOwner;

		pevOwner = VARS( pev->owner );

		// UNDONE: this needs to call TraceAttack instead
		ClearMultiDamage();

		pOther->TraceAttack( pevOwner, gSkillData.mercDmgSpear, pev->velocity.Normalize(), &tr, DMG_NEVERGIB );

		ApplyMultiDamage( pev, pevOwner );

		pev->velocity = Vector( 0, 0, 0 );
		// play body "thwack" sound
		switch( RANDOM_LONG( 0, 1 ) )
		{
		case 0:
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/xbow_hitbod1.wav", 1, ATTN_NORM ); break;
		case 1:
			EMIT_SOUND( ENT( pev ), CHAN_BODY, "weapons/xbow_hitbod2.wav", 1, ATTN_NORM ); break;
		}

		if( !g_pGameRules->IsMultiplayer() )
		{
			Killed( pev, GIB_NEVER );
		}
	}
	else
	{
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, IMPACT_SND, RANDOM_FLOAT( 0.95, 1.0 ), ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 7 ) );

		SetThink( &CSpearProjectile::SUB_Remove );
		SetNextThink( 0 );// this will get changed below if the bolt is allowed to stick in what it hit.

		if( FClassnameIs( pOther->pev, "worldspawn" ) )
		{
			// if what we hit is static architecture, can stay around for a while.
			Vector vecDir = pev->velocity.Normalize();
			UTIL_SetOrigin( this, pev->origin - vecDir * 12 );
			pev->angles = UTIL_VecToAngles( vecDir );
			pev->solid = SOLID_NOT;
			pev->movetype = MOVETYPE_FLY;
			pev->velocity = Vector( 0, 0, 0 );
			pev->avelocity.z = 0;
			pev->angles.z = RANDOM_LONG( 0, 360 );
			SetNextThink( 10.0 );
		}

		if( UTIL_PointContents( pev->origin ) != CONTENTS_WATER )
		{
			UTIL_Sparks( pev->origin );
		}
	}
}

void CSpearProjectile::FlyThink()
{
	SetNextThink( 0.1 );

	// create trail
	if( !bTrailCreated && m_iTrail )
	{
		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			WRITE_BYTE( TE_BEAMFOLLOW );
			WRITE_SHORT( entindex() );	// entity
			WRITE_SHORT( m_iTrail );	// model
			WRITE_BYTE( 4 ); // life
			WRITE_BYTE( 1 );  // width
			WRITE_BYTE( 50 );   // r
			WRITE_BYTE( 50 );   // g
			WRITE_BYTE( 50 );   // b
			WRITE_BYTE( 125 );	// brightness
		MESSAGE_END();

		bTrailCreated = true;
	}

	// handle water
	if( pev->waterlevel == 0 || pev->watertype <= CONTENT_FLYFIELD )
		return;

	UTIL_BubbleTrail( pev->origin - pev->velocity * 0.1, pev->origin, 1 );
}

#endif // #ifndef CLIENT_DLL