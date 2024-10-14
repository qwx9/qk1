#include "quakedef.h"
#include "stb_vorbis.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

typedef struct albuf_t albuf_t;
typedef struct alchan_t alchan_t;

struct albuf_t {
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
	Srcamb,
};

cvar_t volume = {"volume", "0.7", true};

static struct {
	stb_vorbis *v;
	int len;
	ALuint src, buf;
	bool stop;
	stb_vorbis_alloc tmp;
}track;

static cvar_t s_al_dev = {"s_al_device", "0", true};

static cvar_t s_al_hrtf = {"s_al_hrtf", "0", true};
static cvar_t s_al_doppler_factor = {"s_al_doppler_factor", "2", true};
static cvar_t s_al_resampler_default = {"s_al_resampler_default", "6", true}; // 23rd order Sinc
static cvar_t s_al_resampler_up = {"s_al_resampler_up", "1", true}; // Linear

static ALCcontext *ctx;
static ALCdevice *dev;
static Sfx *known_sfx;
static int num_sfx;
static int map;
static alchan_t *chans;
static char *curdev = nil;

static cvar_t ambient_level = {"ambient_level", "0.3"};
static cvar_t ambient_fade = {"ambient_fade", "100"};
static Sfx *ambsfx[Namb];
static float ambvol[Namb];

static int al_default_resampler, al_num_resamplers;
static ALchar *(*qalGetStringiSOFT)(ALenum, ALsizei);
static ALCchar *(*qalcGetStringiSOFT)(ALCdevice *, ALenum, ALsizei);
static ALCboolean (*qalcResetDeviceSOFT)(ALCdevice *, const ALCint *attr);
typedef ALsizei (*qalBufferCallbackTypeSOFT)(ALvoid *, ALvoid *, ALsizei);
static void (*qalBufferCallbackSOFT)(ALuint buf, ALenum fmt, ALsizei freq, qalBufferCallbackTypeSOFT cb, ALvoid *aux);

#define ALERR() alcheckerr(__FILE__, __LINE__)

static char *
vorbiserr(int err)
{
	switch(err){
	case VORBIS_invalid_api_mixing: return "can't mix api modes";
	case VORBIS_outofmem: return "oom";
	case VORBIS_feature_not_supported: return "feature not supported";
	case VORBIS_too_many_channels: return "too many channels";
	case VORBIS_unexpected_eof: return "unexpected EOF";
	case VORBIS_seek_invalid: return "invalid seek";
	case VORBIS_invalid_setup: return "invalid setup";
	case VORBIS_invalid_stream: return "invalid stream";
	case VORBIS_missing_capture_pattern: return "missing capture pattern";
	case VORBIS_invalid_stream_structure_version: return "invalid stream struct verion";
	case VORBIS_continued_packet_flag_invalid: return "continued packet flag invalid";
	case VORBIS_incorrect_stream_serial_number: return "incorrect stream serial number";
	case VORBIS_invalid_first_page: return "invalid first page";
	case VORBIS_bad_packet_type: return "bad packet type";
	case VORBIS_cant_find_last_page: return "can't find last page";
	case VORBIS_seek_failed: return "seek failed";
	case VORBIS_ogg_skeleton_not_supported: return "ogg skeleton not supported";
	default: return "???";
	}
}

static ALsizei
trackcb(ALvoid *aux, ALvoid *sampledata, ALsizei numbytes)
{
	ssize_t n;
	byte *b;
	bool eof;
	int err;

	USED(aux);
	eof = false;
	for(b = sampledata; numbytes > 0; b += n, numbytes -= n){
		n = 2 * sizeof(float) * stb_vorbis_get_samples_float_interleaved(track.v, 2, (float*)b, numbytes/sizeof(float));
		if(n <= 0){
			if(n < 0 || eof || !cdloop){
				if((err = stb_vorbis_get_error(track.v)) != 0)
					Con_Printf("track%02d: %s\n", cdntrk, vorbiserr(err));
				track.stop = true;
				break;
			}
			eof = true;
			n = 0;
			stb_vorbis_seek_start(track.v);
		}
	}

	return b - (byte*)sampledata;
}

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
		if(c->ent == ent && c->ch == ch)
			return c;
		if(stopped == nil){
			alGetSourcei(c->src, AL_SOURCE_STATE, &state);
			if(!ALERR() && state != AL_PLAYING)
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
	if(c->prev != nil)
		c->prev->next = c->next;
	if(c->next != nil)
		c->next->prev = c->prev;
	if(chans == c)
		chans = c->next;
	alSourceStop(c->src); ALERR();
	alSourcei(c->src, AL_BUFFER, 0); ALERR();
	alDeleteSources(1, &c->src); ALERR();
	free(c);
}

