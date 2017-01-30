#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <bio.h>
#include "quakedef.h"

u16int crcn;
char fsdir[Nfspath];

typedef struct Lump Lump;
typedef struct Pak Pak;
typedef struct Paklist Paklist;

enum{
	Npakhdr = 4+4+4,
	Npaksz = 56+4+4,
	Npaklmp = 2048,
	Npak0lmp = 339,
	Npak0crc = 0x80d5,
	Fhunk = 0,
	Fcache,
	Fstack
};
struct Lump{
	char f[Npath];
	int ofs;
	int len;
};
struct Pak{
	char f[Nfspath];
	Biobuf *bf;
	Lump *l;
	Lump *e;
};
struct Paklist{
	char f[Nfspath];
	Pak *p;
	Paklist *pl;
};
static Paklist *pkl;

static u16int pop[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x6600, 0x0000, 0x0000, 0x0000, 0x6600, 0x0000,
	0x0000, 0x0066, 0x0000, 0x0000, 0x0000, 0x0000, 0x0067, 0x0000,
	0x0000, 0x6665, 0x0000, 0x0000, 0x0000, 0x0000, 0x0065, 0x6600,
	0x0063, 0x6561, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6563,
	0x0064, 0x6561, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6564,
	0x0064, 0x6564, 0x0000, 0x6469, 0x6969, 0x6400, 0x0064, 0x6564,
	0x0063, 0x6568, 0x6200, 0x0064, 0x6864, 0x0000, 0x6268, 0x6563,
	0x0000, 0x6567, 0x6963, 0x0064, 0x6764, 0x0063, 0x6967, 0x6500,
	0x0000, 0x6266, 0x6769, 0x6a68, 0x6768, 0x6a69, 0x6766, 0x6200,
	0x0000, 0x0062, 0x6566, 0x6666, 0x6666, 0x6666, 0x6562, 0x0000,
	0x0000, 0x0000, 0x0062, 0x6364, 0x6664, 0x6362, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0062, 0x6662, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0061, 0x6661, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x6500, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x6400, 0x0000, 0x0000, 0x0000
};

/* this is a 16 bit, non-reflected CRC using the polynomial 0x1021 and the
 * initial and final xor values shown below; in other words, the CCITT standard
 * CRC used by XMODEM */
