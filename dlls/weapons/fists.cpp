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

// Aynekko: this is weapon_fists

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "monsters.h"
#include "weapons.h"
#include "CPipewrench.h"
#include "nodes.h"
#include "player.h"
#include "gamerules.h"

#define	PIPEWRENCH_BODYHIT_VOLUME 128
#define	PIPEWRENCH_WALLHIT_VOLUME 512

#define ANIM_FISTS_RIGHTSTART 11
#define ANIM_FISTS_RIGHTEND 12
#define ANIM_FISTS_LEFTSTART 15
#define ANIM_FISTS_LEFTEND 16

#ifndef CLIENT_DLL
TYPEDESCRIPTION	CPipewrench::m_SaveData[] =
{
	DEFINE_FIELD( CPipewrench, m_flBigSwingStart, FIELD_TIME ),
	DEFINE_FIELD( CPipewrench, m_iSwing, FIELD_INTEGER ),
	DEFINE_FIELD( CPipewrench, m_iSwingMode, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CPipewrench, CPipewrench::BaseClass );
#endif

LINK_ENTITY_TO_CLASS( weapon_pipewrench, CPipewrench );
LINK_ENTITY_TO_CLASS( weapon_fists, CPipewrench );

void CPipewrench::Spawn()
{
	pev->classname = MAKE_STRING( "weapon_fists" );
	Precache();
	m_iId = WEAPON_FISTS;
	SET_MODEL( edict(), "models/w_fists.mdl");
	m_iClip = WEAPON_NOCLIP;
	m_iSwingMode = SWING_NONE;

	FallInit();// get ready to fall down.
}

void CPipewrench::Precache()
{
	PRECACHE_MODEL("models/v_fists.mdl");
	PRECACHE_MODEL("models/w_fists.mdl");
	PRECACHE_MODEL("models/p_fists.mdl");
	// Shepard - The commented sounds below are unused
	// in Opposing Force, if you wish to use them,
	// uncomment all the appropriate lines.
	/*PRECACHE_SOUND("weapons/pwrench_big_hit1.wav");
	PRECACHE_SOUND("weapons/pwrench_big_hit2.wav");*/
	PRECACHE_SOUND("weapons/pwrench_big_hitbod1.wav");
	PRECACHE_SOUND("weapons/pwrench_big_hitbod2.wav");
	PRECACHE_SOUND("weapons/pwrench_hit1.wav");
	PRECACHE_SOUND("weapons/pwrench_hit2.wav");
	PRECACHE_SOUND("weapons/pwrench_hitbod1.wav");
	PRECACHE_SOUND("weapons/pwrench_hitbod2.wav");
	PRECACHE_SOUND("weapons/pwrench_hitbod3.wav");
	PRECACHE_SOUND( "weapons/melee_fist.wav" );

	m_usPipewrench = PRECACHE_EVENT ( 1, "events/pipewrench.sc" );
}

// attack_state
#define ATTACK_LMB_SMALL 1
#define ATTACK_LMB_BIG 2
#define ATTACK_RMB_SMALL 3
#define ATTACK_RMB_BIG 4
#define ATTACK_IDLE 5

//#define FISTS_DBG_MSG

BOOL CPipewrench::Deploy()
{
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.15 );
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.15 );
	attack_state = ATTACK_IDLE;
	time_counter = 0;
	bDidStartAnim = false;
	m_flTimeWeaponIdle = gpGlobals->time + 1;
	return DefaultDeploy( "models/v_fists.mdl", "models/p_fists.mdl", PIPEWRENCH_DRAW, "crowbar" );
}

void CPipewrench::Holster( int skiplocal )
{
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;
	SendWeaponAnim( PIPEWRENCH_HOLSTER );
}

void CPipewrench::PrimaryAttack()
{
	attack_state = ATTACK_LMB_SMALL;

	time_counter += gpGlobals->frametime;
	if( time_counter > 0.25f )
		attack_state = ATTACK_LMB_BIG;

	if( attack_state == ATTACK_LMB_BIG && !bDidStartAnim )
	{
		SendWeaponAnim( ANIM_FISTS_LEFTSTART );
		bDidStartAnim = true;
	}

	m_flTimeWeaponIdle = 0;
	m_flNextSecondaryAttack = GetNextAttackDelay( 0.15 );

#ifdef FISTS_DBG_MSG
	ALERT( at_console, "LMB pressed!\n" );
#endif
}

void CPipewrench::SecondaryAttack()
{
	attack_state = ATTACK_RMB_SMALL;

	time_counter += gpGlobals->frametime;
	if( time_counter > 0.25f )
		attack_state = ATTACK_RMB_BIG;

	if( attack_state == ATTACK_RMB_BIG && !bDidStartAnim )
	{
		SendWeaponAnim( ANIM_FISTS_RIGHTSTART );
		bDidStartAnim = true;
	}

	m_flTimeWeaponIdle = 0;
	m_flNextPrimaryAttack = GetNextAttackDelay( 0.15 );

#ifdef FISTS_DBG_MSG
	ALERT( at_console, "RMB pressed!\n" );
#endif
}