static Sfx *
findsfx(char *s)
{
	Sfx *sfx, *e;
	albuf_t *b;

	if(strlen(s) >= Npath)
		Host_Error("findsfx: path too long %s", s);
	sfx = known_sfx;
	e = known_sfx + num_sfx;
	while(sfx < e){
		if(strcmp(sfx->s, s) == 0){
			sfx->map = map;
			return sfx;
		}
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
		if((b = Cache_Check(&sfx->cu)) != nil){
			alDeleteBuffers(1, &b->buf); ALERR();
			Cache_Free(&sfx->cu);
			sfx->map = map;
		}
	}else
		num_sfx++;
	strcpy(sfx->s, s);
	return sfx;
}

static albuf_t *
loadsfx(Sfx *sfx)
{
	ALint loop[2];
	wavinfo_t info;
	ALuint buf;
	ALenum fmt;
	albuf_t *b;
	byte *in;
	int len;

	if((b = Cache_Check(&sfx->cu)) != nil)
		return b;
	in = loadstklmp(va("sound/%s", sfx->s), nil, 0, &len);
	if(in == nil){
		Con_DPrintf("loadsfx: %s\n", lerr());
		return nil;
	}
	if(wavinfo(in, len, &info) != 0){
		Con_Printf("loadsfx: %s: %s\n", sfx->s, lerr());
		// complain but get some silence in place so it looks like it got loaded
		memset(&info, 0, sizeof(info));
		info.width = 8;
		info.channels = 1;
		info.rate = 11025;
		info.loopofs = -1;
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
	b = Cache_Alloc(&sfx->cu, sizeof(*b));
	b->buf = buf;
	if(info.loopofs >= 0){
		loop[0] = info.loopofs;
		loop[1] = info.samples;
		alBufferiv(b->buf, AL_LOOP_POINTS_SOFT, loop); ALERR();
		b->loop = true;
	}
	b->upsample = info.rate < 22050;

	return b;
}

static void
alplay(alchan_t *c, albuf_t *b, vec3_t zp, float vol, float att, bool rel, bool loop)
{
	ALint src;
	int n;

	src = c->src;
	if(rel){
		alSourcefv(src, AL_POSITION, vec3_origin); ALERR();
		alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE); ALERR();
		alSourcef(src, AL_ROLLOFF_FACTOR, 0.0f); ALERR();
		alSourcef(src, AL_REFERENCE_DISTANCE, 0.0f); ALERR();
	}else{
		alSourcefv(src, AL_POSITION, zp); ALERR();
		alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE); ALERR();
		alSourcef(src, AL_ROLLOFF_FACTOR, att * 8.191); ALERR();
		alSourcef(src, AL_REFERENCE_DISTANCE, 1.0f); ALERR();
		alSourcef(src, AL_MAX_DISTANCE, 8192.0f); ALERR();
	}
	alSourcef(src, AL_GAIN, vol); ALERR();
	if(al_num_resamplers > 0){
		n = b->upsample ? s_al_resampler_up.value : s_al_resampler_default.value;
		if(n >= 0){
			alSourcei(src, AL_SOURCE_RESAMPLER_SOFT, n);
			ALERR();
		}
	}
	alSourcei(src, AL_BUFFER, b->buf); ALERR();
	alSourcei(src, AL_LOOPING, (b->loop || loop) ? AL_TRUE : AL_FALSE); ALERR();
	alSourcePlay(src); ALERR();
}


static void
ambs(const vec3_t org)
{
	float vol, *av;
	ALint state;
	alchan_t *ch;
	byte *asl;
	mleaf_t *l;
	albuf_t *b;
	Sfx *sfx;
	int i;

	if(cl.worldmodel == nil)
		return;
	l = Mod_PointInLeaf(org, cl.worldmodel);
	asl = l->ambient_sound_level;
	for(i = 0; i < Namb; i++){
		if((sfx = ambsfx[i]) == nil || (b = loadsfx(sfx)) == nil)
			continue;
		vol = ambient_level.value * asl[i];
		av = &ambvol[i];
		if(vol < 8)
			vol = 0;
		if(*av < vol){
			*av += host_frametime * ambient_fade.value;
			if(*av > vol)
				*av = vol;
		}else if(*av > vol){
			*av -= host_frametime * ambient_fade.value;
			if(*av < vol)
				*av = vol;
		}
		if((ch = getchan(Srcamb, i)) != nil){
			alSourcef(ch->src, AL_GAIN, *av / 255.0f); ALERR();
			alGetSourcei(ch->src, AL_SOURCE_STATE, &state);
			if(!ALERR() && state != AL_PLAYING)
				alplay(ch, b, vec3_origin, *av, 0.0f, true, true);
		}
	}
}

