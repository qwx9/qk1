#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

viddef_t vid;		/* global video state */
int resized;
int dumpwin;
Point center;		/* of window */
Rectangle grabr;
int d_con_indirect;
void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

typedef u32int px24;
px24 st2d_8to24table[256];
ushort d_8to16table[256];

enum{
	Rmask	= 0xff0000,
	Gmask	= 0xff00,
	Bmask	= 0xff
};
static int shifton;
static int rshift;
static int gshift;
static int bshift;
static uchar *framebuf;	/* draw buffer */
static Image *fbim;	/* framebuf image */


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

static px24
rgb24(int r, int g, int b)
{
	px24 p = 0;

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
st3_fixup(uchar *data, int x, int y, int width, int height)
{
	int yi;
	uchar *src;
	px24 *dest;
	register int count, n;

	if(x < 0 || y < 0)
		return;

	for(yi = y; yi < y+height; yi++){
		src = &data[yi*vid.rowbytes];

		// Duff's Device
		count = width;
		n = (count+7) / 8;
		dest = ((px24 *)src) + x+width-1;
		src += x+width-1;

		switch(count % 8){
		case 0:	do{	*dest-- = st2d_8to24table[*src--];
		case 7:		*dest-- = st2d_8to24table[*src--];
		case 6:		*dest-- = st2d_8to24table[*src--];
		case 5:		*dest-- = st2d_8to24table[*src--];
		case 4:		*dest-- = st2d_8to24table[*src--];
		case 3:		*dest-- = st2d_8to24table[*src--];
		case 2:		*dest-- = st2d_8to24table[*src--];
		case 1:		*dest-- = st2d_8to24table[*src--];
			}while(--n > 0);
		}
		//for(xi = x+width-1; xi >= x; xi--)
		//	dest[xi] = st2d_8to24table[src[xi]];
	}
}

/* vid.height and vid.width must be set correctly before this call */
static void
resetfb(void)
{
	static int highhunk;
	void *surfcache;
	int hunkvbuf, scachesz;
	Point p;

	if(d_pzbuffer != nil){
		D_FlushCaches();
		Hunk_FreeToHighMark(highhunk);
		d_pzbuffer = nil;
	}

	highhunk = Hunk_HighMark();
	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof *d_pzbuffer;
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	if((d_pzbuffer = Hunk_HighAllocName(hunkvbuf, "video")) == nil)
		sysfatal("Not enough memory for video mode\n");
	surfcache = (byte *)d_pzbuffer + vid.width * vid.height * sizeof *d_pzbuffer;
	D_InitCaches(surfcache, scachesz);

	vid.rowbytes = vid.width * 32/8;
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	center = addpt(screen->r.min, Pt(vid.width/2, vid.height/2));
	p = Pt(vid.width/4, vid.height/4);
	grabr = Rpt(subpt(center, p), addpt(center, p));
	freeimage(fbim);
	free(framebuf);
	fbim = allocimage(display, Rect(0,0,vid.width,vid.height), XRGB32, 0, 0);
	if(fbim == nil)
		sysfatal("resetfb: %r");
	framebuf = emalloc(vid.width * vid.height * 32/8 * sizeof *framebuf);
	vid.buffer = framebuf;
	vid.conbuffer = framebuf;
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
		sysfatal("initdraw: %r\n");
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
		st2d_8to24table[i] = rgb24(palette[i*3], palette[i*3+1], palette[i*3+2]);
}

void
VID_Shutdown(void)
{
	free(framebuf);
	freeimage(fbim);
}

/* only exists to allow taking tear-free screenshots ingame... */
static int
writebit(void)
{
	int n, fd;
	char *s;

	for(n=0, s=nil; n<100; n++){
		s = va("%s/quake%02d.bit", fsdir, n);
		if(access(s, AEXIST) == -1)
			break;
	}
	if(n == 100){
		werrstr("at static file limit");
		return -1;
	}
	if(fd = create(s, OWRITE, 0644), fd < 0)
		return -1;
	n = writeimage(fd, fbim, 0);
	close(fd);
	if(n >= 0)
		Con_Printf("Wrote %s\n", s);
	return n;
}

/* flush given rectangles from view buffer to the screen */
void
VID_Update(vrect_t *rects)
{
	if(resized){		/* skip this frame if window resize */
		resized = 0;
		if(getwindow(display, Refnone) < 0)
			sysfatal("getwindow: %r");
		vid.width = Dx(screen->r);
		vid.height = Dy(screen->r);
		resetfb();
		vid.recalc_refdef = 1;	/* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	scr_fullupdate = 0;	/* force full update if not 8bit (cf. screen.h) */

	while(rects != nil){
		st3_fixup(framebuf, rects->x, rects->y, rects->width, rects->height);
		rects = rects->pnext;
	}
	loadimage(fbim, fbim->r, framebuf, vid.height * vid.rowbytes);
	draw(screen, screen->r, fbim, nil, ZP);
	flushimage(display, 1);
	if(dumpwin){
		if(writebit() < 0)
			Con_Printf("writebit: %r\n");
		dumpwin = 0;
	}
}

/* direct drawing of the "accessing disk" icon */
void
D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
	USED(x, y, pbitmap, width, height);
}

void
D_EndDirectRect(int x, int y, int width, int height)
{
	USED(x, y, width, height);
}