void CPipewrench::WeaponIdle()
{
#ifndef CLIENT_DLL
	if( m_flTimeWeaponIdle > UTIL_WeaponTimeBase() )
		return;

	if( !m_pPlayer )
		return;

	if( !attack_state )
	{
		SendWeaponAnim( 0 );
		attack_state = ATTACK_IDLE;
	}

	DoAttack();

#endif // !CLIENT_DLL
}

void CPipewrench::DoAttack()
{
#ifndef CLIENT_DLL
	if( !attack_state || attack_state == ATTACK_IDLE )
		return;

#ifdef FISTS_DBG_MSG
	ALERT( at_console, "DoAttack()\n" );
#endif

	bool bDidHit = false;

	// setup damage values
	int Damage = 0;
	switch( attack_state )
	{
	case ATTACK_LMB_SMALL:
		Damage = gSkillData.punchDmg; // left hand punch
		break;
	case ATTACK_RMB_SMALL:
		Damage = gSkillData.punchDmg; // right hand punch
		break;
	case ATTACK_LMB_BIG:
		Damage = gSkillData.punch2Dmg; // left hand punch
		break;
	case ATTACK_RMB_BIG:
		Damage = gSkillData.punch2Dmg; // right hand punch
		break;
	}
	if( !Damage )
		ALERT( at_console, "ERROR: Zero damage for fists!" );

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecEnd = vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

	if( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}

	switch( attack_state )
	{
	case ATTACK_LMB_SMALL:
		SendWeaponAnim( 5 ); // left hand punch
		break;
	case ATTACK_RMB_SMALL:
		SendWeaponAnim( 7 ); // right hand punch
		break;
	case ATTACK_LMB_BIG:
		SendWeaponAnim( ANIM_FISTS_LEFTEND ); // left hand punch
		break;
	case ATTACK_RMB_BIG:
		SendWeaponAnim( ANIM_FISTS_RIGHTEND ); // right hand punch
		break;
	}

	if( tr.flFraction >= 1.0 )
	{
		EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "zombie/claw_miss2.wav", 1, ATTN_NORM );
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}
	else
	{
		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );

		// screen shake
		if( attack_state == ATTACK_LMB_BIG || attack_state == ATTACK_RMB_BIG )
			UTIL_ScreenShake( m_pPlayer->pev->origin, 0.01, 100, 0.5, 300 );

		// hit
		bDidHit = true;
		CBaseEntity *pEntity = CBaseEntity::Instance( tr.pHit );

		if( pEntity )
		{
			ClearMultiDamage();

			pEntity->TraceAttack( m_pPlayer->pev, Damage, gpGlobals->v_forward, &tr, DMG_CLUB );

			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
		}

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		bool bHitWorld = true;

		if( pEntity )
		{
			if( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG( 0, 2 ) )
				{
				case 0:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod1.wav", 1, ATTN_NORM ); break;
				case 1:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod2.wav", 1, ATTN_NORM ); break;
				case 2:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod3.wav", 1, ATTN_NORM ); break;
				}
				m_pPlayer->m_iWeaponVolume = PIPEWRENCH_BODYHIT_VOLUME;
				if( !pEntity->IsAlive() )
				{
					// nothing
				}
				else
					flVol = 0.1;

				bHitWorld = false;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if( bHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound( &tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_PLAYER_CROWBAR );

			if( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play pipe wrench strike
			switch( RANDOM_LONG( 0, 1 ) )
			{
			case 0:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			case 1:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * PIPEWRENCH_WALLHIT_VOLUME;

		//RENDERERS START
		DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR, vecSrc, vecEnd );
		//RENDERERS END
	}

	if( attack_state == ATTACK_LMB_SMALL || attack_state == ATTACK_RMB_SMALL )
	{
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.15 );
		m_flNextSecondaryAttack = GetNextAttackDelay( 0.15 );
	}
	else
	{
		m_flNextPrimaryAttack = GetNextAttackDelay( 0.25 );
		m_flNextSecondaryAttack = GetNextAttackDelay( 0.25 );
	}

	attack_state = 0;
	bDidStartAnim = false;
	time_counter = 0;
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1;
#ifdef FISTS_DBG_MSG
	ALERT( at_console, "DID ATTACK!\n" );
#endif
#endif
}


#if 0
void CPipewrench::Smack()
{
	//DecalGunshot( &m_trHit, BULLET_PLAYER_CROWBAR );
}

