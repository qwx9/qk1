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
	char fname[Npath];
	int ofs;
	int len;
};
struct Pak{
	char fname[Nfspath];
	FILE *f;
	Lump *l;
	Lump *e;
};
struct Paklist{
	char fname[Nfspath];
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
static mem_user_t *loadcache;
static FILE *demof;
static vlong demoofs;

#define	GBIT32(p)	((p)[0]|((p)[1]<<8)|((p)[2]<<16)|((p)[3]<<24))
#define	PBIT32(p,v)	(p)[0]=(v);(p)[1]=(v)>>8;(p)[2]=(v)>>16;(p)[3]=(v)>>24

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

char *
ext(char *f, char *e)
{
	return strrchr(f, '.') > strrchr(f, '/') ? "" : e;
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

static void
path(void)
{
	Paklist *pl;

	for(pl=pkl; pl!=nil; pl=pl->pl)
		if(pl->p)
			Con_Printf(va("%s (%zd files)\n", pl->p->f, pl->p->e - pl->p->l));
		else
			Con_Printf("%s\n", pl->fname);
}

static long
eread(FILE *f, void *u, long n)
{
	if(fread(u, 1, n, f) != n)
		fatal("eread: short read");
	return n;
}

static u8int
get8(FILE *f)
{
	u8int v;

	eread(f, &v, 1);
	return v;
}

static u16int
get16(FILE *f)
{
	u16int v;

	v = get8(f);
	return v | get8(f) << 8;
}

static u32int
get32(FILE *f)
{
	u32int v;

	v = get16(f);
	return v | get16(f) << 16;
}

static float
getfl(FILE *f)
{
	union{
		float v;
		u32int u;
	} u;

	u.u = get32(f);
	return u.v;
}

static u16int
get16b(FILE *f)
{
	u16int v;

	v = get8(f);
	return v << 8 | get8(f);
}

static void
ewrite(FILE *f, void *u, long n)
{
	if(fwrite(u, 1, n, f) != n)
		fatal("eread: short write");
}

static void
put32(FILE *f, u32int v)
{
	uchar u[4];

	PBIT32(u, v);
	ewrite(f, u, 4);
}

static void
putfl(FILE *f, float v)
{
	union{
		float v;
		u32int u;
	} w;

	w.v = v;
	put32(f, w.u);
}

static vlong
bsize(FILE *f)
{
	struct stat st;

	if(fstat(fileno(f), &st) != 0)
		fatal("fstat");
	return st.st_size;
}

static int
mkpath(char *path)
{
	char *d;

	d = path;
	if(d == nil || *d == 0)
		return -1;
	if(*d == '/')
		d++;
	while(*d != 0){
		if(*d == '/'){
			*d = 0;
			if(mkdir(path, 0770) < 0)
				return -1;
			*d = '/';
		}
		d++;
	}
	return mkdir(path, 0770);
}

static void
closelmp(FILE *f)
{
	Paklist *pl;

	for(pl=pkl; pl!=nil; pl=pl->pl)
		if(pl->p && pl->p->f == f)
			return;
	fclose(f);
}

static FILE *
openlmp(char *fname, int *len)
{
	char d[Nfspath+1];
	FILE *f;
	Paklist *pl;
	Pak *p;
	Lump *l;

	for(pl=pkl; pl != nil; pl=pl->pl){
		if(pl->p != nil){
			p = pl->p;
			l = p->l;
			while(l < p->e){
				if(strcmp(l->fname, fname) == 0){
					fseek(p->f, l->ofs, SEEK_SET);
					if(len != nil)
						*len = l->len;
					return p->f;
				}
				l++;
			}
			continue;
		}
		snprint(d, sizeof d, "%s/%s", pl->fname, fname);
		if(f = fopen(d, "rb"), f == nil)
			continue;
		if(len != nil)
			*len = bsize(f);
		return f;
	}
	//fprintf(stderr, "openlmp %s: not found\n", fname);
	return nil;
}

static uchar *
loadlmp(char *fname, int mth, int *n)
{
	int m;
	char r[32];
	uchar *buf;
	FILE *f;

	f = openlmp(fname, &m);
	if(f == nil)
		return nil;
	radix(fname, r);
	buf = nil;
	switch(mth){
	case Fhunk: buf = Hunk_Alloc(m + 1); break;
	case Fcache: buf = Cache_Alloc(loadcache, m + 1); break;
	case Fstack: buf = m + 1 <= loadsize ? loadbuf : Hunk_TempAlloc(m + 1); break;
	}
	if(buf == nil)
		fatal("loadlmp %s %d: memory allocation failed", fname, m + 1);
	buf[m] = 0;
	eread(f, buf, m);
	closelmp(f);
	if(n != nil)
		*n = m;
	return buf;
}

void *
loadhunklmp(char *f, int *n)
{
	return loadlmp(f, Fhunk, n);
}

void *
loadcachelmp(char *f, mem_user_t *c)
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
loadpoints(void)
{
	int i, n, nv;
	FILE *f;
	vec3_t v3;
	vec_t *v;
	particle_t *p;

	f = openlmp(va("maps/%s.pts", sv.name), &n);
	if(f == nil){
		Con_Printf(va("loadpoints failed\n"));
		return;
	}
	nv = 0;
	for(;;){
		if(n < 3*4+3)
			break;
		for(i=0, v=v3; i<3; i++){
			*v++ = getfl(f);
			fseek(f, 1, SEEK_CUR);
		}
		n -= 3*4+3;
		nv++;
		if(free_particles == nil){
			Con_Printf("loadpoints: insufficient free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;
		p->die = Q_MAXFLOAT;
		p->color = -nv & 15;
		p->type = pt_static;
		VectorCopy(vec3_origin, p->vel);
		VectorCopy(v3, p->org);
	}
	closelmp(f);
	Con_Printf("loadpoints: %d points read\n", nv);
}

static void
dumpcvars(FILE *f)
{
	cvar_t *c;

	for(c=cvar_vars; c!=nil; c=c->next)
		if(c->archive)
			if(fprintf(f, "%s \"%s\"\n", c->name, c->string) < 0)
				fatal("dumpcvars");
}

static void
dumpkeys(FILE *f)
{
	char **k;

	for(k=keybindings; k<keybindings+256; k++)
		if(*k != nil && **k != 0)
			fprintf(f, "bind \"%s\" \"%s\"\n",
				Key_KeynumToString(k-keybindings), *k);
}

void
dumpcfg(void)
{
	FILE *f;

	if(!host_initialized)
		return;
	f = fopen(va("%s/config.cfg", fsdir), "wb");
	if(f == nil){
		Con_DPrintf("dumpcfg failed\n");
		return;
	}
	dumpkeys(f);
	dumpcvars(f);
	fclose(f);
}

void
savnames(void)
{
	int n, *canld;
	char (*e)[Nsavcm], (*s)[Nsavcm], *p;
	char tmp[8192];
	FILE *f;

	s = savs;
	canld = savcanld;
	for(n=0, e=savs+Nsav; s<e; n++, s++, canld++){
		*canld = 0;
		memset(*s, 0, sizeof *s);
		strcpy(*s, "--- UNUSED SLOT ---");
		f = fopen(va("%s/s%d.sav", fsdir, n), "rb");
		if(f == nil){
			Con_DPrintf("savnames failed\n");
			continue;
		}
		if((p = fgets(tmp, sizeof(tmp), f), p == nil)	/* discard version */
		|| (p = fgets(tmp, sizeof(tmp), f), p == nil)){
			Con_DPrintf("savnames: short read\n");
			continue;
		}
		strncpy(*s, p, sizeof(*s)-1);
		for(p=*s; p<*(s+1); p++)
			if(*p == '_')
				*p = ' ';
		*canld = 1;
		fclose(f);
	}
}

static void
dumpedicts(FILE *f, edict_t *ed)
{
	int *vp, *ve;
	char *s;
	uchar *ev;
	ddef_t *d, *de;
	eval_t *v;

	fprintf(f, "{\n");
	if(ed->free)
		goto end;
	ev = (uchar *)&ed->v;
	de = pr_fielddefs + progs->numfielddefs;
	for(d=pr_fielddefs+1; d<de; d++){
		s = PR_Str(d->s_name);
		if(s[strlen(s)-2] == '_')
			continue;
		/* TODO: pragma pack hazard */
		vp = (int *)(ev + d->ofs * 4);
		ve = vp + type_size[d->type & ~DEF_SAVEGLOBAL];
		v = (eval_t *)vp;
		for(; vp<ve; vp++)
			if(*vp != 0)
				break;
		if(vp == ve)
			continue;
		fprintf(f, "\"%s\" ", s);
		fprintf(f, "\"%s\"\n", PR_UglyValueString(d->type, v));
	}
end:
	fprintf(f, "}\n");
}

static void
dumpdefs(FILE *f)
{
	ushort t;
	ddef_t *d, *de;

	fprintf(f, "{\n");
	de = pr_globaldefs + progs->numglobaldefs;
	for(d=pr_globaldefs; d<de; d++){
		t = d->type;
		if((t & DEF_SAVEGLOBAL) == 0)
			continue;
		t &= ~DEF_SAVEGLOBAL;
		if(t != ev_string && t != ev_float && t != ev_entity)
			continue;
		fprintf(f, "\"%s\" \"%s\"\n", PR_Str(d->s_name),
			PR_UglyValueString(t, (eval_t *)&pr_globals[d->ofs]));
	}
	fprintf(f, "}\n");
}

int
dumpsav(char *fname, char *cm)
{
	int i;
	char **s, **e;
	float *fs, *fe;
	FILE *f;

	f = fopen(fname, "wb");
	if(f == nil)
		return -1;
	fprintf(f, "%d\n%s\n", Nsavver, cm);
	fs = svs.clients->spawn_parms;
	fe = fs + nelem(svs.clients->spawn_parms);
	while(fs < fe)
		fprintf(f, "%f\n", *fs++);
	fprintf(f, "%d\n%s\n%f\n", current_skill, sv.name, sv.time);
	s = sv.lightstyles;
	e = s + nelem(sv.lightstyles);
	while(s < e){
		fprintf(f, "%s\n", *s != nil ? *s : "m");
		s++;
	}
	dumpdefs(f);
	for(i=0; i<sv.num_edicts; i++)
		dumpedicts(f, EDICT_NUM(i));
	fclose(f);
	return 0;
}

static void
loadedicts(FILE *f)
{
	int ent, c;
	char sb[32768], *s;
	edict_t *ed;

	ent = -1;
	c = 0;
	do{
		for(s=sb; s<sb+sizeof(sb)-1; s++){
			c = fgetc(f);
			if(c == EOF || c == 0)
				break;
			*s = c;
			if(c == '}'){
				s++;
				break;
			}
		}
		if(s == sb + sizeof(sb) - 1)
			fatal("loadgame: buffer overflow");
		*s = 0;
		s = COM_Parse(sb);
		if(com_token[0] == 0)
			break;
		if(strcmp(com_token, "{") != 0)
			fatal("loadgame: missing opening brace");
		if(ent == -1)
			ED_ParseGlobals(s);
		else{
			ed = EDICT_NUM(ent);
			/* TODO: pragma pack hazard */
			memset(&ed->v, 0, progs->entityfields * 4);
			ed->free = 0;
			ED_ParseEdict(s, ed);
			if(!ed->free)
				SV_LinkEdict(ed, 0);
		}
		ent++;
	}while(c != EOF);
	sv.num_edicts = ent;
}

static int
loadparms(FILE *f, char *fname)
{
	int r;
	float sp[Nparms], *p;
	char *s, **lp;
	char tmp[8192];

	r = -1;
	p = sp;
	while(p < sp + nelem(sp)){
		if(s = fgets(tmp, sizeof(tmp), f), s == nil)
			goto exit;
		*p++ = (float)strtod(s, nil);
	}
	if(s = fgets(tmp, sizeof(tmp), f), s == nil)
		goto exit;
	current_skill = (int)(strtod(s, nil) + 0.1);
	setcvarv("skill", (float)current_skill);
	if(s = fgets(tmp, sizeof(tmp), f), s == nil)
		goto exit;
	CL_Disconnect_f();
	SV_SpawnServer(s);
	if(!sv.active)
		goto exit;
	sv.paused = 1;
	sv.loadgame = 1;
	if(s = fgets(tmp, sizeof(tmp), f), s == nil)
		goto exit;
	sv.time = strtod(s, nil);
	lp = sv.lightstyles;
	while(lp < sv.lightstyles + nelem(sv.lightstyles)){
		if(s = fgets(tmp, sizeof(tmp), f), s == nil)
			goto exit;
		*lp = Hunk_Alloc(strlen(s)+1);
		strcpy(*lp++, s);
	}
	r = 0;
	loadedicts(f);
	memcpy(svs.clients->spawn_parms, sp, sizeof sp);
exit:
	return r;
}

int
loadsav(char *fname)
{
	int n, r;
	char *s;
	FILE *f;
	char tmp[8192];

	f = fopen(fname, "rb");
	if(f == nil)
		return -1;
	r = -1;
	if(s = fgets(tmp, sizeof(tmp), f), s == nil)
		goto exit;
	n = strtol(s, nil, 10);
	if(n != Nsavver){
		werrstr("invalid version %d", n);
		goto exit;
	}
	s = fgets(tmp, sizeof(tmp), f);
	r = loadparms(f, fname);
exit:
	fclose(f);
	return r;
}

void
closedm(void)
{
	if(demof == nil)
		return;
	closelmp(demof);
	demof = nil;
}

void
writedm(void)
{
	int i;

	put32(demof, net_message.cursize);
	for(i=0; i<3; i++)
		putfl(demof, cl.viewangles[i]);
	ewrite(demof, net_message.data, net_message.cursize);
}

int
readdm(void)
{
	int n;
	vec_t *f;

	fseek(demof, demoofs, SEEK_SET);
	net_message.cursize = get32(demof);
	VectorCopy(cl.mviewangles[0], cl.mviewangles[1]);
	for(n=0, f=cl.mviewangles[0]; n<3; n++)
		*f++ = getfl(demof);
	if(net_message.cursize > NET_MAXMESSAGE)
		fatal("readdm: invalid message size %d\n", net_message.cursize);
	n = fread(net_message.data, 1, net_message.cursize, demof);
	demoofs = ftell(demof);
	if(n < 0)
		Con_DPrintf("readdm: bad read\n");
	if(n != net_message.cursize){
		Con_DPrintf("readdm: short read\n");
		n = -1;
	}
	return n;
}

int
loaddm(char *fname)
{
	int n;
	char *s, tmp[8192];

	demof = openlmp(fname, &n);
	if(demof == nil)
		return -1;
	s = fgets(tmp, sizeof(tmp), demof);
	n = strlen(s) - 1;
	if(s == nil || n < 0 || n > 11){
		Con_DPrintf("loaddm: invalid trk field\n");
		closelmp(demof);
		return -1;
	}
	demoofs = ftell(demof);
	s[n] = 0;
	cls.forcetrack =  strtol(s, nil, 10);
	return 0;
}

int
opendm(char *f, int trk)
{
	char s[16];

	demof = fopen(f, "wb");
	if(demof == nil)
		return -1;
	sprint(s, "%d\n", trk);
	ewrite(demof, s, strlen(s));
	return 0;
}

static Pak *
pak(char *fname)
{
	int n, ofs, len, nlmp;
	uchar u[8];
	FILE *f;
	Lump *l;
	Pak *p;

	f = fopen(fname, "rb");
	if(f == nil)
		return nil;
	memset(u, 0, sizeof u);
	eread(f, u, 4);
	if(memcmp(u, "PACK", 4) != 0)
		fatal("pak %s: invalid pak file", fname);
	ofs = get32(f);
	len = get32(f);
	nlmp = len / Npaksz;
	if(nlmp > Npaklmp)
		fatal("pak %s: invalid lump number %d", fname, nlmp);
	if(nlmp != Npak0lmp)
		notid1 = 1;
	l = Hunk_Alloc(nlmp * sizeof *l);
	p = Hunk_Alloc(sizeof *p);
	snprint(p->fname, sizeof(p->fname), "%s", fname);
	p->f = f;
	p->l = l;
	p->e = l + nlmp;
	fseek(f, ofs, SEEK_SET);
	initcrc();
	while(l < p->e){
		eread(f, l->fname, 56);
		for(n=0; n<56; n++)
			crc(l->fname[n]);
		eread(f, u, 8);
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
	strncpy(pl->fname, d, sizeof(pl->fname)-1);
	pl->pl = pkl;
	pkl = pl;
	for(n=0; ; n++){
		snprint(f, sizeof f, "%s/pak%d.pak", d, n);
		p = pak(f);
		if(p == nil){
			Con_DPrintf("pakdir: %r\n");
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
	char *home;

	pakdir("/usr/games/quake/id1");
	if(game != nil){
		if(strcmp(game, "rogue") == 0){
			rogue = 1;
			standard_quake = 0;
		}else if(strcmp(game, "hipnotic") == 0){
			hipnotic = 1;
			standard_quake = 0;
		}else
			notid1 = 1;
		pakdir(va("/usr/games/quake/%s", game));
	}
	if((home = getenv("HOME")) != nil){
		pakdir(va("%s/.quake/id1", home));
		if(game != nil)
			pakdir(va("%s/.quake/%s", home, game));
		mkpath(fsdir);
	}
}

static void
chkreg(void)
{
	u16int *p;
	FILE *f;

	Cvar_RegisterVariable(&registered);
	f = openlmp("gfx/pop.lmp", nil);
	if(f == nil){
		Con_DPrintf("chkreg: shareware version\n");
		if(notid1)
			fatal("chkreg: phase error");
		return;
	}
	p = pop;
	while(p < pop + nelem(pop))
		if(*p++ != get16b(f))
			fatal("chkreg: corrupted pop lump");
	closelmp(f);
	setcvar("registered", "1");
	Con_DPrintf("chkreg: registered version\n");
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
}
