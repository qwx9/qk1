#define	QUAKE_GAME			// as opposed to utilities
#define	VERSION				1.09
//#define	PARANOID			// speed sapping error checking
#define	GAMENAME	"id1"		// directory to look in by default

#define	VID_LockBuffer()
#define	VID_UnlockBuffer()

#ifdef __id386__
#define UNALIGNED_OK	// unset if unaligned accesses are not supported
#endif

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define CACHE_SIZE	32		// used to align key data structures

#define MAX_NUM_ARGVS	50

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2

#define	ON_EPSILON		0.1			// point on plane side epsilon

#define	MAX_DATAGRAM	1400		// max length of unreliable message
#define	MAX_DATAGRAM_LOCAL	65000	// on loopback we don't care

//
// per-level limits
//
#define	MAX_EDICTS		65536
#define	Nlights	64
#define	MAX_MODELS		4096			// these are sent over the net as bytes
#define	MAX_SOUNDS		2048			// so they cannot be blindly increased

#define	MAX_STYLESTRING	64

//
// stats are integers communicated to the client by the server
//
#define	MAX_CL_STATS		256
#define	STAT_HEALTH			0
#define	STAT_FRAGS			1
#define	STAT_WEAPON			2
#define	STAT_AMMO			3
#define	STAT_ARMOR			4
#define	STAT_WEAPONFRAME	5
#define	STAT_SHELLS			6
#define	STAT_NAILS			7
#define	STAT_ROCKETS		8
#define	STAT_CELLS			9
#define	STAT_ACTIVEWEAPON	10
#define	STAT_TOTALSECRETS	11
#define	STAT_TOTALMONSTERS	12
#define	STAT_SECRETS		13		// bumped on client side by svc_foundsecret
#define	STAT_MONSTERS		14		// bumped by svc_killedmonster

// stock defines

#define	IT_SHOTGUN				1
#define	IT_SUPER_SHOTGUN		2
#define	IT_NAILGUN				4
#define	IT_SUPER_NAILGUN		8
#define	IT_GRENADE_LAUNCHER		16
#define	IT_ROCKET_LAUNCHER		32
#define	IT_LIGHTNING			64
#define IT_SUPER_LIGHTNING      128
#define IT_SHELLS               256
#define IT_NAILS                512
#define IT_ROCKETS              1024
#define IT_CELLS                2048
#define IT_AXE                  4096
#define IT_ARMOR1               8192
#define IT_ARMOR2               16384
#define IT_ARMOR3               32768
#define IT_SUPERHEALTH          65536
#define IT_KEY1                 131072
#define IT_KEY2                 262144
#define	IT_INVISIBILITY			524288
#define	IT_INVULNERABILITY		1048576
#define	IT_SUIT					2097152
#define	IT_QUAD					4194304
#define IT_SIGIL1               (1<<28)
#define IT_SIGIL2               (1<<29)
#define IT_SIGIL3               (1<<30)
#define IT_SIGIL4               (1<<31)

//===========================================
//rogue changed and added defines

#define RIT_SHELLS              128
#define RIT_NAILS               256
#define RIT_ROCKETS             512
#define RIT_CELLS               1024
#define RIT_AXE                 2048
#define RIT_LAVA_NAILGUN        4096
#define RIT_LAVA_SUPER_NAILGUN  8192
#define RIT_MULTI_GRENADE       16384
#define RIT_MULTI_ROCKET        32768
#define RIT_PLASMA_GUN          65536
#define RIT_ARMOR1              8388608
#define RIT_ARMOR2              16777216
#define RIT_ARMOR3              33554432
#define RIT_LAVA_NAILS          67108864
#define RIT_PLASMA_AMMO         134217728
#define RIT_MULTI_ROCKETS       268435456
#define RIT_SHIELD              536870912
#define RIT_ANTIGRAV            1073741824
#define RIT_SUPERHEALTH         2147483648

//MED 01/04/97 added hipnotic defines
//===========================================
//hipnotic added defines
#define HIT_PROXIMITY_GUN_BIT 16
#define HIT_MJOLNIR_BIT       7
#define HIT_LASER_CANNON_BIT  23
#define HIT_PROXIMITY_GUN   (1<<HIT_PROXIMITY_GUN_BIT)
#define HIT_MJOLNIR         (1<<HIT_MJOLNIR_BIT)
#define HIT_LASER_CANNON    (1<<HIT_LASER_CANNON_BIT)
#define HIT_WETSUIT         (1<<(23+2))
#define HIT_EMPATHY_SHIELDS (1<<(23+3))

//===========================================

#define	MAX_SCOREBOARD		16
#define	MAX_SCOREBOARDNAME	32

typedef unsigned char 		byte;
typedef enum {false, true}	qboolean;

#include "cvar.h"
#include "common.h"
#include "bspfile.h"
#include "vid.h"
#include "zone.h"
#include "mathlib.h"

typedef struct
{
	vec3_t	origin;
	vec3_t	angles;
	int		modelindex;
	int		frame;
	int		colormap;
	int		skin;
	int		effects;
} entity_state_t;

#include "wad.h"
#include "draw.h"
#include "screen.h"
#include "net.h"
#include "protocol.h"
#include "cmd.h"
#include "sbar.h"

struct Sfx{
	char s[Npath];
	mem_user_t cu;
};

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

/* included here to avoid type incompatibilities errors from 2c(1),2l(1) */
#include "r_local.h"
#include "d_local.h"

extern qboolean noclip_anglehack;


//
// host
//
extern	cvar_t		sys_ticrate;

extern	qboolean	host_initialized;		// true if into command execution
extern	double		host_frametime;
extern	byte		*host_basepal;
extern	byte		*host_colormap;
extern	int		host_framecount;	// incremented every frame, never reset
extern	double		realtime;		// not bounded in any way, changed at
						// start of every frame, never reset

void Host_ClearMemory (void);
void Host_ServerFrame (void);
void Host_InitCommands (void);
void Host_Init (void);
void Host_Shutdown(void);
void Host_Error (char *error, ...);
void Host_EndGame (char *message, ...);
void Host_Frame (float time);
void Host_Quit_f (void);
void Host_ClientCommands (char *fmt, ...);
void Host_ShutdownServer (qboolean crash);

extern cvar_t	pausable;
extern qboolean		msg_suppress_1;		// suppresses resolution and cache size console output
										//  an fullscreen DIB focus gain/loss
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