enum{
	Ncrc0 = 0xffff,
	Nxor = 0
};
static u16int crct[] ={
	0x0000,	0x1021,	0x2042,	0x3063,	0x4084,	0x50a5,	0x60c6,	0x70e7,
	0x8108,	0x9129,	0xa14a,	0xb16b,	0xc18c,	0xd1ad,	0xe1ce,	0xf1ef,
	0x1231,	0x0210,	0x3273,	0x2252,	0x52b5,	0x4294,	0x72f7,	0x62d6,
	0x9339,	0x8318,	0xb37b,	0xa35a,	0xd3bd,	0xc39c,	0xf3ff,	0xe3de,
	0x2462,	0x3443,	0x0420,	0x1401,	0x64e6,	0x74c7,	0x44a4,	0x5485,
	0xa56a,	0xb54b,	0x8528,	0x9509,	0xe5ee,	0xf5cf,	0xc5ac,	0xd58d,
	0x3653,	0x2672,	0x1611,	0x0630,	0x76d7,	0x66f6,	0x5695,	0x46b4,
	0xb75b,	0xa77a,	0x9719,	0x8738,	0xf7df,	0xe7fe,	0xd79d,	0xc7bc,
	0x48c4,	0x58e5,	0x6886,	0x78a7,	0x0840,	0x1861,	0x2802,	0x3823,
	0xc9cc,	0xd9ed,	0xe98e,	0xf9af,	0x8948,	0x9969,	0xa90a,	0xb92b,
	0x5af5,	0x4ad4,	0x7ab7,	0x6a96,	0x1a71,	0x0a50,	0x3a33,	0x2a12,
	0xdbfd,	0xcbdc,	0xfbbf,	0xeb9e,	0x9b79,	0x8b58,	0xbb3b,	0xab1a,
	0x6ca6,	0x7c87,	0x4ce4,	0x5cc5,	0x2c22,	0x3c03,	0x0c60,	0x1c41,
	0xedae,	0xfd8f,	0xcdec,	0xddcd,	0xad2a,	0xbd0b,	0x8d68,	0x9d49,
	0x7e97,	0x6eb6,	0x5ed5,	0x4ef4,	0x3e13,	0x2e32,	0x1e51,	0x0e70,
	0xff9f,	0xefbe,	0xdfdd,	0xcffc,	0xbf1b,	0xaf3a,	0x9f59,	0x8f78,
	0x9188,	0x81a9,	0xb1ca,	0xa1eb,	0xd10c,	0xc12d,	0xf14e,	0xe16f,
	0x1080,	0x00a1,	0x30c2,	0x20e3,	0x5004,	0x4025,	0x7046,	0x6067,
	0x83b9,	0x9398,	0xa3fb,	0xb3da,	0xc33d,	0xd31c,	0xe37f,	0xf35e,
	0x02b1,	0x1290,	0x22f3,	0x32d2,	0x4235,	0x5214,	0x6277,	0x7256,
	0xb5ea,	0xa5cb,	0x95a8,	0x8589,	0xf56e,	0xe54f,	0xd52c,	0xc50d,
	0x34e2,	0x24c3,	0x14a0,	0x0481,	0x7466,	0x6447,	0x5424,	0x4405,
	0xa7db,	0xb7fa,	0x8799,	0x97b8,	0xe75f,	0xf77e,	0xc71d,	0xd73c,
	0x26d3,	0x36f2,	0x0691,	0x16b0,	0x6657,	0x7676,	0x4615,	0x5634,
	0xd94c,	0xc96d,	0xf90e,	0xe92f,	0x99c8,	0x89e9,	0xb98a,	0xa9ab,
	0x5844,	0x4865,	0x7806,	0x6827,	0x18c0,	0x08e1,	0x3882,	0x28a3,
	0xcb7d,	0xdb5c,	0xeb3f,	0xfb1e,	0x8bf9,	0x9bd8,	0xabbb,	0xbb9a,
	0x4a75,	0x5a54,	0x6a37,	0x7a16,	0x0af1,	0x1ad0,	0x2ab3,	0x3a92,
	0xfd2e,	0xed0f,	0xdd6c,	0xcd4d,	0xbdaa,	0xad8b,	0x9de8,	0x8dc9,
	0x7c26,	0x6c07,	0x5c64,	0x4c45,	0x3ca2,	0x2c83,	0x1ce0,	0x0cc1,
	0xef1f,	0xff3e,	0xcf5d,	0xdf7c,	0xaf9b,	0xbfba,	0x8fd9,	0x9ff8,
	0x6e17,	0x7e36,	0x4e55,	0x5e74,	0x2e93,	0x3eb2,	0x0ed1,	0x1ef0
};
static int notid1;

static int loadsize;
static uchar *loadbuf;
static cache_user_t *loadcache;

static Biobuf *demobf;
static vlong demoofs;

#define	GBIT32(p)	((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))
#define	PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24

static Biobuf *
bopen(char *f, int m, int arm)
{
	Biobuf *bf;

	bf = Bopen(f, m);
	if(bf == nil)
		return nil;
	if(arm)
		Blethal(bf, nil);
	return bf;
}

static long
eread(Biobuf *bf, void *u, long n)
{
	if(Bread(bf, u, n) != n)
		fatal("eread: short read: %r");
	return n;
}

static u8int
get8(Biobuf *bf)
{
	u8int v;

	eread(bf, &v, 1);
	return v;
}

static u16int
get16(Biobuf *bf)
{
	u16int v;

	v = get8(bf);
	return v | get8(bf) << 8;
}

static u32int
get32(Biobuf *bf)
{
	u32int v;

	v = get16(bf);
	return v | get16(bf) << 16;
}

static float
getfl(Biobuf *bf)
{
	union{
		float v;
		u32int u;
	} u;

	u.u = get32(bf);
	return u.v;
}

static u16int
get16b(Biobuf *bf)
{
	u16int v;

	v = get8(bf);
	return v << 8 | get8(bf);
}

