#include "quakedef.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

typedef struct alsfx_t alsfx_t;
typedef struct alchan_t alchan_t;

struct alsfx_t {
	ALuint buf;
	bool loop;
	bool upsample;
};

struct alchan_t {
	int ent;
	int ch;
	ALuint src;

	alchan_t *prev, *next;
};

enum {
	Srcstatic = -666,
};

cvar_t volume = {"volume", "0.7", 1};

static cvar_t s_al_dev = {"s_al_device", "-1", true};
static int s_al_dev_prev = -2;

static cvar_t s_al_hrtf = {"s_al_hrtf", "0", true};
static cvar_t s_al_resampler_default = {"s_al_resampler_default", "6", true}; // 23rd order Sinc
static cvar_t s_al_resampler_up = {"s_al_resampler_up", "1", true}; // Linear
static cvar_t ambient_level = {"ambient_level", "0.3"};
static cvar_t ambient_fade = {"ambient_fade", "100"};

static ALCcontext *ctx;
static ALCdevice *dev;
static ALuint localsrc;
static Sfx *known_sfx;
static int num_sfx;
static int map;
static alchan_t *chans;

static int al_default_resampler, al_num_resamplers;
static ALchar *(*qk1alGetStringiSOFT)(ALenum, ALsizei);
static ALCchar *(*qk1alcGetStringiSOFT)(ALCdevice *, ALenum, ALsizei);
static ALCboolean (*qk1alcResetDeviceSOFT)(ALCdevice *, const ALCint *attr);
static ALCboolean *(*qk1alcReopenDeviceSOFT)(ALCdevice *, const ALCchar *devname, const ALCint *attr);

#define ALERR() alcheckerr(__FILE__, __LINE__)

static int
alcheckerr(const char *file, int line)
{
	int e, ret;
	char *s, tmp[32];

	ret = 0;
	if(ctx != nil && (e = alGetError()) != AL_NO_ERROR){
		switch(e){
		case AL_INVALID_NAME: s = "invalid name"; break;
		case AL_INVALID_ENUM: s = "invalid enum"; break;
		case AL_INVALID_VALUE: s = "invalid value"; break;
		case AL_INVALID_OPERATION: s = "invalid operation"; break;
		case AL_OUT_OF_MEMORY: s = "out of memory"; break;
		default:
			snprint(tmp, sizeof(tmp), "unknown (0x%x)", e);
			s = tmp;
			break;
		}
		ret |= e;
		fprintf(stderr, "AL: %s:%d: %s\n", file, line, s);
	}
	if(dev != nil && (e = alcGetError(dev)) != ALC_NO_ERROR){
		switch(e){
		case ALC_INVALID_DEVICE: s = "invalid device"; break;
		case ALC_INVALID_ENUM: s = "invalid enum"; break;
		case ALC_INVALID_VALUE: s = "invalid value"; break;
		case ALC_INVALID_CONTEXT: s = "invalid context"; break;
		case ALC_OUT_OF_MEMORY: s = "out of memory"; break;
		default:
			snprint(tmp, sizeof(tmp), "unknown error (0x%x)", e);
			s = tmp;
			break;
		}
		ret |= e;
		fprintf(stderr, "ALC: %s:%d: %s\n", file, line, s);
	}

	return ret;
}

static alchan_t *
getchan(int ent, int ch)
{
	alchan_t *c, *stopped;
	ALint state;
	ALuint src;

	stopped = nil;
	for(c = chans; c != nil; c = c->next){
		if(c->ent == ent && c->ch == ch){
			alSourceStop(c->src); ALERR();
			return c;
		}
		if(stopped == nil){
			alGetSourcei(c->src, AL_SOURCE_STATE, &state);
			if(!ALERR() && state == AL_STOPPED)
				stopped = c;
		}
	}

	if(stopped != nil){
		c = stopped;
		c->ent = ent;
		c->ch = ch;
		return c;
	}

	alGenSources(1, &src);
	if(ALERR())
		return nil;

	c = calloc(1, sizeof(*c));
	c->ent = ent;
	c->ch = ch;
	c->src = src;
	c->next = chans;
	if(chans != nil)
		chans->prev = c;
	chans = c;

	return c;
}

