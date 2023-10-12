typedef struct Sfx Sfx;

extern char *game;

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

extern char fsdir[];
extern u16int crcn;

extern char savs[][Nsavcm];
extern int savcanld[];

extern int dedicated;

enum{
	Fpsmin = 10,
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