void
stepsnd(const view_t *v)
{
	vec_t fwup[6] = {v->pn[0], v->pn[1], v->pn[2], v->up[0], v->up[1], v->up[2]};
	alchan_t *c, *next;
	static vec3_t oorg;
	ALint state;
	vec3_t vel;

	if(dev == nil)
		return;
	if(v->pn[0] == 0 && v->pn[1] == 0 && v->pn[2] == 0){
		alListenerf(AL_GAIN, 0);
		ALERR();
		return;
	}

	alListenerfv(AL_POSITION, v->org); ALERR();
	alListenerfv(AL_ORIENTATION, fwup); ALERR();
	VectorSubtract(v->org, oorg, vel);
	VectorCopy(v->org, oorg);
	alListenerfv(AL_VELOCITY, vel); ALERR();
	alListenerf(AL_GAIN, volume.value); ALERR();

	ambs(v->org);

	for(c = chans; c != nil; c = next){
		next = c->next;
		alGetSourcei(c->src, AL_SOURCE_STATE, &state);
		if(!ALERR() && state != AL_PLAYING)
			delchan(c);
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
	memset(ambvol, 0, sizeof(ambvol));
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
	if(ent == Srcamb)
		ambvol[ch] = 0;
}

void
startsfx(int ent, int ch, Sfx *sfx, vec3_t zp, float vol, float att)
{
	alchan_t *c;
	albuf_t *b;

	if(dev == nil || (b = loadsfx(sfx)) == nil || (c = getchan(ent, ch)) == nil)
		return;
	alSourceStop(c->src); ALERR();
	alplay(c, b, zp, vol, att, ent == cl.viewentity, ent == Srcstatic);
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

	if(dev == nil)
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
	sfx->map = map;
	loadsfx(sfx);
	return sfx;
}

static ALCint *
alcattr(bool hrtf)
{
	static ALCint attr[16] = {0};
	int i;

	attr[i = 0] = 0;
	if(hrtf){
		attr[i++] = s_al_hrtf.value != 0 ? ALC_HRTF_SOFT : 0;
		attr[i++] = s_al_hrtf.value != 0 ? (s_al_hrtf.value > 0 ? ALC_TRUE : ALC_FALSE) : ALC_DONT_CARE_SOFT;
		if(attr[i-1] == ALC_TRUE){
			attr[i++] = ALC_HRTF_ID_SOFT;
			attr[i++] = s_al_hrtf.value - 1;
		}
	}
	// get more sources, those get depleated fast with AD
	attr[i++] = ALC_MONO_SOURCES;
	attr[i++] = 512;
	attr[i++] = 0;

	return attr;
}

static int
alinit(const char *devname)
{
	ALCcontext *c;
	int e;

	if(devname == nil){
		// ALC_DEFAULT_DEVICE_SPECIFIER would make more sense,
		// but unfortunately it's not a _full_ name
		devname = alcGetString(nil, ALC_ALL_DEVICES_SPECIFIER);
		ALERR();
	}

	dev = alcOpenDevice(devname); ALERR();
	if(dev == nil)
		return -1;
	free(curdev);
	curdev = strdup(devname);

	c = alcCreateContext(dev, nil); ALERR();
	if(c == nil){
closedev:
		alcCloseDevice(dev); ALERR();
		dev = nil;
		free(curdev);
		curdev = nil;
		return -1;
	}
	ctx = c;
	e = alcMakeContextCurrent(c); ALERR();
	if(!e){
		ctx = nil;
		alcDestroyContext(c); ALERR();
		goto closedev;
	}
	alListenerf(AL_GAIN, volume.value); ALERR();
	alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED); ALERR();

	// assuming 64 Quake units is ~1.7m
	alSpeedOfSound(343.3 * 64.0 / 1.7); ALERR();
	alDopplerFactor(s_al_doppler_factor.value); ALERR();

	if(alIsExtensionPresent("AL_SOFT_source_resampler")){
		al_default_resampler = alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);
		if(ALERR())
			al_default_resampler = 0;
		al_num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
		if(ALERR())
			al_num_resamplers = 0;
		qalGetStringiSOFT = alGetProcAddress("alGetStringiSOFT");
	}else{
		qalGetStringiSOFT = nil;
	}

	qalBufferCallbackSOFT = nil;
	cdenabled = cdenabled &&
		alIsExtensionPresent("AL_EXT_float32") &&
		alIsExtensionPresent("AL_SOFT_callback_buffer") &&
		(alGenSources(1, &track.src), !ALERR());
	if(cdenabled){
		alSourcei(track.src, AL_SOURCE_SPATIALIZE_SOFT, AL_FALSE); ALERR();
		qalBufferCallbackSOFT = alGetProcAddress("alBufferCallbackSOFT"); ALERR();
		alGenBuffers(1, &track.buf); ALERR();
		// test run to make sure it works
		qalBufferCallbackSOFT(track.buf, AL_FORMAT_STEREO_FLOAT32, 44100, trackcb, nil);
		if(ALERR()){
fail:
			alDeleteBuffers(1, &track.buf); ALERR();
			track.buf = 0;
			cdenabled = false;
		}
		alSourcei(track.src, AL_BUFFER, track.buf);
		if(ALERR())
			goto fail;
	}

	return 0;
}