static void
delchan(alchan_t *c)
{
	alSourceStop(c->src); ALERR();
	alDeleteSources(1, &c->src); ALERR();
	free(c);
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

static alsfx_t *
loadsfx(Sfx *sfx)
{
	alsfx_t *s;
	wavinfo_t info;
	byte *in;
	ALuint buf;
	ALenum fmt;
	ALint loop[2];
	int len;

	if((s = Cache_Check(&sfx->cu)) != nil)
		return s;
	in = loadstklmp(va("sound/%s", sfx->s), nil, 0, &len);
	if(in == nil){
		Con_DPrintf("loadsfx: %s\n", lerr());
		return nil;
	}
	if(wavinfo(in, len, &info) != 0){
		Con_Printf("loadsfx: %s: %s\n", sfx->s, lerr());
		return nil;
	}
	if(info.channels < 2)
		fmt = info.width == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
	else
		fmt = info.width == 1 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
	alGenBuffers(1, &buf);
	if(ALERR())
		return nil;
	alBufferData(buf, fmt, in+info.dataofs, info.samples*info.width, info.rate);
	if(ALERR()){
		alDeleteBuffers(1, &buf); ALERR();
		return nil;
	}
	s = Cache_Alloc(&sfx->cu, sizeof(*s));
	s->buf = buf;
	if(info.loopofs >= 0){
		loop[0] = info.loopofs;
		loop[1] = info.samples;
		alBufferiv(s->buf, AL_LOOP_POINTS_SOFT, loop); ALERR();
		s->loop = true;
	}
	s->upsample = info.rate < 22050;

	return s;
}

void
stepsnd(vec3_t zp, vec3_t fw, vec3_t rt, vec3_t up)
{
	vec_t fwup[6] = {fw[0], fw[1], fw[2], up[0], up[1], up[2]};

	if(dev == nil)
		return;

	if(zp == vec3_origin && fw == vec3_origin && rt == vec3_origin){
		alListenerf(AL_GAIN, 0);
		ALERR();
	}else{
		alListenerfv(AL_POSITION, zp); ALERR();
		alListenerfv(AL_ORIENTATION, fwup); ALERR();
		alListenerf(AL_GAIN, volume.value); ALERR();
	}
}

void
stopallsfx(void)
{
	alchan_t *c, *next;

	if(dev == nil)
		return;
	alListenerf(AL_GAIN, 0); ALERR();
	for(c = chans; c != nil; c = next){
		next = c->next;
		delchan(c);
	}
	chans = nil;
}

void
stopsfx(int ent, int ch)
{
	alchan_t *c;

	if(dev == nil)
		return;
	for(c = chans; c != nil; c = c->next){
		if(c->ent == ent && c->ch == ch)
			break;
	}
	if(c == nil)
		return;
	if(c->prev != nil)
		c->prev->next = c->next;
	if(c->next != nil)
		c->next->prev = c->prev;
	if(chans == c)
		chans = c->next;
	delchan(c);
}

void
startsfx(int ent, int ch, Sfx *sfx, vec3_t zp, float vol, float att)
{
	alchan_t *c;
	alsfx_t *s;
	float x;
	int n;

	if(dev == nil || (s = loadsfx(sfx)) == nil)
		return;
	if((c = getchan(ent, ch)) == nil)
		return;
	x = att * 0.001f;
	if(ent == cl.viewentity){
		alSourcefv(c->src, AL_POSITION, vec3_origin); ALERR();
		alSourcei(c->src, AL_SOURCE_RELATIVE, AL_TRUE); ALERR();
		alSourcef(c->src, AL_ROLLOFF_FACTOR, 0.0f); ALERR();
		alSourcef(c->src, AL_REFERENCE_DISTANCE, 0.0f); ALERR();
	}else{
		alSourcefv(c->src, AL_POSITION, zp); ALERR();
		alSourcei(c->src, AL_SOURCE_RELATIVE, AL_FALSE); ALERR();
		alSourcef(c->src, AL_ROLLOFF_FACTOR, x * (8192.0f - 1.0f)); ALERR();
		alSourcef(c->src, AL_REFERENCE_DISTANCE, 1.0f); ALERR();
		alSourcef(c->src, AL_MAX_DISTANCE, 8192.0f); ALERR();
	}
	alSourcef(c->src, AL_GAIN, vol); ALERR();
	if(al_num_resamplers > 0){
		n = s->upsample ? s_al_resampler_up.value : s_al_resampler_default.value;
		if(n >= 0){
			alSourcei(c->src, AL_SOURCE_RESAMPLER_SOFT, n);
			ALERR();
		}
	}
	alSourcei(c->src, AL_BUFFER, s->buf); ALERR();
	alSourcei(c->src, AL_LOOPING, (s->loop || ent == Srcstatic) ? AL_TRUE : AL_FALSE); ALERR();
	alSourcePlay(c->src); ALERR();
}

void
localsfx(char *name)
{
	if(dev == nil)
		return;

	startsfx(cl.viewentity, -1, findsfx(name), vec3_origin, 1.0f, 1.0f);
}

void
staticsfx(Sfx *sfx, vec3_t zp, float vol, float att)
{
	static int numst = 0;
	alsfx_t *s;

	if(dev == nil || (s = loadsfx(sfx)) == nil)
		return;

	startsfx(Srcstatic, numst++, sfx, zp, vol, att/1.5f);
}

void
touchsfx(char *s)
{
	Sfx *sfx;

	if(dev == nil)
		return;
	sfx = findsfx(s);
	Cache_Check(&sfx->cu);
}

Sfx *
precachesfx(char *s)
{
	Sfx *sfx;

	if(dev == nil)
		return nil;
	sfx = findsfx(s);
	assert(sfx != nil);
	sfx->map = map;
	loadsfx(sfx);
	return sfx;
}

static ALCint *
alcattr(bool hrtf)
{
	static ALCint attr[] = {
		0, 0, 0, 0, 0,
	};

	attr[0] = 0;
	if(hrtf){
		attr[0] = s_al_hrtf.value != 0 ? ALC_HRTF_SOFT : 0;
		attr[1] = s_al_hrtf.value != 0 ? (s_al_hrtf.value > 0 ? ALC_TRUE : ALC_FALSE) : ALC_DONT_CARE_SOFT;
		if(attr[1] == ALC_TRUE){
			attr[2] = ALC_HRTF_ID_SOFT;
			attr[3] = s_al_hrtf.value - 1;
		}
	}

	return attr;
}

static int
alinit(const char *devname)
{
	ALCcontext *c;
	int e;

	dev = alcOpenDevice(devname); ALERR();
	if(dev == nil)
		return -1;

	c = alcCreateContext(dev, nil); ALERR();
	if(c == nil){
closedev:
		alcCloseDevice(dev); ALERR();
		dev = nil;
		return -1;
	}
	ctx = c;
	e = alcMakeContextCurrent(c); ALERR();
	if(!e){
		ctx = nil;
		alcDestroyContext(c); ALERR();
		goto closedev;
	}
	alGenSources(1, &localsrc);
	if(!ALERR()){
		alSourcei(localsrc, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE);
		ALERR();
	}
	alListenerf(AL_GAIN, volume.value); ALERR();
	alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);

	if(alIsExtensionPresent("AL_SOFT_source_resampler")){
		al_default_resampler = alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);
		if(ALERR())
			al_default_resampler = 0;
		al_num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
		if(ALERR())
			al_num_resamplers = 0;
		qk1alGetStringiSOFT = alGetProcAddress("alGetStringiSOFT");
	}
	return 0;
}

