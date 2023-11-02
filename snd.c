#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

cvar_t volume = {"volume", "0.7", 1};

typedef struct Chan Chan;

enum{
	Srate = 44100,
	Ssize = 2,
	Sch = 2,
	Sblk = Ssize * Sch,
	Ssamp = Srate / Fpsmin,
	Snbuf = Ssamp * Sblk,

	Nchan = 256,
	Ndyn = 8,
	Sstat = Ndyn + Namb
};
static float Clipdist = 1000.0;

struct Chan{
	Sfx *sfx;
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

static Chan *chans, *che;

static int afd = -1, mixbufi;
static uchar mixbufs[2][Snbuf], *mixbuf = mixbufs[0];
static vlong sndt, sampt;
static int nsamp;
static int sampbuf[Ssamp*Sch*sizeof(int)];
static int scalt[32][256];

static Sfx *ambsfx[Namb];

typedef struct
{
	int 	length;
	int 	speed;
	int 	width;
	int 	stereo;
	int		loop;
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
static Sfx			*known_sfx;		// hunk allocated
static int			num_sfx;
static int			map;

static cvar_t precache = {"precache", "1"};
static cvar_t loadas8bit = {"loadas8bit", "0"};
static cvar_t ambient_level = {"ambient_level", "0.3"};
static cvar_t ambient_fade = {"ambient_fade", "100"};

static byte	*data_p;
static byte 	*iff_end;
static byte 	*last_chunk;
static byte 	*iff_data;

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
//			fatal ("FindNextChunk: %d length is past the 1 meg sanity limit", iff_chunk_len);
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
			fatal ("Sound %s has a bad loop length", name);
	}
	else
		info.samples = samples;

	info.dataofs = data_p - wav;
	
	return info;
}

