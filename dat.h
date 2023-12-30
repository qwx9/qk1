enum{
	Npath = 128,
	Nfspath = 128,
	Nsav = 12,
	Nsavcm = 40,
	Nsavver = 5,
	Nparms = 16,

	Te9 = 1000000000,

	Udpport = 26000,
};

typedef struct Sfx Sfx;
typedef struct wavinfo_t wavinfo_t;

struct Sfx {
	char s[Npath];
	int map;
	mem_user_t cu;
};

struct wavinfo_t
{
	int		rate;
	int		width;
	int		channels;
	int		loopofs;
	int		samples;
	int		dataofs;
};

extern char *game;

extern char fsdir[];
extern u16int crcn;
extern char *fs_lmpfrom;

extern char savs[][Nsavcm];
extern int savcanld[];

extern int dedicated;

enum{
	Fpsmin = 5,
	Fpsmax = 72
};

enum{
	Ambwater,
	Ambsky,
	Ambslime,
	Amblava,
	Namb
};

#define Spktvol 255.0
#define Spktatt 1.0

extern cvar_t bgmvolume;
extern int cdtrk, cdntrk;
extern bool cdloop, cdenabled;

extern int debug;