static void
alreinit(const char *def, const char *all)
{
	const char *s;
	int i, n;

	n = s_al_dev.value;
	if(n == s_al_dev_prev)
		return;
	if(qk1alcReopenDeviceSOFT == nil && alcIsExtensionPresent(nil, "ALC_SOFT_reopen_device"))
		qk1alcReopenDeviceSOFT = alGetProcAddress("alcReopenDeviceSOFT");
	if(qk1alcReopenDeviceSOFT == nil){
		Con_Printf("AL: can't change device settings on the fly\n");
		return;
	}
	for(i = 1, s = all; s != nil && *s; i++){
		if((n == 0 && def != nil && strcmp(s, def) == 0) || n == i){
			if(dev == nil)
				n = alinit(all);
			else{
				n = qk1alcReopenDeviceSOFT(dev, s, alcattr(false)) ? 0 : -1;
				ALERR();
			}
			if(n != 0)
				Con_Printf("AL: failed to switch to %s\n", s);
			else
				s_al_dev_prev = n;
			return;
		}
		s += strlen(s)+1;
	}
	Con_Printf("AL: no such device: %d\n", n);
}

static void
alvarcb(cvar_t *var)
{
	const char *all, *def, *s;
	bool hrtf;
	int i, n;

	def = alcGetString(nil, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
	if(ALERR())
		def = nil;
	all = alcGetString(nil, ALC_ALL_DEVICES_SPECIFIER);
	if(ALERR())
		all = nil;

	if(var == &s_al_dev && Cmd_Argc() == 1){
		Con_Printf("%-2d: default (%s)\n", 0, def ? def : "<invalid>");
		for(i = 1, s = all; s != nil && *s; i++){
			Con_Printf("%-2d: %s%s\n", i, s, strcmp(s, def) == 0 ? " (default)" : "");
			s += strlen(s)+1;
		}
		return;
	}

	alreinit(def, all);

	if(alcIsExtensionPresent(dev, "ALC_SOFT_HRTF")){
		qk1alcGetStringiSOFT = alcGetProcAddress(dev, "alcGetStringiSOFT");
		qk1alcResetDeviceSOFT = alcGetProcAddress(dev, "alcResetDeviceSOFT");
		hrtf = true;
	}else{
		qk1alcGetStringiSOFT = nil;
		qk1alcResetDeviceSOFT = nil;
		hrtf = false;
	}
	if(var == &s_al_hrtf && Cmd_Argc() == 1){
		Con_Printf("%-2d: disabled\n", -1);
		Con_Printf("%-2d: don't care (default)\n", 0);
		if(qk1alcGetStringiSOFT == nil)
			return;
		alcGetIntegerv(dev, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &n); ALERR();
		for(i = 0; i < n; i++){
			s = qk1alcGetStringiSOFT(dev, ALC_HRTF_SPECIFIER_SOFT, i);
			if(!ALERR() && s != nil)
				Con_Printf("%-2d: %s\n", i+1, s);
		}
		return;
	}

	if(qk1alcResetDeviceSOFT != nil){
		qk1alcResetDeviceSOFT(dev, alcattr(hrtf));
		ALERR();
	}
}

void
sfxbegin(void)
{
}

int
initsnd(void)
{
	s_al_dev.cb = s_al_hrtf.cb = alvarcb;

	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&s_al_dev);
	Cvar_RegisterVariable(&s_al_resampler_default);
	Cvar_RegisterVariable(&s_al_resampler_up);
	Cvar_RegisterVariable(&s_al_hrtf);
	Cmd_AddCommand("stopsound", stopallsfx);

	alinit(nil);
	known_sfx = Hunk_Alloc(MAX_SOUNDS * sizeof *known_sfx);
	num_sfx = 0;

	//ambsfx[Ambwater] = precachesfx("ambience/water1.wav");
	//ambsfx[Ambsky] = precachesfx("ambience/wind2.wav");

	return 0;
}

void
sndclose(void)
{
}
