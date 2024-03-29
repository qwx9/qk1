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

struct Sfx {
	char s[Npath];
	int map;
	mem_user_t cu;
};

extern char *game;

extern char fsdir[];
extern u16int crcn;

extern char savs[][Nsavcm];
extern int savcanld[];

extern int dedicated;

enum{
	Fpsmin = 5,
	Fpsmax = 72
};

enum{
	Ambwater = 0,
	Ambsky,
	Ambslime,
	Amblava,
	Namb
};

enum{
	Spktvol = 255
};
#define Spktatt 1.0

extern int debug;