void CPipewrench::SwingAgain()
{
	Swing( false );
}

bool CPipewrench::Swing( const bool bFirst )
{
	bool bDidHit = false;

	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	if( bFirst )
	{
		PLAYBACK_EVENT_FULL(UTIL_DefaultPlaybackFlags(), m_pPlayer->edict(), m_usPipewrench,
							 0.0, g_vecZero, g_vecZero, 0, 0, 0,
							 0.0, 0, tr.flFraction < 1 );
	}


	if ( tr.flFraction >= 1.0 )
	{
		if( bFirst )
		{
			// miss
			m_flNextPrimaryAttack = GetNextAttackDelay(0.75);
			m_flNextSecondaryAttack = GetNextAttackDelay(0.75);
			m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;

			// Shepard - In Opposing Force, the "miss" sound is
			// played twice (maybe it's a mistake from Gearbox or
			// an intended feature), if you only want a single
			// sound, comment this "switch" or the one in the
			// event (EV_Pipewrench)
			/*
			switch ( ((m_iSwing++) % 1) )
			{
			case 0: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_miss1.wav", 1, ATTN_NORM); break;
			case 1: EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_miss2.wav", 1, ATTN_NORM); break;
			}
			*/

			// player "shoot" animation
			m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		}
	}
	else
	{
		switch( ((m_iSwing++) % 2) + 1 )
		{
		case 0:
			SendWeaponAnim( PIPEWRENCH_ATTACK1HIT ); break;
		case 1:
			SendWeaponAnim( PIPEWRENCH_ATTACK2HIT ); break;
		case 2:
			SendWeaponAnim( PIPEWRENCH_ATTACK3HIT ); break;
		}

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		bDidHit = true;
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if( pEntity )
		{
			ClearMultiDamage();

			if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
			{
				// first swing does full damage
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.punchDmg, gpGlobals->v_forward, &tr, DMG_CLUB );
			}
			else
			{
				// subsequent swings do half
				pEntity->TraceAttack( m_pPlayer->pev, gSkillData.punchDmg * 0.75f, gpGlobals->v_forward, &tr, DMG_CLUB );
			}	

			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
		}

#endif

		m_flNextPrimaryAttack = GetNextAttackDelay(0.5);
		m_flNextSecondaryAttack = GetNextAttackDelay(0.5);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;

#ifndef CLIENT_DLL

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		bool bHitWorld = true;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod1.wav", 1, ATTN_NORM); break;
				case 1:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod2.wav", 1, ATTN_NORM); break;
				case 2:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hitbod3.wav", 1, ATTN_NORM); break;
				}
				m_pPlayer->m_iWeaponVolume = PIPEWRENCH_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return true;
				else
					  flVol = 0.1;

				bHitWorld = false;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if( bHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR );

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play pipe wrench strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * PIPEWRENCH_WALLHIT_VOLUME;

		SetThink( &CPipewrench::Smack );
		pev->nextthink = gpGlobals->time + 0.2;
#endif
		//RENDERERS START
		DecalGunshot(&m_trHit, BULLET_PLAYER_CROWBAR, vecSrc, vecEnd);
		//RENDERERS END
	}
	return bDidHit;
}

void CPipewrench::BigSwing()
{
	TraceResult tr;

	UTIL_MakeVectors( m_pPlayer->pev->v_angle );
	Vector vecSrc	= m_pPlayer->GetGunPosition( );
	Vector vecEnd	= vecSrc + gpGlobals->v_forward * 32;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, ENT( m_pPlayer->pev ), &tr );

#ifndef CLIENT_DLL
	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceHull( vecSrc, vecEnd, dont_ignore_monsters, head_hull, ENT( m_pPlayer->pev ), &tr );
		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			CBaseEntity *pHit = CBaseEntity::Instance( tr.pHit );
			if ( !pHit || pHit->IsBSPModel() )
				FindHullIntersection( vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX, m_pPlayer->edict() );
			vecEnd = tr.vecEndPos;	// This is the point on the actual surface (the hull could have hit space)
		}
	}
