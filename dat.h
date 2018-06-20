typedef struct Sfx Sfx;

extern char *game;
extern uchar *membase;
extern int memsize;

enum{
	Npath = 64,
	Nfspath = 128,
	Nmsg = 8000,
	Nsav = 12,
	Nsavcm = 40,
	Nsavver = 5,
	Nparms = 16,

	Te9 = 1000000000
};

extern char fsdir[];
extern u16int crcn;

extern char savs[][Nsavcm];
extern int savcanld[];

extern int dedicated, listener;

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

extern int dumpwin;
