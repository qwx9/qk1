#include <u.h>
#include <libc.h>
#include <draw.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

viddef_t vid;		/* global video state */
int resized;
int dumpwin, scaleon;
Point center;		/* of window */
Rectangle grabr;

static int scale = 1;
static s32int fbpal[256];
static uchar *fb, *fbs;
static Image *fbi;
static Rectangle fbr;

static void
scalefb(int dy)
{
	int *p, c, *s;

	if(scale < 2)
		return;
	p = (s32int *)fbs;
	s = (s32int *)fb;
	dy *= vid.width;
	while(dy-- > 0){
		c = *s++;
		switch(scale){
		case 16: p[15] = c;
		case 15: p[14] = c;
		case 14: p[13] = c;
		case 13: p[12] = c;
		case 12: p[11] = c;
		case 11: p[10] = c;
		case 10: p[9] = c;
		case 9: p[8] = c;
		case 8: p[7] = c;
		case 7: p[6] = c;
		case 6: p[5] = c;
		case 5: p[4] = c;
		case 4: p[3] = c;
		case 3: p[2] = c;
		case 2: p[1] = c; p[0] = c;
		}
		p += scale;
	}
}

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

	if(scaleon){
		scale = Dx(screen->r) / vid.width;
		if(scale <= 0)
			scale = 1;
		else if(scale > 16)
			scale = 16;
	}else{
		/* lower than 320x240 doesn't really make sense,
		 * but at least this prevents a crash, beyond that
		 * it's your funeral */
		vid.width = Dx(screen->r);
		if(vid.width < 320)
			vid.width = 320;
		vid.height = Dy(screen->r);
		if(vid.height < 160)
			vid.height = 160;
	}
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

	center = divpt(addpt(screen->r.min, screen->r.max), 2);
	p = Pt(scale * vid.width/2, scale * vid.height/2);
	fbr = Rpt(subpt(center, p), addpt(center, p));
	p = Pt(vid.width/4, vid.height/4);
	grabr = Rpt(subpt(center, p), addpt(center, p));
	freeimage(fbi);
	free(fb);
	fbi = allocimage(display,
		Rect(0, 0, vid.width * scale, scale > 1 ? 1 : vid.height),
		XRGB32, scale > 1, 0);
	if(fbi == nil)
		sysfatal("resetfb: %r (%d %d)", vid.width, vid.height);
	fb = emalloc(vid.rowbytes * vid.height);
	if(scaleon){
		free(fbs);
		fbs = emalloc(vid.rowbytes * scale * vid.height);
	}
	vid.buffer = fb;
	vid.conbuffer = fb;
	draw(screen, screen->r, display->black, nil, ZP);
}

/* only exists to allow taking tear-free screenshots ingame... */
static int
writebit(void)
{
	int n, fd;
	char *s;

	s = va("%s/quake.%ld.bit", fsdir, time(nil));
	if(access(s, AEXIST) != -1){
		werrstr("writebit: not overwriting %s", s);
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
	scalefb(dy);
	if(scale == 1){
		loadimage(fbi, Rect(0,0,vid.width,dy), fb, dy * vid.rowbytes);
		draw(screen, Rpt(fbr.min, Pt(fbr.max.x,
			fbr.max.y - vid.height + dy)), fbi, nil, ZP);
	}else{
		Rectangle r;
		uchar *p;

		p = fbs;
		r = fbr;
		while(r.min.y < fbr.max.y){
			r.max.y = r.min.y + scale;
			p += loadimage(fbi, fbi->r, p, vid.rowbytes * scale);
			draw(screen, r, fbi, nil, ZP);
			r.min.y = r.max.y;
		}
	}
	flushimage(display, 1);
	if(dumpwin){
		if(writebit() < 0)
			Con_Printf(va("writebit: %r\n"));
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