static u32int
get32b(Biobuf *bf)
{
	u32int v;

	v = get16(bf);
	return v << 16 | get16(bf);
}

static void
ewrite(Biobuf *bf, void *u, long n)
{
	if(Bwrite(bf, u, n) != n)
		fatal("eread: short write: %r");
}

static void
put32(Biobuf *bf, u32int v)
{
	uchar u[4];

	PBIT32(u, v);
	ewrite(bf, u, 4);
}

static void
putfl(Biobuf *bf, float v)
{
	union{
		float v;
		u32int u;
	} w;

	w.v = v;
	put32(bf, w.u);
}

static vlong
bsize(Biobuf *bf)
{
	vlong n;
	Dir *d;

	d = dirfstat(Bfildes(bf));
	if(d == nil)
		sysfatal("bstat: %r");
	n = d->length;
	free(d);
	return n;
}

static Pak *
pak(char *f)
{
	int n, ofs, len, nlmp;
	uchar u[8];
	Biobuf *bf;
	Lump *l;
	Pak *p;

	bf = bopen(f, OREAD, 1);
	if(bf == nil)
		return nil;
	memset(u, 0, sizeof u);
	eread(bf, u, 4);
	if(memcmp(u, "PACK", 4) != 0)
		fatal("pak %s: invalid pak file", f);
	ofs = get32(bf);
	len = get32(bf);
	nlmp = len / Npaksz;
	if(nlmp > Npaklmp)
		fatal("pak %s: invalid lump number %d", f, nlmp);
	if(nlmp != Npak0lmp)
		notid1 = 1;
	l = Hunk_AllocName(nlmp * sizeof *l, "pak");
	p = Hunk_Alloc(sizeof *p);
	strncpy(p->f, f, sizeof(p->f)-1);
	p->bf = bf;
	p->l = l;
	p->e = l + nlmp;
	Bseek(bf, ofs, 0);
	initcrc();
	while(l < p->e){
		eread(bf, l->f, 56);
		for(n=0; n<56; n++)
			crc(l->f[n]);
		eread(bf, u, 8);
		for(n=0; n<8; n++)
			crc(u[n]);
		l->ofs = GBIT32(u);
		l->len = GBIT32(u+4);
		l++;
	}
	if(crcn != Npak0crc)
		notid1 = 1;
	return p;
}

static void
pakdir(char *d)
{
	int n;
	char f[Nfspath];
	Paklist *pl;
	Pak *p;

	strncpy(fsdir, d, sizeof(fsdir)-1);
	pl = Hunk_Alloc(sizeof *pl);
	strncpy(pl->f, d, sizeof(pl->f)-1);
	pl->pl = pkl;
	pkl = pl;
	for(n=0; ; n++){
		snprint(f, sizeof f, "%s/pak%d.pak", d, n);
		p = pak(f);
		if(p == nil){
			dprint("pakdir: can't open %s: %r", f);
			break;
		}
		pl = Hunk_Alloc(sizeof *pl);
		pl->p = p;
		pl->pl = pkl;
		pkl = pl;       
	}
}

static void
initns(void)
{
	int i;
	char d[Nfspath], *e;

	memset(d, 0, sizeof d);
	i = COM_CheckParm("-basedir");
	if(i != 0 && i < com_argc - 1)
		strncpy(d, com_argv[i+1], sizeof(d)-1);
	else
		strncpy(d, host_parms.basedir, sizeof(d)-1);
	e = d + strlen(d) - 1;
	if(e > d && *e == '/')
		*e = 0;
	pakdir(va("%s%s", d, "/id1"));
	if(COM_CheckParm("-rogue"))
		pakdir(va("%s%s", d, "/rogue"));
	if(COM_CheckParm("-hipnotic"))
		pakdir(va("%s%s", d, "/hipnotic"));
	i = COM_CheckParm("-game");
	if(i != 0 && i < com_argc - 1){
		notid1 = 1;
		pakdir(va("%s/%s", d, com_argv[i+1]));
	}
}

