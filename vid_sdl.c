#include "quakedef.h"
#include "colormatrix.h"
#include <SDL.h>

pixel_t q1pal[256];

static SDL_Renderer *rend;
static SDL_Texture *fbi;
static SDL_Window *win;
static pixel_t *vidbuffer;
extern pixel_t *r_warpbuffer;

static cvar_t v_fullscreen = {"v_fullscreen", "0", true};
static cvar_t v_sync = {"v_sync", "1", true};

static int
curwinmode(void)
{
	Uint32 fl;

	fl = (win != nil ? SDL_GetWindowFlags(win) : 0) & (SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_FULLSCREEN);
	if(fl == SDL_WINDOW_FULLSCREEN_DESKTOP)
		return 2;
	if(fl == SDL_WINDOW_FULLSCREEN)
		return 1;

	return 0;
}

static int
cvarwinflags(void)
{
	if(v_fullscreen.value >= 2)
		return SDL_WINDOW_FULLSCREEN_DESKTOP;
	if(v_fullscreen.value >= 1)
		return SDL_WINDOW_FULLSCREEN;
	return 0;
}

static void
resetfb(void)
{
	void *surfcache;
	int hunkvbuf, scachesz, n;

	setcvar(v_fullscreen.name, va("%d", curwinmode()));

	/* lower than 320x240 doesn't really make sense,
	 * but at least this prevents a crash, beyond that
	 * it's your funeral */
	SDL_GetRendererOutputSize(rend, &vid.width, &vid.height);
	vid.width /= v_scale.value;
	vid.height /= v_scale.value;
	vid.width = clamp(vid.width, 320, MAXWIDTH);
	vid.height = clamp(vid.height, 240, MAXHEIGHT);

	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	n = (vid.width*vid.height+16)*sizeof(pixel_t);
	vidbuffer = realloc(vidbuffer, n);
	r_warpbuffer = realloc(r_warpbuffer, n);
	vid.maxwarpwidth = vid.width;
	vid.maxwarpheight = vid.height;

	if(fbi != nil)
		SDL_DestroyTexture(fbi);
	fbi = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STATIC, vid.width, vid.height);
	if(fbi == NULL)
		fatal("SDL_CreateTexture: %s", SDL_GetError());
	SDL_SetTextureBlendMode(fbi, SDL_BLENDMODE_NONE);
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
	if(vid.resized){		/* skip this frame if window resize */
		vid.resized = false;
		resetfb();
		vid.recalc_refdef = true;	/* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	cmprocess(cm, vidbuffer, vidbuffer, vid.width*vid.height);

	SDL_UpdateTexture(fbi, nil, vidbuffer, vid.width*4);
	SDL_RenderCopy(rend, fbi, nil, nil);
	SDL_RenderPresent(rend);
}

void
setpal(byte *p0)
{
	int x;
	byte *p;

	for(p = p0, x = 0; x < 256; x++, p += 3)
		q1pal[x] = (x < 256-32 ? 0xff : 0)<<24 | p[0]<<16 | p[1]<<8 | p[2];
	q1pal[255] &= 0;

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
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	SDL_SetHint(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1");
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_PING, "0");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, v_sync.value ? "1" : "0");
}

static void
makewindow(void)
{
	int x, y, w, h;

	if(win != nil){
		SDL_GetWindowPosition(win, &x, &y);
		SDL_GetWindowSize(win, &w, &h);
	}else{
		x = SDL_WINDOWPOS_CENTERED;
		y = SDL_WINDOWPOS_CENTERED;
		w = 800;
		h = 600;
	}

	if(fbi != nil)
		SDL_DestroyTexture(fbi);
	if(rend != nil)
		SDL_DestroyRenderer(rend);
	if(win != nil)
		SDL_DestroyWindow(win);

	hints();
	win = SDL_CreateWindow("quake", x, y, w, h, cvarwinflags() | SDL_WINDOW_RESIZABLE);
	if(win == nil)
		fatal("SDL_CreateWindow: %s", SDL_GetError());
	SDL_SetWindowResizable(win, SDL_TRUE);
	SDL_SetWindowMinimumSize(win, 320, 240);
	rend = SDL_CreateRenderer(win, -1,
		(v_sync.value ? SDL_RENDERER_PRESENTVSYNC : 0)
	);
	if(rend == nil)
		fatal("SDL_CreateRenderer: %s", SDL_GetError());
	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	SDL_RenderClear(rend);
	SDL_RenderPresent(rend);
	vid.resized = true;
}

static void
v_sync_cb(cvar_t *var)
{
	static int vsync;

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
	vid.numpages = 2;
	vid.colormap = malloc(256*64*sizeof(pixel_t));
	torgbx(host_colormap, vid.colormap, 256*64);
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
		fatal("SDL_Init: %s", SDL_GetError());
	makewindow();
	resetfb();
	IN_Grabm(1);

	Cvar_RegisterVariable(&v_fullscreen);
	v_fullscreen.cb = v_fullscreen_cb;
	Cvar_RegisterVariable(&v_sync);
	v_sync.cb = v_sync_cb;
}
