#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "quakedef.h"

cvar_t bgmvolume = {"bgmvolume", "1", 1};
cvar_t volume = {"volume", "0.7", 1};

typedef struct Chan Chan;

enum{
	Te9 = 1000000000,
	Fpsmin = 10,
	Fpsmax = 72,
	Srate = 44100,
	Ssize = 2,
	Sch = 2,
	Sblk = Ssize * Sch,
	Ssamp = Srate / Fpsmin,
	Snbuf = Ssamp * Sblk,
	Nchan = 128,
	Ndyn = 8,
	Sstat = Ndyn + NUM_AMBIENTS
};
static float Clipdist = 1000.0;

struct Chan{
	sfx_t *sfx;
	int chvol;
	int lvol;
	int rvol;
	vec_t attf;
	vec3_t zp;
	int entn;
	int entch;
	int p;
	int n;
};
static Chan chans[Nchan], *che;

static int afd = -1;
static uchar mixbuf[Snbuf];
static vlong sndt;
static int nsamp;
static int sampbuf[Ssamp*Sch*sizeof(int)];
static int scalt[32][256];

static sfx_t *ambsfx[NUM_AMBIENTS];

static char cdfile[13];
static int ntrk;
static int cdfd = -1;
static int cdread, cdloop, cdvol;

typedef struct
{
	int 	length;
	int 	speed;
	int 	width;
	int 	stereo;
	int loop;
	byte	data[1];		// variable sized
} sfxcache_t;

typedef struct
{
	int		rate;
	int		width;
	int		channels;
	int		loopofs;
	int		samples;
	int		dataofs;
} wavinfo_t;

static vec3_t		listener_origin;
static vec3_t		listener_forward;
static vec3_t		listener_right;
static vec3_t		listener_up;
#define	MAX_SFX		512
static sfx_t		*known_sfx;		// hunk allocated [MAX_SFX]
static int			num_sfx;

static cvar_t precache = {"precache", "1"};
static cvar_t loadas8bit = {"loadas8bit", "0"};
static cvar_t ambient_level = {"ambient_level", "0.3"};
static cvar_t ambient_fade = {"ambient_fade", "100"};

static byte	*data_p;
static byte 	*iff_end;
static byte 	*last_chunk;
static byte 	*iff_data;

// QuakeWorld hack...
#define	viewentity	playernum+1

/* TODO: refuctor wav loading */
static void
resample(sfxcache_t *sc, byte *data, float stepscale)
{
	int inwidth;
	int		outcount;
	int		srcsample;
	int		i;
	int		sample, samplefrac, fracstep;

	inwidth = sc->width;
	outcount = sc->length / stepscale;
	sc->length = outcount;
	sc->speed = Srate;
	if (loadas8bit.value)
		sc->width = 1;
	else
		sc->width = inwidth;
	sc->stereo = 0;

	if (stepscale == 1 && inwidth == 1 && sc->width == 1)
	{
// fast special case
		for (i=0 ; i<outcount ; i++)
			((signed char *)sc->data)[i]
			= (int)( (unsigned char)(data[i]) - 128);
	}
	else
	{
// general case
		samplefrac = 0;
		fracstep = stepscale*256;
		for (i=0 ; i<outcount ; i++)
		{
			srcsample = samplefrac >> 8;
			samplefrac += fracstep;
			if (inwidth == 2)
				sample = LittleShort ( ((short *)data)[srcsample] );
			else
				sample = (int)( (unsigned char)(data[srcsample]) - 128) << 8;
			if (sc->width == 2)
				((short *)sc->data)[i] = sample;
			else
				((signed char *)sc->data)[i] = sample >> 8;
		}
	}
}

static short
GetLittleShort(void)
{
	short val;

	val = *data_p;
	val = val + (*(data_p+1)<<8);
	data_p += 2;
	return val;
}

static int
GetLittleLong(void)
{
	int val;

	val = *data_p;
	val = val + (*(data_p+1)<<8);
	val = val + (*(data_p+2)<<16);
	val = val + (*(data_p+3)<<24);
	data_p += 4;
	return val;
}

