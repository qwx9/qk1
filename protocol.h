// protocol.h -- communications protocols

enum {
	PROTO_NQ,
	PROTO_RMQ,
	PROTO_NUM,
};

// these are the only ones supported with RMQ atm
enum {
	PF_RMQ_ANGLE_INT16 = 1<<1,
	PF_RMQ_COORD_INT32 = 1<<7,
};

enum {
	DEFAULT_ALPHA,
	ZERO_ALPHA,
};

#define defalpha(a) ((a) == DEFAULT_ALPHA)
#define zeroalpha(a) (!defalpha(a) && (a) <= ZERO_ALPHA)
#define f2alpha(f) ((f) == 0.0 ? DEFAULT_ALPHA : clamp((f)*254+1, 1, 255))
#define alpha2f(a) (zeroalpha(a) ? 0.0 : (float)(a)/255.0)

typedef struct protocol_t protocol_t;

struct protocol_t {
	int version;
	int flags;
	char *name;

	// flexible limits - messages change
	u32int fl_large_entity;
	int large_entity;
	u32int fl_large_channel;
	int large_channel;
	u32int fl_large_sound;
	int large_sound;
	u32int fl_large_frame;
	int large_frame;
	u32int fl_large_model;
	int large_model;
	u32int fl_large_weaponframe;
	u32int fl_large_weaponmodel;
	u32int fl_large_baseline_model;
	u32int fl_large_baseline_frame;

	u32int fl_alpha;
	u32int fl_baseline_alpha;
	u32int fl_weapon_alpha;

	// absolute limits for the protocol
	int limit_entity;
	int limit_channel;
	int limit_model;
	int limit_frame;
	int limit_sound;

	void (*MSG_WriteCoord)(sizebuf_t *sb, float f);
	void (*MSG_WriteAngle)(sizebuf_t *sb, float f);
	float (*MSG_ReadCoord)(void);
	float (*MSG_ReadAngle)(void);

	void (*MSG_WriteProtocolInfo)(sizebuf_t *sb, protocol_t *proto);
	void (*MSG_ReadProtocolInfo)(protocol_t *proto);
};

extern protocol_t protos[PROTO_NUM];

// defaults for clientinfo messages
#define	DEFAULT_VIEWHEIGHT	22

// if the high bit of the servercmd is set, the low bits are fast update flags:
enum {
	// game types sent by serverinfo
	// these determine which intermission screen plays
	GAME_COOP = 0,
	GAME_DEATHMATCH,

	U_MOREBITS = 1<<0,
	U_ORIGIN1 = 1<<1,
	U_ORIGIN2 = 1<<2,
	U_ORIGIN3 = 1<<3,
	U_ANGLE2 = 1<<4,
	U_NOLERP = 1<<5,		// don't interpolate movement
	U_FRAME = 1<<6,
	U_SIGNAL = 1<<7,		// just differentiates from other updates
	// svc_update can pass all of the fast update bits, plus more
	U_ANGLE1 = 1<<8,
	U_ANGLE3 = 1<<9,
	U_MODEL = 1<<10,
	U_COLORMAP = 1<<11,
	U_SKIN = 1<<12,
	U_EFFECTS = 1<<13,
	U_LONGENTITY = 1<<14,
	U_MOREBITS2 = 1<<15,
	// some of the bits here (1<<16…1<<22) are reserved by specific protocols
	U_MOREBITS3 = 1<<23,

	SU_VIEWHEIGHT = 1<<0,
	SU_IDEALPITCH = 1<<1,
	SU_PUNCH1 = 1<<2,
	SU_PUNCH2 = 1<<3,
	SU_PUNCH3 = 1<<4,
	SU_VELOCITY1 = 1<<5,
	SU_VELOCITY2 = 1<<6,
	SU_VELOCITY3 = 1<<7,
	//SU_AIMENT = 1<<8,  AVAILABLE BIT
	SU_ITEMS = 1<<9,
	SU_ONGROUND = 1<<10,		// no data follows, the bit is it
	SU_INWATER = 1<<11,		// no data follows, the bit is it
	SU_WEAPONFRAME = 1<<12,
	SU_ARMOR = 1<<13,
	SU_WEAPON = 1<<14,
	SU_MOREBITS = 1<<15,
	// some of the bits here (1<<16…1<<22) are reserved by specific protocols
	SU_MOREBITS2 = 1<<23,

	// a sound with no channel is a local only sound
	SND_VOLUME = 1<<0,		// a byte
	SND_ATTENUATION = 1<<1,		// a byte
	SND_LOOPING = 1<<2,		// a long

	// server to client
	svc_bad = 0,
	svc_nop,
	svc_disconnect,
	svc_updatestat, // [byte] [long]
	svc_version, // [long] server version
	svc_setview, // [short] entity number
	svc_sound, // <see code>
	svc_time, // [float] server time
	svc_print, // [string] null terminated string
	svc_stufftext, // [string] stuffed into client's console buffer
								// the string should be \n terminated
	svc_setangle, // [angle3] set the view angle to this absolute value
	svc_serverinfo, // [long] version
						// [string] signon string
						// [string]..[0]model cache
						// [string]...[0]sounds cache
	svc_lightstyle, // [byte] [string]
	svc_updatename, // [byte] [string]
	svc_updatefrags, // [byte] [short]
	svc_clientdata, // <shortbits + data>
	svc_stopsound, // <see code>
	svc_updatecolors, // [byte] [byte]
	svc_particle, // [vec3] <variable>
	svc_damage,
	svc_spawnstatic,
	svc_spawnbinary, // unused
	svc_spawnbaseline,
	svc_temp_entity,
	svc_setpause, // [byte] on / off
	svc_signonnum, // [byte]  used for the signon sequence
	svc_centerprint, // [string] to put in center of the screen
	svc_killedmonster,
	svc_foundsecret,
	svc_spawnstaticsound, // [coord3] [byte] samp [byte] vol [byte] aten
	svc_intermission, // [string] music
	svc_finale, // [string] music [string] text
	svc_cdtrack, // [byte] track [byte] looptrack
	svc_sellscreen,
	svc_cutscene,

	// non-NQ stuff
	svc_spawnbaseline2 = 42,
	svc_spawnstatic2,
	svc_spawnstaticsound2,

	// client to server
	clc_bad = 0,
	clc_nop,
	clc_disconnect,
	clc_move, // [usercmd_t]
	clc_stringcmd, // [string] message

	// temp entity events
	TE_SPIKE = 0,
	TE_SUPERSPIKE,
	TE_GUNSHOT,
	TE_EXPLOSION,
	TE_TAREXPLOSION,
	TE_LIGHTNING1,
	TE_LIGHTNING2,
	TE_WIZSPIKE,
	TE_KNIGHTSPIKE,
	TE_LIGHTNING3,
	TE_LAVASPLASH,
	TE_TELEPORT,
	TE_EXPLOSION2,
	TE_BEAM,
};
