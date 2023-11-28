#include "quakedef.h"
#include <draw.h>
#include <thread.h>

viddef_t vid;		/* global video state */
int resized;
Point center;		/* of window */
Rectangle grabr;

s32int fbpal[256];
pixel_t q1pal[256];
static Image *fbi;
static Rectangle fbr;
static pixel_t *vidbuffers[2];
static int bufi = 0;
static Channel *frame;

void pal2xrgb(int n, s32int *pal, u8int *s, u32int *d);

static void
resetfb(void)
{
	static int highhunk;
	void *surfcache;
	int hunkvbuf, scachesz, i;
	Point p;

	/* lower than 320x240 doesn't really make sense,
	 * but at least this prevents a crash, beyond that
	 * it's your funeral */
	vid.width = Dx(screen->r);
	if(vid.width < 320)
		vid.width = 320;
	vid.height = Dy(screen->r);
	if(vid.height < 160)
		vid.height = 160;
	if(d_pzbuffer != nil)
		D_FlushCaches();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof *d_pzbuffer;
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	if((d_pzbuffer = realloc(d_pzbuffer, hunkvbuf)) == nil)
		sysfatal("%r");
	surfcache = (byte*)(d_pzbuffer + vid.width * vid.height);
	D_InitCaches(surfcache, scachesz);

	vid.rowbytes = vid.width;
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	center = divpt(addpt(screen->r.min, screen->r.max), 2);
	p = Pt(vid.width/2, vid.height/2);
	fbr = Rpt(subpt(center, p), addpt(center, p));
	p = Pt(vid.width/4, vid.height/4);
	grabr = Rpt(subpt(center, p), addpt(center, p));
	for(i = 0; i < nelem(vidbuffers); i++)
		vidbuffers[i] = realloc(vidbuffers[i], (vid.width*vid.height+16)*sizeof(pixel_t));
	freeimage(fbi);
	fbi = allocimage(display, Rect(0, 0, vid.width, vid.height), XRGB32, 0, 0);
	if(fbi == nil)
		sysfatal("resetfb: %r (%d %d)", vid.width, vid.height);
	vid.buffer = vidbuffers[bufi = 0];
	vid.conbuffer = vid.buffer;
	draw(screen, screen->r, display->black, nil, ZP);
}

static void
loader(void *)
{
	byte *f;
	Rectangle r;
	int n;

	r = Rect(0, 0, vid.width, vid.height);
	n = vid.width * vid.height;
	for(;;){
		if((f = recvp(frame)) == nil)
			break;
		if(loadimage(fbi, r, f, n*4) != n*4)
			sysfatal("%r");
		draw(screen, fbr, fbi, nil, ZP);
		if(flushimage(display, 1) < 0)
			sysfatal("%r");
	}
	threadexits(nil);
}

void
stopfb(void)
{
	if(frame != nil){
		sendp(frame, nil);
		chanclose(frame);
		frame = nil;
	}
}

void
flipfb(void)
{
	if(resized){		/* skip this frame if window resize */
		stopfb();
		if(getwindow(display, Refnone) < 0)
			sysfatal("%r");
		resized = 0;
		resetfb();
		vid.recalc_refdef = 1;	/* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f();
		return;
	}
	if(frame == nil){
		frame = chancreate(sizeof(pixel_t*), 0);
		proccreate(loader, nil, 4096);
	}
	if(sendp(frame, vidbuffers[bufi]) > 0){
		bufi = (bufi+1) % nelem(vidbuffers);
		vid.buffer = vidbuffers[bufi];
		vid.conbuffer = vid.buffer;
	}
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
	if(initdraw(nil, nil, "quake") < 0)
		sysfatal("initdraw: %r\n");
	resetfb();
}
