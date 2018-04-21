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

static int fbpal[256];
static uchar *fb;
static Image *fbi;

static void
drawfb(int dy)
{
	uchar *s;
	int n, we, w8, wr, *d;

	we = vid.width - 1;
	w8 = vid.width + 7 >> 3;
	wr = vid.width % 8;
	dy *= vid.rowbytes;
	while((dy -= vid.rowbytes) >= 0){
		s = fb + dy;
		d = ((int *)s) + we;
		s += we;
		n = w8;
		switch(wr){
		case 0:	do{	*d-- = fbpal[*s--];
		case 7:		*d-- = fbpal[*s--];
		case 6:		*d-- = fbpal[*s--];
		case 5:		*d-- = fbpal[*s--];
		case 4:		*d-- = fbpal[*s--];
		case 3:		*d-- = fbpal[*s--];
		case 2:		*d-- = fbpal[*s--];
		case 1:		*d-- = fbpal[*s--];
			}while(--n > 0);
		}
	}
}

static void
resetfb(void)
{
	static int highhunk;
	void *surfcache;
	int hunkvbuf, scachesz;
	Point p;

	vid.width = Dx(screen->r);
	vid.height = Dy(screen->r);
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

	vid.rowbytes = vid.width * sizeof *fbpal;
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	center = addpt(screen->r.min, Pt(vid.width/2, vid.height/2));
	p = Pt(vid.width/4, vid.height/4);
	grabr = Rpt(subpt(center, p), addpt(center, p));
	freeimage(fbi);
	free(fb);
	fbi = allocimage(display, Rect(0,0,vid.width,vid.height), XRGB32, 0, 0);
	if(fbi == nil)
		sysfatal("resetfb: %r");
	fb = emalloc(vid.rowbytes * vid.height * sizeof *fb);
	vid.buffer = fb;
	vid.conbuffer = fb;
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
	n = writeimage(fd, fbi, 0);
	close(fd);
	if(n >= 0)
		Con_Printf("Wrote %s\n", s);
	return n;
}

void
flipfb(int dy)
{
	if(resized){		/* skip this frame if window resize */
		resized = 0;
		if(getwindow(display, Refnone) < 0)
			sysfatal("getwindow: %r");
		resetfb();
		vid.recalc_refdef = 1;	/* force a surface cache flush */
		Con_CheckResize();
		Con_Clear_f();
		return;
	}
	drawfb(dy);
	loadimage(fbi, Rect(0,0,vid.width,dy), fb, dy * vid.rowbytes);
	draw(screen, Rpt(screen->r.min, Pt(screen->r.max.x,
		screen->r.max.y - vid.height + dy)), fbi, nil, ZP);
	flushimage(display, 1);
	if(dumpwin){
		if(writebit() < 0)
			Con_Printf("writebit: %r\n");
		dumpwin = 0;
	}
}

void
setpal(uchar *p)
{
	int *fp;

	for(fp=fbpal; fp<fbpal+nelem(fbpal); p+=3)
		*fp++ = p[0] << 16 | p[1] << 8 | p[2];
	scr_fullupdate = 0;
}

void
initfb(void)
{
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	if(initdraw(nil, nil, "quake") < 0)
		sysfatal("initdraw: %r\n");
	resetfb();
}