static int
alreopen(const char *new, ALCint *attr)
{
	ALCboolean *(*qalcReopenDeviceSOFT)(
		ALCdevice *,
		const ALCchar *devname,
		const ALCint *attr
	);
	int r;

	qalcReopenDeviceSOFT = nil;
	if(alcIsExtensionPresent(dev, "ALC_SOFT_reopen_device"))
		qalcReopenDeviceSOFT = alcGetProcAddress(dev, "alcReopenDeviceSOFT");
	if(qalcReopenDeviceSOFT == nil){
		Con_Printf("AL: can't change device settings on the fly\n");
		return -1;
	}
	r = qalcReopenDeviceSOFT(dev, new, attr) ? 0 : -1;
	ALERR();
	return r;
}

static void
alreinit(const char *def, const char *all)
{
	const char *newdev;
	int i, n;
	bool hrtf;

	n = s_al_dev.value;
	for(i = 1, newdev = all; newdev != nil && *newdev; i++){
		if((n == 0 && def != nil && strcmp(newdev, def) == 0) || n == i){
			if(curdev != nil && strcmp(newdev, curdev) == 0)
				break;
			n = dev == nil ? alinit(all) : alreopen(newdev, alcattr(false));
			if(n != 0){
				Con_Printf("AL: failed to switch to %s\n", newdev);
				return;
			}
			free(curdev);
			curdev = strdup(newdev);
			break;
		}
		newdev += strlen(newdev)+1;
	}
	if(newdev == nil || !*newdev){
		Con_Printf("AL: no such device: %d\n", n);
		return;
	}

	if(alcIsExtensionPresent(dev, "ALC_SOFT_HRTF")){
		qalcGetStringiSOFT = alcGetProcAddress(dev, "alcGetStringiSOFT");
		qalcResetDeviceSOFT = alcGetProcAddress(dev, "alcResetDeviceSOFT");
		hrtf = true;
	}else{
		qalcGetStringiSOFT = nil;
		qalcResetDeviceSOFT = nil;
		hrtf = false;
	}

	if(qalcResetDeviceSOFT != nil){
		qalcResetDeviceSOFT(dev, alcattr(hrtf));
		ALERR();
	}
}

static void
alvarcb(cvar_t *var)
{
	const char *all, *def, *s;
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
			Con_Printf(
				"%-2d: %s%s\n",
				i, s, (def != nil && strcmp(s, def) == 0) ? " (default)" : ""
			);
			s += strlen(s)+1;
		}
		return;
	}

	alreinit(def, all);

	if(var == &s_al_hrtf && Cmd_Argc() == 1){
		Con_Printf("%-2d: disabled\n", -1);
		Con_Printf("%-2d: don't care (default)\n", 0);
		if(qalcGetStringiSOFT == nil)
			return;
		alcGetIntegerv(dev, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &n); ALERR();
		for(i = 0; i < n; i++){
			s = qalcGetStringiSOFT(dev, ALC_HRTF_SPECIFIER_SOFT, i);
			if(!ALERR() && s != nil)
				Con_Printf("%-2d: %s\n", i+1, s);
		}
	}
}

static void
aldopplercb(cvar_t *var)
{
	alDopplerFactor(var->value); ALERR();
}

static void
sfxlist(void)
{
	int sz, sum, w, ch;
	Sfx *sfx, *e;
	albuf_t *b;

	sum = 0;
	for(sfx = known_sfx, e = known_sfx+num_sfx; sfx < e; sfx++){
		if((b = Cache_Check(&sfx->cu)) == nil)
			continue;
		alGetBufferi(b->buf, AL_SIZE, &sz); ALERR();
		alGetBufferi(b->buf, AL_CHANNELS, &ch); ALERR();
		alGetBufferi(b->buf, AL_BITS, &w); ALERR();
		sum += sz * ch * w/8;
		Con_Printf("%c(%2db) %6d : %s\n", b->loop ? 'L' : ' ', w, sz, sfx->s);
	}
	Con_Printf("Total resident: %d\n", sum);
}