static Biobuf *
openlmp(char *f, int *len)
{
	char d[Nfspath];
	Biobuf *bf;
	Paklist *pl;
	Pak *p;
	Lump *l;

	for(pl=pkl; pl != nil; pl=pl->pl){
		if(pl->p != nil){
			p = pl->p;
			l = p->l;
			while(l < p->e){
				if(strcmp(l->f, f) == 0){
					Bseek(p->bf, l->ofs, 0);
					if(len != nil)
						*len = l->len;
					return p->bf;
				}
				l++;
			}
			continue;
		}
		snprint(d, sizeof d, "%s/%s", pl->f, f);
		if(access(d, AREAD) < 0)
			continue;
		bf = bopen(d, OREAD, 1);
		if(bf == nil)
			return nil;
		if(len != nil)
			*len = bsize(bf);
		return bf;
	}
	werrstr("openlmp %s: not found", f);
	return nil;
}

static void
closelmp(Biobuf *bf)
{
	Paklist *pl;

	for(pl=pkl; pl!=nil; pl=pl->pl)
		if(pl->p && pl->p->bf == bf)
			return;
	Bterm(bf);
}

static uchar *
loadlmp(char *f, int mth, int *n)
{
	int m;
	char r[32];
	uchar *buf;
	Biobuf *bf;

	bf = openlmp(f, &m);
	if(bf == nil)
		return nil;
	radix(f, r);
	buf = nil;
	switch(mth){
	case Fhunk: buf = Hunk_AllocName(m + 1, r); break;
	case Fcache: buf = Cache_Alloc(loadcache, m + 1, r); break;
	case Fstack: buf = m+1 <= loadsize ? loadbuf : Hunk_TempAlloc(m+1);
	}
	if(buf == nil)
		fatal("loadlmp %s %d: memory allocation failed: %r", f, m+1);
	buf[m] = 0;
	Draw_BeginDisc();
	eread(bf, buf, m);
	closelmp(bf);
	Draw_EndDisc();
	if(n != nil)
		*n = m;
	return buf;
}

static void
path(void)
{
	Paklist *pl;

	for(pl=pkl; pl!=nil; pl=pl->pl)
		if(pl->p)
			Con_Printf("%s (%zd files)\n", pl->p->f, pl->p->e - pl->p->l);
		else
			Con_Printf("%s\n", pl->f);
}

static void
chkreg(void)
{
	u16int *p;
	Biobuf *bf;

	Cvar_RegisterVariable(&registered);
	bf = openlmp("gfx/pop.lmp", nil);
	if(bf == nil){
		dprint("chkreg: shareware version");
		if(notid1)
			fatal("chkreg: phase error -- %r");
		return;
	}
	p = pop;
	while(p < pop + nelem(pop))
		if(*p++ != get16b(bf))
			fatal("chkreg: corrupted pop lump");
	closelmp(bf);
	setcvar("registered", "1");
	dprint("chkreg: registered version");
}

void *
loadhunklmp(char *f, int *n)
{
	return loadlmp(f, Fhunk, n);
}

void *
loadcachelmp(char *f, cache_user_t *c)
{
	loadcache = c;
	loadlmp(f, Fcache, nil);
	return c->data;
}

void *
loadstklmp(char *f, void *buf, int nbuf, int *n)
{
	loadbuf = buf;
	loadsize = nbuf;
	return loadlmp(f, Fstack, n);
}