static void
FindNextChunk(char *name)
{
	int iff_chunk_len;

	while (1)
	{
		data_p=last_chunk;

		if (data_p >= iff_end)
		{	// didn't find the chunk
			data_p = nil;
			return;
		}
		
		data_p += 4;
		iff_chunk_len = GetLittleLong();
		if (iff_chunk_len < 0)
		{
			data_p = nil;
			return;
		}
//		if (iff_chunk_len > 1024*1024)
//			Sys_Error ("FindNextChunk: %d length is past the 1 meg sanity limit", iff_chunk_len);
		data_p -= 8;
		last_chunk = data_p + 8 + ( (iff_chunk_len + 1) & ~1 );
		if(strncmp((char *)data_p, name, 4) == 0)
			return;
	}
}

static void
FindChunk(char *name)
{
	last_chunk = iff_data;
	FindNextChunk (name);
}

static wavinfo_t
GetWavinfo(char *name, byte *wav, vlong wavlength)
{
	wavinfo_t	info;
	int     i;
	int     format;
	int		samples;

	memset(&info, 0, sizeof info);

	if (!wav)
		return info;
		
	iff_data = wav;
	iff_end = wav + wavlength;

// find "RIFF" chunk
	FindChunk("RIFF");
	if(!(data_p && strncmp((char *)data_p+8, "WAVE", 4) == 0))
	{
		Con_Printf("Missing RIFF/WAVE chunks\n");
		return info;
	}

// get "fmt " chunk
	iff_data = data_p + 12;

	FindChunk("fmt ");
	if (!data_p)
	{
		Con_Printf("Missing fmt chunk\n");
		return info;
	}
	data_p += 8;
	format = GetLittleShort();
	if (format != 1)
	{
		Con_Printf("Microsoft PCM format only\n");
		return info;
	}

	info.channels = GetLittleShort();
	info.rate = GetLittleLong();
	data_p += 4+2;
	info.width = GetLittleShort() / 8;

// get cue chunk
	FindChunk("cue ");
	if (data_p)
	{
		data_p += 32;
		info.loopofs = GetLittleLong();

	// if the next chunk is a LIST chunk, look for a cue length marker
		FindNextChunk ("LIST");
		if (data_p)
		{
			if(strncmp((char *)data_p+28, "mark", 4) == 0)
			{	// this is not a proper parse, but it works with cooledit...
				data_p += 24;
				i = GetLittleLong ();	// samples in loop
				info.samples = info.loopofs + i;
//				Con_Printf("looped length: %d\n", i);
			}
		}
	}
	else
		info.loopofs = -1;

// find data chunk
	FindChunk("data");
	if (!data_p)
	{
		Con_Printf("Missing data chunk\n");
		return info;
	}

	data_p += 4;
	samples = GetLittleLong () / info.width;

	if (info.samples)
	{
		if (samples < info.samples)
			Sys_Error ("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	
	return info;
}

static sfxcache_t *
loadsfx(sfx_t *sfx)
{
	wavinfo_t info;
	int len;
	float stepscale;
	sfxcache_t *sc;
	uchar *u, buf[1024];	/* avoid dirtying the cache heap */

	if(sc = Cache_Check(&sfx->cache), sc != nil)
		return sc;
	u = COM_LoadStackFile(va("sound/%s", sfx->name), buf, sizeof buf);
	if(u == nil){
		fprint(2, "loadsfx: %r\n");
		return nil;
	}
	info = GetWavinfo(sfx->name, u, com_filesize);
	if(info.channels != 1){
		fprint(2, "loadsfx: non mono wave %s\n", sfx->name);
		return nil;
	}
	stepscale = (float)info.rate / Srate;	
	len = info.samples / stepscale;
	len *= info.width * info.channels;
	if(sc = Cache_Alloc(&sfx->cache, len + sizeof *sc, sfx->name), sc == nil)
		return nil;
	sc->length = info.samples;
	sc->loop = info.loopofs;
	if(info.loopofs >= 0)
		sc->loop /= stepscale;
	sc->speed = info.rate;
	sc->width = info.width;
	sc->stereo = info.channels;
	resample(sc, u + info.dataofs, stepscale);
	return sc;
}

void
stepcd(void)
{
	cdvol = bgmvolume.value * 256;
	cdread = cdfd >= 0 && cdvol > 0;
}

static void
sndout(void)
{
	int v, vol, *pb, *pe;
	uchar *p;

	vol = volume.value * 256;
	p = mixbuf;
	pb = sampbuf;
	pe = sampbuf + nsamp * 2;
	while(pb < pe){
		v = (short)(p[1] << 8 | p[0]) * cdvol >> 8;
		v += *pb++ * vol >> 8;
		if(v > 0x7fff)
			v = 0x7fff;
		else if(v < -0x8000)
			v = -0x8000;
		p[0] = v;
		p[1] = v >> 8;
		p += 2;
	}
}

static void
sample8(Chan *c, void *d, int n)
{
	int v, *pb, *pe, *ls, *rs;
	uchar *p;

	if(c->lvol > 255)
		c->lvol = 255;
	if(c->rvol > 255)
		c->rvol = 255;
	ls = scalt[c->lvol >> 3];
	rs = scalt[c->rvol >> 3];
	p = (uchar *)d + c->p;
	pb = sampbuf;
	pe = sampbuf + n * 2;
	while(pb < pe){
		v = *p++;
		*pb++ += ls[v];
		*pb++ += rs[v];
	}
}

static void
sample16(Chan *c, void *d, int n)
{
	int v, *pb, *pe, lv, rv;
	short *p;

	lv = c->lvol;
	rv = c->rvol;
	p = (short *)d + c->p;
	pb = sampbuf;
	pe = sampbuf + n * 2;
	while(pb < pe){
		v = *p++;
		*pb++ += v * lv >> 8;
		*pb++ += v * rv >> 8;
	}
}

static void
samplesfx(void)
{
	int n, m;
	Chan *c;
	sfxcache_t *sc;
	void (*sf)(Chan *, void *, int);

	memset(sampbuf, 0, sizeof sampbuf);
	for(c=chans; c<che; c++){
		if(c->sfx == nil)
			continue;
		if(c->lvol == 0 && c->rvol == 0)
			continue;
		if(sc = loadsfx(c->sfx), sc == nil)
			continue;
		sf = sc->width == 1 ? sample8 : sample16;
		n = nsamp;
		while(n > 0){
			m = n < c->n ? n : c->n;
			if(m > 0)
				sf(c, sc->data, m);
			c->p += m;
			c->n -= m;
			n -= m;
			if(c->n <= 0){
				if(sc->loop >= 0){
					c->p = sc->loop;
					c->n = sc->length - c->p;
				}else{
					c->sfx = nil;
					break;
				}
			}
		}
	}
	sndout();
}

void
stopcd(void)
{
	if(cdfd >= 0)
		close(cdfd);
	cdread = 0;
	cdloop = 0;
}

void
pausecd(void)
{
	cdread = 0;
}

void
resumecd(void)
{
	cdread = 1;
}

static void
readcd(int ns)
{
	int n;

	if(cdfd < 0 || !cdread)
		return;
	if(n = readn(cdfd, mixbuf, ns), n != ns){
		if(n < 0 || !cdloop)
			stopcd();
		else{
			seek(cdfd, 0, 0);
			ns -= n;
			if(n = readn(cdfd, mixbuf+n, ns), n != ns)
				stopcd();
		}
	}
}

static void
spatialize(Chan *c)
{
	vec_t Δr, m;
	vec3_t src;

	if(c->entn == cl.viewentity){
		c->lvol = c->chvol;
		c->rvol = c->chvol;
		return;
	}
	VectorSubtract(c->zp, listener_origin, src);
	Δr = 1.0 - VectorNormalize(src) * c->attf;
	m = DotProduct(listener_right, src);
	c->rvol = Δr * (1.0 + m) * c->chvol;
	if(c->rvol < 0)
		c->rvol = 0;
	c->lvol = Δr * (1.0 - m) * c->chvol;
	if(c->lvol < 0)
		c->lvol = 0;
}

static void
ambs(void)
{
	uchar *av;
	float vol;
	Chan *c, *e;
	mleaf_t *l;
	sfx_t **sfx;

	if(cl.worldmodel == nil)
		return;
	c = chans;
	e = chans + NUM_AMBIENTS;
	l = Mod_PointInLeaf(listener_origin, cl.worldmodel);
	if(l == nil || !ambient_level.value){
		while(c < e)
			c++->sfx = nil;
		return;
	}
	sfx = ambsfx;
	av = l->ambient_sound_level;
	while(c < e){
		c->sfx = *sfx++;
		vol = ambient_level.value * *av++;
		if(vol < 8)
			vol = 0;
		if(c->chvol < vol){
			c->chvol += host_frametime * ambient_fade.value;
			if(c->chvol > vol)
				c->chvol = vol;
		}else if(c->chvol > vol){
			c->chvol -= host_frametime * ambient_fade.value;
			if(c->chvol < vol)
				c->chvol = vol;
		}
		c->lvol = c->chvol;
		c->rvol = c->chvol;
		c++;
	}
}

void
stepsnd(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	int ns;
	Chan *c, *sum;

	if(afd < 0)
		return;
	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);
	ambs();
	sum = nil;
	for(c=chans+NUM_AMBIENTS; c<che; c++){
		if(c->sfx == nil)
			continue;
		spatialize(c);
		if(c->lvol == 0 && c->rvol == 0)
			continue;
		/* sum static sounds to avoid useless remixing */
		if(c >= chans + Sstat){
			if(sum != nil && sum->sfx == c->sfx){
				sum->lvol += c->lvol;
				sum->rvol += c->rvol;
				c->lvol = c->rvol = 0;
				continue;
			}
			for(sum=chans + Sstat; sum<c; sum++)
				if(sum->sfx == c->sfx)
					break;
			if(sum == che)
				sum = nil;
			else if(sum != c){
				sum->lvol += c->lvol;
				sum->rvol += c->rvol;
				c->lvol = c->rvol = 0;
			}
		}
	}
	if(sndt == 0)
		sndt = nanosec() - Te9 / Fpsmax;
	nsamp = (nanosec() - sndt) / (Te9 / Srate);
	if(!cls.timedemo)
		nsamp = nsamp + 15 & ~15;
	if(nsamp > Ssamp)
		nsamp = Ssamp;
	ns = nsamp * Sblk;
	memset(mixbuf, 0, ns);
	readcd(ns);
	samplesfx();
	if(write(afd, mixbuf, ns) != ns){
		fprint(2, "sndwrite: %r\n");
		shutsnd();
	}
	sndt = nanosec();
}

void
startcd(int nt, int loop)
{
	if(ntrk < 1)
		return;
	nt -= 1;	/* d001 assumed part of track list */
	if(nt < 1 || nt > ntrk){
		fprint(2, "startcd: invalid track number %d\n", nt);
		return;
	}
	if(cdfd = open(va("%s%03d", cdfile, nt), OREAD), cdfd < 0){
		fprint(2, "startcd: open: %r\n");
		return;
	}
	cdloop = loop;
	if(cdvol > 0)
		cdread = 1;
}

void
stopallsfx(void)
{
	if(afd < 0)
		return;
	memset(chans, 0, sizeof chans);
	che = chans + Sstat;
}

void
stopsfx(int n, int ch)
{
	Chan *c, *e;

	if(afd < 0)
		return;
	c = chans;
	e = chans + Ndyn;
	while(c < e){
		if(c->entn == n && c->entch == ch){
			c->sfx = nil;
			return;
		}
		c++;
	}
}

static Chan *
pickchan(int entn, int entch)
{
	int Δt;
	Chan *c, *p, *e;

	p = nil;
	Δt = 0x7fffffff;
	for(c=chans+NUM_AMBIENTS, e=c+Ndyn; c<e; c++){
		if(entch != 0 && c->entn == entn
		&& (c->entch == entch || entch == -1)){
			p = c;
			break;
		}
		if(c->entn == cl.viewentity && entn != cl.viewentity && c->sfx != nil)
			continue;
		if(c->n < Δt){
			Δt = c->n;
			p = c;
		}
	}
	if(p != nil)
		p->sfx = nil;
	return p;
}       

void
startsfx(int entn, int entch, sfx_t *sfx, vec3_t zp, float vol, float att)
{
	int skip;
	Chan *c, *c2, *e;
	sfxcache_t *sc;

	if(afd < 0 || sfx == nil)
		return;
	if(c = pickchan(entn, entch), c == nil)
		return;
	memset(c, 0, sizeof *c);
	VectorCopy(zp, c->zp);
	c->attf = att / Clipdist;
	c->chvol = vol * 255;
	c->entn = entn;
	c->entch = entch;
	spatialize(c);
	if(c->lvol == 0 && c->rvol == 0)
		return;
	if(sc = loadsfx(sfx), sc == nil)
		return;
	c->sfx = sfx;
	c->p = 0;
	c->n = sc->length;
	/* don't sum identical sfx started on the same frame */
	for(c2=chans+NUM_AMBIENTS, e=chans+Sstat; c2<e; c2++){
		if(c2 == c || c2->sfx != sfx || c2->p != 0)
			continue;
		skip = nrand(Srate / Fpsmin);
		if(skip >= c->n)
			skip = c->n - 1;
		c->p += skip;
		c->n -= skip;
		break;
	}
}

void
localsfx(char *s)
{
	sfx_t *sfx;

	if(afd < 0)
		return;
	sfx = precachesfx(s);
	startsfx(cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

void
staticsfx(sfx_t *sfx, vec3_t zp, float vol, float att)
{
	Chan *c;
	sfxcache_t *sc;

	if(sfx == nil)
		return;
	if(che >= chans + nelem(chans)){
		fprint(2, "staticsfx: channel overflow\n");
		return;
	}
	c = che++;
	if(sc = loadsfx(sfx), sc == nil)
		return;
	if(sc->loop < 0){
		fprint(2, "staticsfx %s: nonlooped static sound\n", sfx->name);
		return;
	}
	c->sfx = sfx;
	VectorCopy(zp, c->zp);
	c->chvol = vol;
	c->attf = (att / 64) / Clipdist;
	c->n = sc->length;
	spatialize(c);
}

static sfx_t *
findsfx(char *s)
{
	sfx_t *sfx, *e;

	if(s == nil)
		Sys_Error("findsfx: nil pointer\n");
	if(strlen(s) >= MAX_QPATH)
		Sys_Error("findsfx: path too long %s", s);
	sfx = known_sfx;
	e = known_sfx + num_sfx;
	while(sfx < e){
		if(strcmp(sfx->name, s) == 0)
			return sfx;
		sfx++;
	}
	if(num_sfx == MAX_SFX)
		Sys_Error("findsfx: sfx list overflow");
	strcpy(sfx->name, s);
	num_sfx++;
	return sfx;
}

void
touchsfx(char *s)
{
	sfx_t *sfx;

	if(afd < 0)
		return;
	sfx = findsfx(s);
	Cache_Check(&sfx->cache);
}

sfx_t *
precachesfx(char *s)
{
	sfx_t *sfx;

	if(afd < 0)
		return nil;
	sfx = findsfx(s);
	if(precache.value)
		loadsfx(sfx);
	return sfx;
}

static void
cdcmd(void)
{
	char *c;

	if(Cmd_Argc() < 2){
usage:
		Con_Printf("cd (play|loop|stop|pause|resume|info) [track]\n");
		return;
	}
	c = Cmd_Argv(1);
	if(cistrcmp(c, "play") == 0){
		if(Cmd_Argc() < 2)
			goto usage;
		startcd(atoi(Cmd_Argv(2)), 0);
	}else if(cistrcmp(c, "loop") == 0){
		if(Cmd_Argc() < 2)
			goto usage;
		startcd(atoi(Cmd_Argv(2)), 1);
	}else if(cistrcmp(c, "stop") == 0)
		stopcd();
	else if(cistrcmp(c, "pause") == 0)
		pausecd();
	else if(cistrcmp(c, "resume") == 0)
		resumecd();
}

static void
playsfx(void)
{
	static int hash = 345;
	int i;
	char *s;
	sfx_t *sfx;

	if(Cmd_Argc() < 2){
		Con_Printf("play wav [wav..]: play a wav lump\n");
		return;
	}
	i = 1;
	while(i < Cmd_Argc()){
		if(strrchr(Cmd_Argv(i), '.') == nil)
			s = va("%s.wav", Cmd_Argv(i));
		else
			s = Cmd_Argv(i);
		sfx = precachesfx(s);
		startsfx(hash++, 0, sfx, listener_origin, 1.0, 1.0);
		i++;
	}
}

static void
playvolsfx(void)
{
	static int hash = 543;
	int i;
	float vol;
	char *s;
	sfx_t *sfx;

	if(Cmd_Argc() < 3){
		Con_Printf("play wav vol [wav vol]..: play an amplified wav lump\n");
		return;
	}
	i = 1;
	while(i < Cmd_Argc()){
		if(strrchr(Cmd_Argv(i), '.') == nil)
			s = va("%s.wav", Cmd_Argv(i));
		else
			s = Cmd_Argv(i);
		sfx = precachesfx(s);
		vol = atof(Cmd_Argv(++i));
		startsfx(hash++, 0, sfx, listener_origin, vol, 1.0);
		i++;
	}
}

static void
sfxlist(void)
{
	char c;
	int sz, sum;
	sfx_t *sfx, *e;
	sfxcache_t *sc;

	sum = 0;
	for(sfx=known_sfx, e=known_sfx+num_sfx; sfx<e; sfx++){
		if(sc = Cache_Check(&sfx->cache), sc == nil)
			continue;
		sz = sc->length * sc->width * (sc->stereo + 1);
		sum += sz;
		c = sc->loop >= 0 ? 'L' : ' ';
		Con_Printf("%c(%2db) %6d : %s\n", c, sc->width * 8, sz, sfx->name);
	}
	Con_Printf("Total resident: %d\n", sum);
}

void
shutcd(void)
{
	stopcd();
}

void
shutsnd(void)
{
	if(afd < 0)
		return;
	close(afd);
}

static int
cdinfo(void)
{
	int fd, i, n, nt;
	char *t, types[] = {'a', 'u', 0};
	Dir *d;

	ntrk = 0;
	if(fd = open("/mnt/cd", OREAD), fd < 0)
		return -1;
	if(n = dirreadall(fd, &d), n < 0){
		close(fd);
		return -1;
	}
	close(fd);
	t = types;
	for(;;){
		for(nt=0, i=0; i<n; i++)
			if(strcmp(d[i].name, va("%c%03d", *t, ntrk+1)) == 0){
				ntrk++;
				nt = 1;
			}
		if(ntrk < 1){
			if(*++t == 0){
				werrstr("cdinfo: no tracks found");
				break;
			}
		}else if(nt == 0){
			snprint(cdfile, sizeof cdfile, "/mnt/cd/%c", *t);
			break;
		}
	}
	free(d);
	return ntrk < 1 ? -1 : 0;
}

int
initcd(void)
{
	if(cdinfo() < 0)
		return -1;
	Cmd_AddCommand("cd", cdcmd);
	return 0;
}

int
initsnd(quakeparms_t *q)
{
	int i, j, *p;

	if(afd = open("/dev/audio", OWRITE), afd < 0)
		return -1;
	for(p=scalt[1], i=8; i<8*nelem(scalt); i+=8)
		for(j=0; j<256; j++)
			*p++ = (char)j * i;
	Cmd_AddCommand("play", playsfx);
	Cmd_AddCommand("playvol", playvolsfx);
	Cmd_AddCommand("stopsound", stopallsfx);
	Cmd_AddCommand("soundlist", sfxlist);
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&precache);
	Cvar_RegisterVariable(&loadas8bit);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	if(q->memsize < 0x800000){
		Cvar_Set("loadas8bit", "1");
		fprint(2, "initsnd: forcing 8bit width\n");
	}
	known_sfx = Hunk_AllocName(MAX_SFX * sizeof *known_sfx, "sfx_t");
	num_sfx = 0;
	ambsfx[AMBIENT_WATER] = precachesfx("ambience/water1.wav");
	ambsfx[AMBIENT_SKY] = precachesfx("ambience/wind2.wav");
	stopallsfx();
	return 0;
}