void
sfxbegin(void)
{
	map++;
}

void
sfxend(void)
{
	albuf_t *b;
	Sfx *sfx;
	int i;

	if(dev == nil)
		return;

	ambsfx[Ambwater] = precachesfx("ambience/water1.wav");
	ambsfx[Ambsky] = precachesfx("ambience/wind2.wav");
	for(i = 0, sfx = known_sfx; i < num_sfx; i++, sfx++){
		if(sfx->map >= map || sfx == ambsfx[Ambsky] || sfx == ambsfx[Ambwater])
			continue;
		if((b = Cache_Check(&sfx->cu)) != nil){
			alDeleteBuffers(1, &b->buf); ALERR();
			Cache_Free(&sfx->cu);
		}
	}
}

void
stepcd(void)
{
	if(dev == nil || !cdenabled)
		return;

	if(track.stop)
		stopcd();
	else if(track.v != nil){
		alSourcef(track.src, AL_GAIN, bgmvolume.value);
		ALERR();
	}
}

void
playcd(int nt, bool loop)
{
	stb_vorbis_info info;
	FILE *f;
	int err;

	if(dev == nil || !cdenabled)
		return;

	stopcd();
	if((f = openlmp(va("music/track%02d.ogg", nt), &track.len)) == nil)
		return;
	if(track.tmp.alloc_buffer == nil){
		track.tmp.alloc_buffer_length_in_bytes = 512*1024;
		track.tmp.alloc_buffer = malloc(track.tmp.alloc_buffer_length_in_bytes);
	}
	track.v = stb_vorbis_open_file_section(f, true, &err, &track.tmp, track.len);
	if(track.v == nil){
		werrstr("track%02d: %s", nt, vorbiserr(err));
		return;
	}
	info = stb_vorbis_get_info(track.v);
	cdloop = loop;
	cdtrk = nt;
	track.stop = false;
	alSourcei(track.src, AL_BUFFER, 0);
	qalBufferCallbackSOFT(track.buf, AL_FORMAT_STEREO_FLOAT32, info.sample_rate, trackcb, nil);
	if(ALERR() || (alSourcei(track.src, AL_BUFFER, track.buf), ALERR()) || (alSourcePlay(track.src), ALERR())){
		stb_vorbis_close(track.v);
		track.v = nil;
	}
}

void
resumecd(void)
{
	if(track.v != nil){
		alSourcePlay(track.src);
		ALERR();
	}
}

void
pausecd(void)
{
	if(track.v != nil){
		alSourcePause(track.src);
		ALERR();
	}
}

int
initcd(void)
{
	cdntrk = cdtrk = 0;
	cdloop = false;
	return 0;
}

void
stopcd(void)
{
	if(track.v != nil){
		alSourceStop(track.src); ALERR();
		stb_vorbis_close(track.v);
		track.v = nil;
		free(track.tmp.alloc_buffer);
		track.tmp.alloc_buffer = nil;
	}
	track.stop = false;
}

void
shutcd(void)
{
	stopcd();
}

int
initsnd(void)
{
	Cvar_RegisterVariable(&volume);
	Cvar_RegisterVariable(&bgmvolume);
	Cvar_RegisterVariable(&ambient_level);
	Cvar_RegisterVariable(&ambient_fade);
	Cvar_RegisterVariable(&s_al_dev);
	Cvar_RegisterVariable(&s_al_resampler_default);
	Cvar_RegisterVariable(&s_al_resampler_up);
	Cvar_RegisterVariable(&s_al_hrtf);
	Cvar_RegisterVariable(&s_al_doppler_factor);
	Cmd_AddCommand("stopsound", stopallsfx);
	Cmd_AddCommand("soundlist", sfxlist);
	Cmd_AddCommand("cd", cdcmd);

	if(!isdisabled("snd")){
		cdenabled = !isdisabled("cd");
		s_al_dev.cb = s_al_hrtf.cb = alvarcb;
		s_al_doppler_factor.cb = aldopplercb;
		alinit(nil);
		known_sfx = Hunk_Alloc(MAX_SOUNDS * sizeof *known_sfx);
		num_sfx = 0;
	}

	return 0;
}

void
sndclose(void)
{
	if(dev == nil)
		return;
	alcDestroyContext(ctx); ctx = nil; ALERR();
	alcCloseDevice(dev); dev = nil; ALERR();
}