#endif

	PLAYBACK_EVENT_FULL(UTIL_DefaultPlaybackFlags(), m_pPlayer->edict(), m_usPipewrench,
		0.0, g_vecZero, g_vecZero, 0, 0, 0,
		0.0, 1, tr.flFraction < 1 );

	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, "weapons/melee_fist.wav", VOL_NORM, ATTN_NORM, 0, 94 + RANDOM_LONG(0, 15));

	if ( tr.flFraction >= 1.0 )
	{
		// miss
		m_flNextPrimaryAttack = GetNextAttackDelay(1.0);
		m_flNextSecondaryAttack = GetNextAttackDelay(1.0);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;

		SendWeaponAnim(PIPEWRENCH_BIG_SWING_MISS);

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
	}
	else
	{
		SendWeaponAnim( PIPEWRENCH_BIG_SWING_HIT );

		// player "shoot" animation
		m_pPlayer->SetAnimation( PLAYER_ATTACK1 );
		
#ifndef CLIENT_DLL

		// hit
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if( pEntity )
		{
			ClearMultiDamage();

			float flDamage = (gpGlobals->time - m_flBigSwingStart) * gSkillData.punchDmg + 25.0f;
			if ( (m_flNextPrimaryAttack + 1 < UTIL_WeaponTimeBase() ) || g_pGameRules->IsMultiplayer() )
			{
				// first swing does full damage
				pEntity->TraceAttack( m_pPlayer->pev, flDamage, gpGlobals->v_forward, &tr, DMG_CLUB );
			}
			else
			{
				// subsequent swings do half
				pEntity->TraceAttack( m_pPlayer->pev, flDamage / 2, gpGlobals->v_forward, &tr, DMG_CLUB );
			}	
			ApplyMultiDamage( m_pPlayer->pev, m_pPlayer->pev );
		}

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		bool bHitWorld = true;

		if (pEntity)
		{
			if ( pEntity->Classify() != CLASS_NONE && pEntity->Classify() != CLASS_MACHINE )
			{
				// play thwack or smack sound
				switch( RANDOM_LONG(0,1) )
				{
				case 0:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_big_hitbod1.wav", 1, ATTN_NORM);
					break;
				case 1:
					EMIT_SOUND( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_big_hitbod2.wav", 1, ATTN_NORM);
					break;
				}
				m_pPlayer->m_iWeaponVolume = PIPEWRENCH_BODYHIT_VOLUME;
				if ( !pEntity->IsAlive() )
					  return;
				else
					  flVol = 0.1;

				bHitWorld = false;
			}
		}

		// play texture hit sound
		// UNDONE: Calculate the correct point of intersection when we hit with the hull instead of the line

		if( bHitWorld )
		{
			float fvolbar = TEXTURETYPE_PlaySound(&tr, vecSrc, vecSrc + (vecEnd-vecSrc)*2, BULLET_PLAYER_CROWBAR );

			if ( g_pGameRules->IsMultiplayer() )
			{
				// override the volume here, cause we don't play texture sounds in multiplayer, 
				// and fvolbar is going to be 0 from the above call.

				fvolbar = 1;
			}

			// also play pipe wrench strike
			// Shepard - The commented sounds below are unused
			// in Opposing Force, if you wish to use them,
			// uncomment all the appropriate lines.
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				//EMIT_SOUND_DYN( m_pPlayer, CHAN_ITEM, "weapons/pwrench_big_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			case 1:
				EMIT_SOUND_DYN( m_pPlayer->edict(), CHAN_ITEM, "weapons/pwrench_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3));
				//EMIT_SOUND_DYN( m_pPlayer, CHAN_ITEM, "weapons/pwrench_big_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG(0,3)); 
				break;
			}

			// delay the decal a bit
			m_trHit = tr;
		}

		m_pPlayer->m_iWeaponVolume = flVol * PIPEWRENCH_WALLHIT_VOLUME;

		// Shepard - The original Opposing Force's pipe wrench
		// doesn't make a bullet hole decal when making a big
		// swing. If you want that decal, just uncomment the
		// 2 lines below.
		/*SetThink( &CPipewrench::Smack );
		SetNextThink( UTIL_WeaponTimeBase() + 0.2 );*/
#endif
		m_flNextPrimaryAttack = GetNextAttackDelay(1.0);
		m_flNextSecondaryAttack = GetNextAttackDelay(1.0);
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + 1.0;
	}
}
#endif

void CPipewrench::GetWeaponData( weapon_data_t& data )
{
	BaseClass::GetWeaponData( data );

	data.m_fInSpecialReload = static_cast<int>( m_iSwingMode );
}

void CPipewrench::SetWeaponData( const weapon_data_t& data )
{
	BaseClass::SetWeaponData( data );

	m_iSwingMode = data.m_fInSpecialReload;
}

int CPipewrench::iItemSlot()
{
	return 1;
}

int CPipewrench::GetItemInfo( ItemInfo* p )
{
	p->pszAmmo1 = nullptr;
	p->iMaxAmmo1 = WEAPON_NOCLIP;
	p->pszName = STRING( pev->classname );
	p->pszAmmo2 = nullptr;
	p->iMaxAmmo2 = WEAPON_NOCLIP;
	p->iMaxClip = WEAPON_NOCLIP;
	p->iSlot = 0;
	p->iPosition = 1;
	p->iId = m_iId = WEAPON_FISTS;
	p->iWeight = PIPEWRENCH_WEIGHT;

	return true;
}
