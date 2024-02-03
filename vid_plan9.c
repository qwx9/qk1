#include "quakedef.h"
#include <draw.h>
#include <thread.h>

viddef_t vid;		/* global video state */
Point center;		/* of window */
Rectangle grabr;

pixel_t q1pal[256];
static Image *fbi;
static u32int *scibuf;
static int scifactor;
static Rectangle fbr;
static pixel_t *vidbuffers[2];
static int bufi = 0;
static Channel *frame;

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
	vid.height = Dy(screen->r);
	scifactor = v_scale.value;
	vid.width /= scifactor;
	vid.height /= scifactor;
	vid.width = clamp(vid.width, 320, MAXWIDTH);
	vid.height = clamp(vid.height, 240, MAXHEIGHT);

	if(dvars.zb != nil)
		D_FlushCaches();

	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	hunkvbuf = vid.width * vid.height * sizeof(*dvars.zb);
	scachesz = D_SurfaceCacheForRes(vid.width, vid.height);
	hunkvbuf += scachesz;
	if((dvars.zb = realloc(dvars.zb, hunkvbuf)) == nil)
		sysfatal("%r");
	memset(dvars.zb, 0, hunkvbuf);
	surfcache = (byte*)(dvars.zb + vid.width * vid.height);
	D_InitCaches(surfcache, scachesz);

	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);
	vid.conwidth = vid.width;
	vid.conheight = vid.height;

	center = divpt(addpt(screen->r.min, screen->r.max), 2);
	p = Pt(vid.width/2, vid.height/2);
	fbr = Rpt(subpt(center, p), addpt(center, p));
	p = Pt(vid.width/4, vid.height/4);
	grabr = Rpt(subpt(center, p), addpt(center, p));
	for(i = 0; i < nelem(vidbuffers); i++)
		vidbuffers[i] = realloc(vidbuffers[i], (vid.width*vid.height+16)*sizeof(pixel_t));
	free(r_warpbuffer);
	r_warpbuffer = emalloc((vid.width*vid.height+16)*sizeof(pixel_t));
	vid.maxwarpwidth = vid.width;
	vid.maxwarpheight = vid.height;
	vid.buffer = vidbuffers[bufi = 0];
	vid.conbuffer = vid.buffer;
	draw(screen, screen->r, display->black, nil, ZP);

	freeimage(fbi);
	if(scifactor != 1){
		fbi = allocimage(display, Rect(0, 0, vid.width*scifactor, 1), XRGB32, 1, DNofill);
		scibuf = realloc(scibuf, vid.width*scifactor*sizeof(*scibuf));
	}else{
		fbi = allocimage(display, Rect(0, 0, vid.width, vid.height), XRGB32, 0, 0);
		free(scibuf);
		scibuf = nil;
	}
	if(fbi == nil)
		sysfatal("resetfb: %r (%d %d)", vid.width, vid.height);
}

static void
loader(void *)
{
	u32int *in, *out;
	int n, x, y, j;
	Point center;
	Rectangle r;
	byte *f;

	center = addpt(screen->r.min, Pt(Dx(screen->r)/2, Dy(screen->r)/2));
	r = Rect(0, 0, vid.width, vid.height);
	if(scibuf == nil)
		n = vid.width * vid.height;
	else
		n = vid.width * scifactor * sizeof(*scibuf);

	for(;;){
		if((f = recvp(frame)) == nil)
			break;
		if(scibuf != nil){
			in = (u32int*)f;

			r = rectsubpt(
				rectaddpt(Rect(0, 0, scifactor*vid.width, scifactor), center),
				Pt(scifactor*vid.width/2, scifactor*vid.height/2)
			);

			for(y = 0; y < vid.height; y++){
				for(x = 0, out = scibuf; x < vid.width; x++, in++){
					for(j = 0; j < scifactor; j++, out++)
						*out = *in;
				}
				if(loadimage(fbi, fbi->r, (byte*)scibuf, n) != n)
					sysfatal("%r");
				draw(screen, r, fbi, nil, ZP);
				r.min.y += scifactor;
				r.max.y += scifactor;
			}
		}else{
			if(loadimage(fbi, r, f, n*4) != n*4)
				sysfatal("%r");
			draw(screen, fbr, fbi, nil, ZP);
		}
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
	if(vid.resized){		/* skip this frame if window resize */
		vid.resized = false;
		stopfb();
		if(getwindow(display, Refnone) < 0)
			sysfatal("%r");
		resetfb();
		vid.recalc_refdef = true;	/* force a surface cache flush */
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
setpal(byte *p0)
{
	int x;
	byte *p;

	for(p = p0, x = 0; x < 256; x++, p += 3)
		q1pal[x] = (x < 256-32 ? 0xff : 0)<<24 | p[0]<<16 | p[1]<<8 | p[2];
	q1pal[255] &= 0;

	scr_fullupdate = 0;
}

void
initfb(void)
{
	vid.numpages = 2;
	vid.colormap = malloc(256*64*sizeof(pixel_t));
	torgbx(host_colormap, vid.colormap, 256*64);
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
	if(initdraw(nil, nil, "quake") < 0)
		sysfatal("initdraw: %r\n");
	resetfb();
}
