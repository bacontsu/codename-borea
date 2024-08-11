/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "nodes.h"
#include "player.h"
#include "soundent.h"
#include "movewith.h"

#define	HANDGRENADE_PRIMARY_VOLUME		450

LINK_ENTITY_TO_CLASS( weapon_molotov, CMolotov );


void CMolotov::Spawn( )
{
	Precache( );
	m_iId = WEAPON_PENGUIN;
	SET_MODEL(ENT(pev), "models/w_molotov.mdl");

#ifndef CLIENT_DLL
	pev->dmg = gSkillData.plrDmgHandGrenade;
#endif

	m_iDefaultAmmo = HANDGRENADE_DEFAULT_GIVE;

	FallInit();// get ready to fall down.
}


void CMolotov::Precache()
{
	PRECACHE_MODEL("models/w_molotov.mdl");
	PRECACHE_MODEL("models/v_molotov.mdl");
	PRECACHE_MODEL("models/p_molotov.mdl");

	PRECACHE_SOUND( "weapons/molotov_break.wav" );
	PRECACHE_SOUND( "props/burning4.wav" );

	UTIL_PrecacheOther( "fire" );
}

int CMolotov::GetItemInfo(ItemInfo *p)
{
	p->pszName = STRING(pev->classname);
	p->pszAmmo1 = "Molotov";
	p->iMaxAmmo1 = HANDGRENADE_MAX_CARRY;
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = -1;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 3;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_PENGUIN; // Aynekko: I'll reuse this, thanks...
	p->iWeight = HANDGRENADE_WEIGHT;
	p->iFlags = ITEM_FLAG_LIMITINWORLD | ITEM_FLAG_EXHAUSTIBLE;

	return 1;
}

void CMolotov::IncrementAmmo(CBasePlayer* pPlayer)
{
#ifndef CLIENT_DLL
	//TODO: not sure how useful this is given that the player has to have this weapon for this method to be called
	if (!pPlayer->HasNamedPlayerItem("weapon_molotov"))
	{
		pPlayer->GiveNamedItem("weapon_molotov");
	}
#endif

	if (pPlayer->GiveAmmo(1, "Molotov", HANDGRENADE_MAX_CARRY))
	{
		EMIT_SOUND(pPlayer->edict(), CHAN_STATIC, "ctf/pow_backpack.wav", 0.5, ATTN_NORM);
	}
}

BOOL CMolotov::Deploy( )
{
	m_flReleaseThrow = -1;
	return DefaultDeploy( "models/v_molotov.mdl", "models/p_molotov.mdl", HANDGRENADE_DRAW, "crowbar" );
}

BOOL CMolotov::CanHolster()
{
	// can only holster hand grenades when not primed!
	return ( m_flStartThrow == 0 );
}

void CMolotov::Holster( int skiplocal /* = 0 */ )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
	{
		SendWeaponAnim( HANDGRENADE_HOLSTER );
	}
	else
	{
		// no more grenades!
		m_pPlayer->pev->weapons &= ~(1<<WEAPON_PENGUIN);
		SetThink( &CHandGrenade::DestroyItem );
		SetNextThink( 0.1 );
	}

	EMIT_SOUND(ENT(m_pPlayer->pev), CHAN_WEAPON, "common/null.wav", 1.0, ATTN_NORM);
}

void CMolotov::PrimaryAttack()
{
	if ( !m_flStartThrow && m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] > 0 )
	{
		m_flStartThrow = gpGlobals->time;
		m_flReleaseThrow = 0;

		SendWeaponAnim( HANDGRENADE_PINPULL );
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;
	}
}


void CMolotov::WeaponIdle()
{
	if ( m_flReleaseThrow == 0 && m_flStartThrow )
		 m_flReleaseThrow = gpGlobals->time;

	if ( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if ( m_flStartThrow )
	{
		Vector angThrow = m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle;

		if ( angThrow.x < 0 )
			angThrow.x = -10 + angThrow.x * ( ( 90 - 10 ) / 90.0 );
		else
			angThrow.x = -10 + angThrow.x * ( ( 90 + 10 ) / 90.0 );

		float flVel = ( 90 - angThrow.x ) * 4;
		if ( flVel > 500 )
			flVel = 500;

		UTIL_MakeVectors( angThrow );

		Vector vecSrc = m_pPlayer->pev->origin + m_pPlayer->pev->view_ofs + gpGlobals->v_forward * 16;

		Vector vecThrow = gpGlobals->v_forward * flVel + m_pPlayer->pev->velocity;

		// alway explode 3 seconds after the pin was pulled
		float time = m_flStartThrow - gpGlobals->time + 3.0;
		if (time < 0)
			time = 0;

		CGrenade::ShootMolotov( m_pPlayer->pev, vecSrc, vecThrow, time );

		if ( flVel < 500 )
		{
			SendWeaponAnim( HANDGRENADE_THROW1 );
		}
		else if ( flVel < 1000 )
		{
			SendWeaponAnim( HANDGRENADE_THROW2 );
		}
		else
		{
			SendWeaponAnim( HANDGRENADE_THROW3 );
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		//m_flReleaseThrow = 0;
		m_flStartThrow = 0;
		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 0.5;

		m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ]--;

		if ( !m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			// just threw last grenade
			// set attack times in the future, and weapon idle in the future so we can see the whole throw
			// animation, weapon idle will automatically retire the weapon for us.
			m_flTimeWeaponIdle = m_flNextSecondaryAttack = m_flNextPrimaryAttack = GetNextAttackDelay(0.5);// ensure that the animation can finish playing
		}
		return;
	}
	else if ( m_flReleaseThrow > 0 )
	{
		// we've finished the throw, restart.
		m_flStartThrow = 0;

		if ( m_pPlayer->m_rgAmmo[ m_iPrimaryAmmoType ] )
		{
			SendWeaponAnim( HANDGRENADE_DRAW );
		}
		else
		{
			RetireWeapon();
			return;
		}

		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );
		m_flReleaseThrow = -1;
		return;
	}

	if ( m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] )
	{
		int iAnim;
		float flRand = UTIL_SharedRandomFloat( m_pPlayer->random_seed, 0, 1 );
		if (flRand <= 0.75)
		{
			iAnim = HANDGRENADE_IDLE;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat( m_pPlayer->random_seed, 10, 15 );// how long till we do this again.
		}
		else 
		{
			iAnim = HANDGRENADE_FIDGET;
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 75.0 / 30.0;
		}

		SendWeaponAnim( iAnim );
	}
}