static sfxcache_t *
loadsfx(Sfx *sfx)
{
	wavinfo_t info;
	int len;
	float stepscale;
	sfxcache_t *sc;
	uchar *u, buf[1024];	/* avoid dirtying the cache heap */

	if(sc = Cache_Check(&sfx->cu), sc != nil)
		return sc;
	u = loadstklmp(va("sound/%s", sfx->s), buf, sizeof buf, &len);
	if(u == nil){
		Con_DPrintf("loadsfx: %r\n");
		return nil;
	}
	info = GetWavinfo(sfx->s, u, len);
	if(info.channels != 1){
		Con_DPrintf("loadsfx: non mono wave %s\n", sfx->s);
		return nil;
	}
	stepscale = (float)info.rate / Srate;	
	len = info.samples / stepscale;
	len *= info.width * info.channels;
	if(sc = Cache_Alloc(&sfx->cu, len + sizeof *sc), sc == nil)
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

static void
sndout(void)
{
	int v, *pb, *pe;
	uchar *p;
	double vol;

	vol = volume.value;
	p = mixbuf;
	pb = sampbuf;
	pe = sampbuf + nsamp * 2;
	while(pb < pe){
		v = *pb++ * vol;
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
	Sfx **sfx;

	if(cl.worldmodel == nil)
		return;
	c = chans;
	e = chans + Namb;
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

static void
auproc(void *p)
{
	long sz;
	uchar *m;

	for(;;){
		if((sz = recvul(p)) == 0)
			break;
		m = mixbufs[0];
		if(sz < 0){
			m = mixbufs[1];
			sz = -sz;
		}
		if(write(afd, m, sz) != sz){
			Con_DPrintf("sndwrite: %r\n");
			shutsnd();
			break;
		}
	}
	chanclose(p);
	threadexits(nil);
}

void
stepsnd(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
	long ns;
	Chan *c, *sum;
	static Channel *ach;

	if(afd < 0)
		return;
	if(ach == nil){
		ach = chancreate(sizeof(ulong), 0);
		proccreate(auproc, ach, 4096);
	}
	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);
	ambs();
	sum = nil;
	for(c=chans+Namb; c<che; c++){
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
	samplesfx();
	sampt += nsamp;
	if(ns != 0){
		sendul(ach, mixbufi == 0 ? ns : -ns);
		mixbufi = (mixbufi + 1) % 2;
		mixbuf = mixbufs[mixbufi];
	}
	sndt = nanosec();
}

void
stopallsfx(void)
{
	if(afd < 0)
		return;
	memset(chans, 0, sizeof(*chans)*Nchan);
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
	for(c=chans+Namb, e=c+Ndyn; c<e; c++){
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
startsfx(int entn, int entch, Sfx *sfx, vec3_t zp, float vol, float att)
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
	for(c2=chans+Namb, e=chans+Sstat; c2<e; c2++){
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
	Sfx *sfx;

	if(afd < 0)
		return;
	sfx = precachesfx(s);
	startsfx(cl.viewentity, -1, sfx, vec3_origin, 1, 1);
}

void
staticsfx(Sfx *sfx, vec3_t zp, float vol, float att)
{
	Chan *c;
	sfxcache_t *sc;

	if(sfx == nil || che >= chans+Nchan)
		return;
	c = che++;
	if(sc = loadsfx(sfx), sc == nil)
		return;
	if(sc->loop < 0){
		Con_DPrintf("staticsfx %s: nonlooped static sound\n", sfx->s);
		return;
	}
	c->sfx = sfx;
	VectorCopy(zp, c->zp);
	c->chvol = vol;
	c->attf = (att / 64) / Clipdist;
	c->n = sc->length;
	spatialize(c);
}

static Sfx *
findsfx(char *s)
{
	Sfx *sfx, *e;

	if(strlen(s) >= Npath)
		Host_Error("findsfx: path too long %s", s);
	sfx = known_sfx;
	e = known_sfx + num_sfx;
	while(sfx < e){
		if(strcmp(sfx->s, s) == 0)
			return sfx;
		sfx++;
	}
	if(num_sfx == MAX_SOUNDS){
		sfx = known_sfx;
		while(sfx < e){
			if(sfx->map && sfx->map < map)
				break;
			sfx++;
		}
		if(sfx == e)
			Host_Error("findsfx: sfx list overflow: %s", s);
		if(Cache_Check(&sfx->cu))
			Cache_Free(&sfx->cu);
	}else
		num_sfx++;
	strcpy(sfx->s, s);
	return sfx;
}

void
touchsfx(char *s)
{
	Sfx *sfx;

	if(afd < 0)
		return;
	sfx = findsfx(s);
	Cache_Check(&sfx->cu);
}

Sfx *
precachesfx(char *s)
{
	Sfx *sfx;

	if(afd < 0)
		return nil;
	sfx = findsfx(s);
	sfx->map = map;
	if(precache.value)
		loadsfx(sfx);
	return sfx;
}

static void
playsfx(void)
{
	static int hash = 345;
	int i;
	char *s;
	Sfx *sfx;

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
	Sfx *sfx;

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
	Sfx *sfx, *e;
	sfxcache_t *sc;

	sum = 0;
	for(sfx=known_sfx, e=known_sfx+num_sfx; sfx<e; sfx++){
		if(sc = Cache_Check(&sfx->cu), sc == nil)
			continue;
		sz = sc->length * sc->width * (sc->stereo + 1);
		sum += sz;
		c = sc->loop >= 0 ? 'L' : ' ';
		Con_Printf("%c(%2db) %6d : %s\n", c, sc->width * 8, sz, sfx->s);
	}
	Con_Printf("Total resident: %d\n", sum);
}

void
sfxbegin(void)
{
	map++;
}

void
shutsnd(void)
{
	if(afd < 0)
		return;
	close(afd);
	afd = -1;
}

int
initsnd(void)
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
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);

	chans = calloc(Nchan, sizeof(*chans));
	known_sfx = Hunk_Alloc(MAX_SOUNDS * sizeof *known_sfx);
	num_sfx = 0;
	
	ambsfx[Ambwater] = precachesfx("ambience/water1.wav");
	ambsfx[Ambsky] = precachesfx("ambience/wind2.wav");
	stopallsfx();
	return 0;
}
