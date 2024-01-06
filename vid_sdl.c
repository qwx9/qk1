#include "quakedef.h"
#include <SDL.h>
#include <sys/mman.h>

pixel_t q1pal[256];

static SDL_Renderer *rend;
static SDL_Texture *fbi;
static SDL_Window *win;
static pixel_t *vidbuffer;
extern pixel_t *r_warpbuffer;

static cvar_t v_snail = {"v_snail", "0"};
static cvar_t v_fullscreen = {"v_fullscreen", "0", true};
static int oldvidbuffersz;

static void
resetfb(void)
{
	void *surfcache;
	int hunkvbuf, scachesz, n;

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
	if(snailenabled){
		if(oldvidbuffersz != 0)
			munmap(vidbuffer, oldvidbuffersz);
		vidbuffer = mmap(nil, n, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		oldvidbuffersz = n;
	}else{
		free(vidbuffer);
		vidbuffer = emalloc(n);
	}
	free(r_warpbuffer);
	r_warpbuffer = emalloc(n);
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

	if(dvars.zbuffer != nil)
		D_FlushCaches();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof(*dvars.zbuffer);
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	dvars.zbuffer = realloc(dvars.zbuffer, hunkvbuf);
	memset(dvars.zbuffer, 0, hunkvbuf);
	surfcache = (byte *)(dvars.zbuffer + vid.width * vid.height);
	D_InitCaches(surfcache, scachesz);
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
v_snail_cb(cvar_t *var)
{
	sys_snail(var->value != 0);
}

static void
v_fullscreen_cb(cvar_t *var)
{
	static int oldmode = 0;
	int mode;

	if(var->value >= 2)
		mode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	else if(var->value >= 1)
		mode = SDL_WINDOW_FULLSCREEN;
	else
		mode = 0;
	if(oldmode != mode && SDL_SetWindowFullscreen(win, mode) == 0){
		vid.resized = true;
		oldmode = mode;
	}
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
	win = SDL_CreateWindow("quake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
	if(win == nil)
		fatal("SDL_CreateWindow: %s", SDL_GetError());
	if((rend = SDL_CreateRenderer(win, -1, 0)) == NULL)
		fatal("SDL_CreateRenderer: %s", SDL_GetError());
	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	SDL_RenderClear(rend);
	SDL_RenderPresent(rend);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	resetfb();
	SDL_SetWindowResizable(win, SDL_TRUE);
	SDL_SetWindowMinimumSize(win, 320, 240);
	IN_Grabm(1);

	Cvar_RegisterVariable(&v_snail);
	v_snail.cb = v_snail_cb;
	Cvar_RegisterVariable(&v_fullscreen);
	v_fullscreen.cb = v_fullscreen_cb;
}
