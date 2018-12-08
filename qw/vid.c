#include <u.h>
#include <libc.h>
#include <draw.h>
#include <stdio.h>
#include "quakedef.h"

viddef_t vid;
ushort d_8to16table[256];
int resized;
Point center;

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int);

typedef ulong PIXEL;

enum{
	Rmask	= 0xff0000,
	Gmask	= 0xff00,
	Bmask	= 0xff
};
static int shifton, rshift, gshift, bshift;
static uchar *framebuf;	/* draw buffer */
static Image *fbim;	/* framebuf image */

static PIXEL st2d_8to16table[256];

static void	mkmasks(void);
static PIXEL	rgb24(int, int, int);
static void	st2_fixup(uchar *, int, int, int, int);
static void	resetfb(void);


static void
mkmasks(void)
{
	uint x;

	for(rshift = -8, x = 1; x < Rmask; x <<= 1)
		rshift++;
	for(gshift = -8, x = 1; x < Gmask; x <<= 1)
		gshift++;
	for(bshift = -8, x = 1; x < Bmask; x <<= 1)
		bshift++;
	shifton = 1;
}

static PIXEL
rgb24(int r, int g, int b)
{
	PIXEL p = 0;

	if(!shifton)
		mkmasks();

	if(rshift > 0)
		p = r<<rshift & Rmask;
	else if(rshift < 0)
		p = r>>-rshift & Rmask;
	else
		p |= r & Rmask;
	if(gshift > 0)
		p |= g<<gshift & Gmask;
	else if(gshift < 0)
		p |= g>>-gshift & Gmask;
	else
		p |= g & Gmask;
	if(bshift > 0)
		p |= b<<bshift & Bmask;
	else if(bshift < 0)
		p |= b>>-bshift & Bmask;
	else
		p |= b & Bmask;
	return p;
}

static void
st2_fixup(uchar *data, int x, int y, int width, int height)
{
	int xi, yi;
	uchar *src;
	PIXEL *dest;

	if(x < 0 || y < 0)
		return;

	for (yi = y; yi < y+height; yi++){
		src = &data[yi*vid.rowbytes];
		dest = (PIXEL *)src;
		for(xi = x+width-1; xi >= x; xi--)
			dest[xi] = st2d_8to16table[src[xi]];
	}
}

/* vid.height and vid.width must be set correctly before this call */
static void
resetfb(void)
{
	static int highhunk;
	void *surfcache;
	int hunkvbuf, scachesz;

	if(framebuf != nil){
		free(framebuf);
		framebuf = nil;
	}
	if(d_pzbuffer){
		D_FlushCaches();
		Hunk_FreeToHighMark(highhunk);
		d_pzbuffer = nil;
	}

	highhunk = Hunk_HighMark();
	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof(*d_pzbuffer);
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	if((d_pzbuffer = Hunk_HighAllocName(hunkvbuf, "video")) == nil)
		sysfatal("Not enough memory for video mode\n");
	surfcache = (byte *)d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);
	D_InitCaches(surfcache, scachesz);

	framebuf = malloc(sizeof *framebuf * Dx(screen->r) * Dy(screen->r) * screen->depth/8);
	vid.buffer = framebuf;
	vid.rowbytes = Dx(screen->r) * screen->depth/8;
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);	/* FIXME */
	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	center = addpt(screen->r.min, Pt(Dx(screen->r)/2, Dy(screen->r)/2));
	if(fbim != nil)
		freeimage(fbim);
	fbim = allocimage(display, Rect(0, 0, vid.width, vid.height), screen->chan, 1, DNofill);
}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again
void
VID_Init(uchar */*palette*/)
{
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

	srand(getpid());

	if(initdraw(nil, nil, "quake") < 0)
		sysfatal("VID_Init:initdraw: %r\n");
	vid.width = Dx(screen->r);
	vid.height = Dy(screen->r);
	resetfb();
	vid.direct = 0;
}

void
VID_ShiftPalette(uchar *p)
{
	VID_SetPalette(p);
}

void
VID_SetPalette(uchar *palette)
{
	int i;

	for(i = 0; i < 256; i++)
		st2d_8to16table[i] = rgb24(palette[i*3], palette[i*3+1], palette[i*3+2]);
}

void
VID_Shutdown(void)
{
	Con_Printf("VID_Shutdown\n");
	free(framebuf);
	freeimage(fbim);
}

void
VID_Update(vrect_t *rects)
{
	if(resized){		/* skip this frame if window resize */
		resized = 0;
		if(getwindow(display, Refnone) < 0)
			sysfatal("VID_Update:getwindow: %r\n");
		vid.width = Dx(screen->r);
		vid.height = Dy(screen->r);
		resetfb();
		vid.recalc_refdef = 1;	/* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	scr_fullupdate = 0;	/* force full update if not 8bit (cf. screen.h) */

	while(rects){
		st2_fixup(framebuf, rects->x, rects->y, rects->width, rects->height);
		rects = rects->pnext;
	}
	loadimage(fbim, fbim->r, framebuf, vid.height * vid.rowbytes);
	draw(screen, screen->r, fbim, nil, ZP);
	flushimage(display, 1);
}

/* direct drawing of the "accessing disk" icon */
void
D_BeginDirectRect(int /*x*/, int /*y*/, byte */*pbitmap*/, int /*width*/, int /*height*/)
{
}

void
D_EndDirectRect(int /*x*/, int /*y*/, int /*width*/, int /*height*/)
{
}

void VID_LockBuffer (void)
{
}

void VID_UnlockBuffer (void)
{
}
