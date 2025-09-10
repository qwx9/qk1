#include "quakedef.h"
#include "colormatrix.h"
#include <SDL3/SDL.h>

SDL_Window *win;
static SDL_Renderer *rend;
static SDL_Texture *fbi;
static pixel_t *vidbuffer;
extern pixel_t *r_warpbuffer;

static cvar_t v_fullscreen = {"v_fullscreen", "0", true};
static cvar_t v_sync = {"v_sync", "1", true};

bool doublebuffer = false;

static int
curwinmode(void)
{
	Uint32 fl;

	fl = (win != nil ? SDL_GetWindowFlags(win) : 0) & SDL_WINDOW_FULLSCREEN;
	if(fl == SDL_WINDOW_FULLSCREEN)
		return 1;

	return 0;
}

static int
cvarwinflags(void)
{
	if(v_fullscreen.value >= 1)
		return SDL_WINDOW_FULLSCREEN;
	return SDL_WINDOW_RESIZABLE;
}

static void
resetfb(void)
{
	void *surfcache;
	int hunkvbuf, scachesz, n;
	float r;

	setcvar(v_fullscreen.name, va("%d", curwinmode()));

	SDL_GetCurrentRenderOutputSize(rend, &vid.width, &vid.height);
	vid.width /= v_scale.value;
	vid.height /= v_scale.value;
	if(vid.width > MAXDIMENSION) {
		r = (float)vid.width / (float)MAXDIMENSION;
		vid.height /= r;
		vid.width = MAXDIMENSION;
	}
	if(vid.height > MAXDIMENSION) {
		r = (float)vid.height / (float)MAXDIMENSION;
		vid.width /= r;
		vid.height = MAXDIMENSION;
	}
	vid.width = max(vid.width, MINWIDTH);
	vid.height = max(vid.height, MINHEIGHT);

	vid.aspect = (float)(vid.height * MINWIDTH) / (float)(vid.width * MINHEIGHT);
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	n = (vid.width * vid.height + 16) * sizeof(pixel_t);
	vidbuffer = realloc(vidbuffer, n);
	r_warpbuffer = realloc(r_warpbuffer, n);
	vid.maxwarpwidth = vid.width;
	vid.maxwarpheight = vid.height;

	if(fbi != nil)
		SDL_DestroyTexture(fbi);
	fbi = SDL_CreateTexture(
		rend, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STATIC, vid.width, vid.height
	);
	if(fbi == NULL)
		fatal("SDL_CreateTexture: %s", SDL_GetError());
	SDL_SetTextureBlendMode(fbi, SDL_BLENDMODE_NONE);
	SDL_SetTextureScaleMode(fbi, SDL_SCALEMODE_NEAREST);
	SDL_RenderClear(rend);

	vid.buffer = vidbuffer;
	vid.conbuffer = vid.buffer;

	if(dvars.zb != nil)
		D_FlushCaches();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof(*dvars.zb);
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	dvars.zb = realloc(dvars.zb, hunkvbuf);
	memset(dvars.zb, 0, hunkvbuf);
	surfcache = (byte *)(dvars.zb + vid.width * vid.height);
	D_InitCaches(surfcache, scachesz);
	vid.resized = false;
}

void
stopfb(void)
{
}

void
flipfb(void)
{
	if(vid.resized) { /* skip this frame if window resize */
		vid.resized = false;
		resetfb();
		vid.recalc_refdef = true; /* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f(nil);
		return;
	}

	cmprocess(cm, vidbuffer, vid.width * vid.height);

	SDL_UpdateTexture(fbi, nil, vidbuffer, vid.width * 4);
	SDL_RenderTexture(rend, fbi, nil, nil);
	SDL_RenderPresent(rend);
}

void
setpal(const byte *p0)
{
	pixel_t x;
	const byte *p;

	for(p = p0, x = 0; x < 256; x++, p += 3) {
		q1pal[x] = (x < 256U - 32U ? 0xffU : 0) << 24 | p[0] << 16 | p[1] << 8 | p[2];
		q1palindexed[x] = opaque(q1pal[x]) ? (x << 24 | p[0] << 16 | p[1] << 8 | p[2]) : 0;
	}
	q1pal[255] = 0;
	q1palindexed[255] = 0;

	scr_fullupdate = 0;
}

static void
v_fullscreen_cb(cvar_t *var)
{
	int oldmode, mode;

	oldmode = curwinmode();
	mode = var->value;
	if(oldmode != mode && SDL_SetWindowFullscreen(win, mode) == 0)
		vid.resized = true;
}

static void
hints(void)
{
	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,cocoa,x11,windows,kmsdrm,kmsdrmvulkan,rpi");
	SDL_SetHint(SDL_HINT_WINDOWS_RAW_KEYBOARD, "1");
	SDL_SetHint(SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS, "1");
	SDL_SetHint(SDL_HINT_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN, "0");
	SDL_SetHint(SDL_HINT_RENDER_GPU_LOW_POWER, "1");
	SDL_SetHint(SDL_HINT_RENDER_METAL_PREFER_LOW_POWER_DEVICE, "1");
	SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, doublebuffer ? "1" : "0");
	SDL_SetHint(SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, "0");
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_PING, "0");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, v_sync.value ? "1" : "0");
}

static void
makewindow(void)
{
	int x, y, w, h;

	if(win != nil) {
		SDL_GetWindowPosition(win, &x, &y);
		SDL_GetWindowSize(win, &w, &h);
	} else {
		x = SDL_WINDOWPOS_CENTERED;
		y = SDL_WINDOWPOS_CENTERED;
		w = MINWIDTH;
		h = MINHEIGHT;
	}

	if(fbi != nil)
		SDL_DestroyTexture(fbi);
	if(rend != nil)
		SDL_DestroyRenderer(rend);
	if(win != nil)
		SDL_DestroyWindow(win);

	hints();
	if(!SDL_CreateWindowAndRenderer("Quake (qk1)", w, h, cvarwinflags(), &win, &rend))
		fatal("SDL_CreateWindow: %s", SDL_GetError());
	SDL_SetWindowResizable(win, true);
	SDL_SetWindowMinimumSize(win, MINWIDTH, MINHEIGHT);
	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	SDL_RenderClear(rend);
	SDL_RenderPresent(rend);
	vid.resized = true;
}

static void
v_sync_cb(cvar_t *var)
{
	static int vsync = -1;

	if(vsync == (var->value != 0))
		return;
	vsync = var->value != 0;
	makewindow();
}

char *
sys_wrpath(void)
{
	// it's a bit awkward to use SDL for this but oh well
	return SDL_GetPrefPath(nil, "qk1");
}

void
initfb(void)
{
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "Quake (qk1)");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, va("%.2f", VERSION));
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");
	SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, "https://sr.ht/~ft/qk1");

	vid.numpages = 2;
	vid.colormap = malloc(256 * 64 * sizeof(pixel_t));
	torgbx(host_colormap, vid.colormap, 256 * 64);
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

	hints();
	if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		fatal("SDL_Init: %s", SDL_GetError());
	makewindow();
	IN_Grabm(true);
	resetfb();

	Cvar_RegisterVariable(&v_fullscreen)->cb = v_fullscreen_cb;
	Cvar_RegisterVariable(&v_sync)->cb = v_sync_cb;
}