#ifndef  CLIENT_DLL // sigh...


// fire entity!!!
class CFire : public CPointEntity
{
public:
	void	Spawn() override;
	void	Precache() override;
	void Think() override;

	float BurnStartTime;
	float InsertSoundTime;

	int	Save( CSave &save ) override;
	int	Restore( CRestore &restore ) override;
	static	TYPEDESCRIPTION m_SaveData[];
};

const int BurningTime = 10; // tune this if needed - fire burns for 10 seconds then dies
// also change it in CGrenade::MolotovExplode !!!

LINK_ENTITY_TO_CLASS( fire, CFire );

TYPEDESCRIPTION	CFire::m_SaveData[] =
{
	DEFINE_FIELD( CFire, BurnStartTime, FIELD_FLOAT ),
	DEFINE_FIELD( CFire, InsertSoundTime, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CFire, CPointEntity );

// Landmark class
void CFire::Spawn()
{
	Precache();
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->gravity = RANDOM_FLOAT( 1.0, 1.5 );
//	UTIL_SetSize( pev, Vector( -16, -16, 0 ), Vector( 16, 16, 32 ) );
	pev->dmgtime = gpGlobals->time + 0.5; // half a second before starting doing damage, give monsters a chance!
	pev->fuser1 = 0.0f; // this will be sound delay

	BurnStartTime = gpGlobals->time; // start burning
	InsertSoundTime = gpGlobals->time; // insert sound every 2-3 seconds
	pev->nextthink = gpGlobals->time + RANDOM_FLOAT( 0.08, 0.16 );

//	SET_MODEL( ENT( pev ), "sprites/pravafire.spr" );
//	pev->rendermode = kRenderTransAdd;
//	pev->rendercolor = Vector( 255, 180, 0 );
//	pev->renderamt = 255;
//	pev->iuser1 = (float)MODEL_FRAMES( pev->modelindex ) - 1;
}

void CFire::Precache()
{
//	PRECACHE_MODEL( "sprites/pravafire.spr" );
}

void CFire::Think( void )
{
	// time's up or hit water, extinguish
	if( pev->waterlevel > 0 || gpGlobals->time > BurnStartTime + BurningTime )
	{
		if( pev->fuser1 > 0.0f )
			STOP_SOUND( ENT( pev ), CHAN_BODY, "props/burning4.wav" );
		DontThink();
		UTIL_Remove( this );
		return;
	}
	
	// send particle here
	UTIL_Particle("flames_tlg.txt", pev->origin, g_vecZero, 0);
//	if( pev->frame >= pev->iuser1 )
//		pev->frame = 0;
//	pev->frame++;

	// notify the world about fire presence
	if( gpGlobals->time > InsertSoundTime )
	{
		CSoundEnt::InsertSound( bits_SOUND_FIRE | bits_SOUND_DANGER, pev->origin, 300, 1.0 );
		InsertSoundTime = gpGlobals->time + RANDOM_FLOAT( 1.5, 3.0 );
	}

	if( pev->fuser1 > 0.0f && gpGlobals->time > pev->fuser1 )
		EMIT_SOUND_DYN( ENT( pev ), CHAN_BODY, "props/burning4.wav", 1.0, ATTN_NORM, SND_CHANGE_PITCH | SND_CHANGE_VOL, 100 );

	// set monsters on fire
	if( gpGlobals->time > pev->dmgtime )
	{
		CBaseEntity *pOther = nullptr;
		while( (pOther = UTIL_FindEntityInSphere( pOther, pev->origin, 80 )) != nullptr )
		{
			if( pOther->IsPlayer() )
			{
				// smash the player with fire damage
				pOther->TakeDamage( VARS( eoNullEntity ), VARS( eoNullEntity ), gSkillData.firepersecDmg * 0.01f, DMG_BURN );
				continue;
			}

			if( !(pOther->pev->flags & FL_MONSTER) || (pOther->pev->deadflag != DEAD_NO) || (pOther->m_iLFlags & LF_BURNING_IMMUNE) )
				continue;

			pOther->m_iLFlags |= LF_BURNING;
		}
	}

	pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1,0.2);
}

#endif // ! CLIENT_DLL