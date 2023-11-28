#include "quakedef.h"
#include <SDL.h>

int resized;

static SDL_Renderer *rend;
static SDL_Texture *fbi;
static SDL_Window *win;
static pixel_t *vidbuffer;

s32int fbpal[256];
pixel_t q1pal[256];

static void
resetfb(void)
{
	void *surfcache;
	int hunkvbuf, scachesz;

	/* lower than 320x240 doesn't really make sense,
	 * but at least this prevents a crash, beyond that
	 * it's your funeral */
	SDL_GetWindowSize(win, &vid.width, &vid.height);
	if(vid.width < 320)
		vid.width = 320;
	if(vid.height < 160)
		vid.height = 160;

	vid.rowbytes = vid.width;
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	free(vidbuffer);
	vidbuffer = emalloc((vid.width*vid.height+16)*4);

	if(fbi != nil)
		SDL_DestroyTexture(fbi);
	fbi = SDL_CreateTexture(rend, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, vid.width, vid.height);
	if(fbi == NULL)
		fatal("SDL_CreateTexture: %s", SDL_GetError());
	SDL_SetTextureBlendMode(fbi, SDL_BLENDMODE_NONE);
	SDL_RenderClear(rend);

	vid.buffer = vidbuffer;
	vid.conbuffer = vid.buffer;

	if(d_pzbuffer != nil){
		D_FlushCaches();
		free(d_pzbuffer);
		d_pzbuffer = nil;
	}

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof *d_pzbuffer;
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	d_pzbuffer = emalloc(hunkvbuf);
	surfcache = (byte *)(d_pzbuffer + vid.width * vid.height);
	D_InitCaches(surfcache, scachesz);
}

void
stopfb(void)
{
}

void
flipfb(void)
{
	int pitch;
	void *p;

	if(resized){		/* skip this frame if window resize */
		stopfb();
		resized = 0;
		resetfb();
		vid.recalc_refdef = 1;	/* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	SDL_LockTexture(fbi, NULL, &p, &pitch);
	memmove(p, vidbuffer, vid.width*vid.height*4);
	SDL_UnlockTexture(fbi);
	SDL_RenderCopy(rend, fbi, NULL, NULL);
	SDL_RenderPresent(rend);
}

void
setpal(uchar *p0)
{
	int *fp, x;
	uchar *p;

	for(p = p0, fp=fbpal; fp<fbpal+nelem(fbpal); p+=3, fp++)
		*fp = p[0] << 16 | p[1] << 8 | p[2];

	for(p = p0, x = 0; x < 256; x++, p += 3)
		q1pal[x] = x<<24 | p[0]<<16 | p[1]<<8 | p[2];

	scr_fullupdate = 0;
}

void
initfb(void)
{
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
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
}