void
pointlmp(void)
{
	int i, n, nv;
	char f[Nfspath];
	Biobuf *bf;
	vec3_t v3;
	vec_t *v;
	particle_t *p;

	snprint(f, sizeof f, "maps/%s.pts", sv.name);
	bf = openlmp(f, &n);
	if(bf == nil){
		Con_Printf("pointlmp: can't open %s: %r\n", f);
		return;
	}
	nv = 0;
	for(;;){
		if(n < 3*4+3)
			break;
		for(i=0, v=v3; i<3; i++){
			*v++ = getfl(bf);
			Bseek(bf, 1, 1);
		}
		n -= 3*4+3;
		nv++;
		if(free_particles == nil){
			Con_Printf("pointlmp: insufficient free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		p->die = 99999;
		p->color = -nv & 15;
		p->type = pt_static;
		VectorCopy(vec3_origin, p->vel);
		VectorCopy(v3, p->org);
	}
	closelmp(bf);
	Con_Printf("pointlmp: %d points read\n", nv);
}

void
endlmp(void)
{
	closelmp(demobf);
	demobf = nil;
}

int
rlmpmsg(void)
{
	int n;
	vec_t *f;

	Bseek(demobf, demoofs, 0);
	net_message.cursize = get32(demobf);
	VectorCopy(cl.mviewangles[0], cl.mviewangles[1]);
	for(n=0, f=cl.mviewangles[0]; n<3; n++)
		*f++ = getfl(demobf);
	if(net_message.cursize > Nmsg)
		fatal("rlmpmsg: invalid message size %d\n", net_message.cursize);
	n = Bread(demobf, net_message.data, net_message.cursize);
	demoofs = Bseek(demobf, 0, 1);
	if(n < 0)
		dprint("rlmpmsg: bad read: %r");
	if(n != net_message.cursize){
		dprint("rlmpmsg: short read: %r");
		n = -1;
	}
	return n;
}

void
wlmpmsg(void)
{
	int i;

	put32(demobf, net_message.cursize);
	for(i=0; i<3; i++)
		putfl(demobf, cl.viewangles[i]);
	ewrite(demobf, net_message.data, net_message.cursize);
}

int
reclmp(char *f, int trk)
{
	char s[16];

	demobf = bopen(f, OWRITE, 0);
	if(demobf == nil)
		return -1;
	sprint(s, "%d\n", trk);
	ewrite(demobf, s, strlen(s));
	return 0;
}

demolmp(char *f)
{
	int n;
	char *s;

	demobf = openlmp(f, &n);
	if(demobf == nil)
		return -1;
	s = Brdline(demobf, '\n');
	n = Blinelen(demobf) - 1;
	if(s == nil || n < 0 || n > 11){
		dprint("demolmp: invalid trk field");
		closelmp(demobf);
		return -1;
	}
	demoofs = Bseek(demobf, 0, 1);
	s[n] = 0;
	cls.forcetrack =  strtol(s, nil, 10);
	return 0;
}

void
crc(u8int v)
{
	crcn = crcn << 8 ^ crct[crcn >> 8 ^ v];
}

void
initcrc(void)
{
	crcn = Ncrc0;
}

void
ext(char *f, char *e)
{
	char *s, *d;

	s = strrchr(f, '/');
	d = strrchr(f, '.');
	if(d > s)
		return;
	strcat(f, e);
}

void
radix(char *f, char *d)
{
	char *s, *e;

	s = strrchr(f, '/');
	e = strrchr(f, '.');
	if(s == nil)
		s = f;
	s++;
	if(e - s < 1)
		strcpy(d, "?model?");
	else{
		strncpy(d, s, e - s);
		d[e - s] = 0;
	}
}

void
unloadfs(void)
{
	Paklist *pl;

	for(pl=pkl; pl!=nil; pl=pl->pl)
		if(pl->p != nil)
			Bterm(pl->p->bf);
}

/* TODO: nuke these from orbit */
static short
ShortSwap(short l)
{
	byte    b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;
	return (b1<<8) + b2;
}
static short
ShortNoSwap(short l)
{
	return l;
}
static int
LongSwap(int l)
{
	byte    b1,b2,b3,b4;

	b1 = l&255;
	b2 = (l>>8)&255;
	b3 = (l>>16)&255;
	b4 = (l>>24)&255;
	return ((int)b1<<24) + ((int)b2<<16) + ((int)b3<<8) + b4;
}
static int
LongNoSwap(int l)
{
	return l;
}
static float
FloatSwap(float f)
{
	union{
		float   f;
		byte    b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}
static float
FloatNoSwap(float f)
{
	return f;
}

void
initfs(void)
{
	byte swaptest[2] = {1,0};

	if(*(short *)swaptest == 1)
	{
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}else{
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		BigFloat = FloatNoSwap;
		LittleFloat = FloatSwap;
	}
	initns();
	chkreg();
	Cmd_AddCommand("path", path);
	Cvar_RegisterVariable(&cmdline);
	setcvar("cmdline", com_cmdline);
}
