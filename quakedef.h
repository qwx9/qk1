#pragma once

#include <u.h>
#include <libc.h>
#include <stdio.h>

#ifdef __plan9__
typedef enum {false, true} bool;
#endif

#define	QUAKE_GAME			// as opposed to utilities
#define	VERSION				1.09
//#define	PARANOID			// speed sapping error checking
#define	GAMENAME	"id1"		// directory to look in by default

enum {
	MAX_DATAGRAM = 1400,		// max length of unreliable message
	MAX_DATAGRAM_LOCAL = 65000,	// on loopback we don't care

	PITCH = 0, // up / down
	YAW, // left / right
	ROLL, // fall over

	// limits
	MAX_EDICTS = 65536,
	MAX_MODELS = 4096,
	MAX_SOUNDS = 2048,
	MAX_STYLESTRING	= 64,
	MAX_SCOREBOARD = 16,
	MAX_SCOREBOARDNAME = 32,
	Nlights = 64,

	// stats are integers communicated to the client by the server
	MAX_CL_STATS = 256,

	STAT_HEALTH = 0,
	STAT_FRAGS,
	STAT_WEAPON,
	STAT_AMMO,
	STAT_ARMOR,
	STAT_WEAPONFRAME,
	STAT_SHELLS,
	STAT_NAILS,
	STAT_ROCKETS,
	STAT_CELLS,
	STAT_ACTIVEWEAPON,
	STAT_TOTALSECRETS,
	STAT_TOTALMONSTERS,
	STAT_SECRETS, // bumped on client side by svc_foundsecret
	STAT_MONSTERS, // bumped by svc_killedmonster

	// stock
	IT_SHOTGUN = 1<<0,
	IT_SUPER_SHOTGUN = 1<<1,
	IT_NAILGUN = 1<<2,
	IT_SUPER_NAILGUN = 1<<3,
	IT_GRENADE_LAUNCHER = 1<<4,
	IT_ROCKET_LAUNCHER = 1<<5,
	IT_LIGHTNING = 1<<6,
	IT_SUPER_LIGHTNING = 1<<7,
	IT_SHELLS = 1<<8,
	IT_NAILS = 1<<9,
	IT_ROCKETS = 1<<10,
	IT_CELLS = 1<<11,
	IT_AXE = 1<<12,
	IT_ARMOR1 = 1<<13,
	IT_ARMOR2 = 1<<14,
	IT_ARMOR3 = 1<<15,
	IT_SUPERHEALTH = 1<<16,
	IT_KEY1 = 1<<17,
	IT_KEY2 = 1<<18,
	IT_INVISIBILITY = 1<<19,
	IT_INVULNERABILITY = 1<<20,
	IT_SUIT = 1<<21,
	IT_QUAD = 1<<22,
	IT_SIGIL1 = 1<<28,
	IT_SIGIL2 = 1<<29,
	IT_SIGIL3 = 1<<30,
	IT_SIGIL4 = 1<<31,

	// rogue changed and added defines
	RIT_SHELLS = 1<<7,
	RIT_NAILS = 1<<8,
	RIT_ROCKETS = 1<<9,
	RIT_CELLS = 1<<10,
	RIT_AXE = 1<<11,
	RIT_LAVA_NAILGUN = 1<<12,
	RIT_LAVA_SUPER_NAILGUN = 1<<13,
	RIT_MULTI_GRENADE = 1<<14,
	RIT_MULTI_ROCKET = 1<<15,
	RIT_PLASMA_GUN = 1<<16,
	RIT_ARMOR1 = 1<<23,
	RIT_ARMOR2 = 1<<24,
	RIT_ARMOR3 = 1<<25,
	RIT_LAVA_NAILS = 1<<26,
	RIT_PLASMA_AMMO = 1<<27,
	RIT_MULTI_ROCKETS = 1<<28,
	RIT_SHIELD = 1<<29,
	RIT_ANTIGRAV = 1<<30,
	RIT_SUPERHEALTH = 1<<31,

	// hipnotic added defines,
	HIT_PROXIMITY_GUN_BIT = 16,
	HIT_MJOLNIR_BIT = 7,
	HIT_LASER_CANNON_BIT = 23,
	HIT_PROXIMITY_GUN = 1<<HIT_PROXIMITY_GUN_BIT,
	HIT_MJOLNIR = 1<<HIT_MJOLNIR_BIT,
	HIT_LASER_CANNON = 1<<HIT_LASER_CANNON_BIT,
	HIT_WETSUIT = 1<<(23+2),
	HIT_EMPATHY_SHIELDS = 1<<(23+3),
};

//===========================================

typedef u8int byte;

#include "cvar.h"
#include "common.h"
#include "zone.h"
#include "dat.h"
#include "mathlib.h"
#include "fns.h"
#include "bspfile.h"
#include "vid.h"

typedef struct
{
	vec3_t	origin;
	vec3_t	angles;
	int		modelindex;
	int		frame;
	int		colormap;
	int		skin;
	int		effects;
	byte	alpha;
} entity_state_t;

#include "wad.h"
#include "draw.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"

extern cvar_t bgmvolume;
extern cvar_t volume;

#include "render.h"
#include "client.h"
#include "progs.h"
#include "server.h"
#include "model.h"
#include "d_iface.h"
#include "input.h"
#include "world.h"
#include "keys.h"
#include "console.h"
#include "view.h"
#include "menu.h"
#include "r_local.h"
#include "d_local.h"

extern bool noclip_anglehack;


//
// host
//
extern	cvar_t		sys_ticrate;
extern	cvar_t		developer;

extern	bool	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;
extern	int		host_framecount;	// incremented every frame, never reset
extern	double		realtime;		// not bounded in any way, changed at
						// start of every frame, never reset

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (int argc, char **argv, char **paths);
void Host_Shutdown(void);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (bool crash);

extern cvar_t	pausable;
extern int			current_skill;		// skill level for currently loaded level (in case
										//  the user changes the cvar while the level is
										//  running, this reflects the level actually in use)

//
// chase
//
extern	cvar_t	chase_active;

void Chase_Init (void);
void Chase_Reset (void);
void Chase_Update (void);

#pragma varargck	argpos	Host_Error	1
#pragma varargck	argpos	Host_EndGame	1
#pragma varargck	argpos	Host_ClientCommands	1
