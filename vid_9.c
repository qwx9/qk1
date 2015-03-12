#include <u.h>
#include <libc.h>
#include <draw.h>
#include <stdio.h>
#include "quakedef.h"
extern int scr_fullupdate;

int config_notify;
int ignorenext;
viddef_t vid;	// global video state
unsigned short d_8to16table[256];
int d_con_indirect = 0;
uchar *framebuf;	/* draw buffer */
Image *fbim;	/* framebuf image */
byte current_palette[768];
int X11_highhunkmark;
int X11_buffersize;
int vid_surfcachesize;
void *vid_surfcache;
Point center;		/* of window */
Rectangle grabout;	/* center mouse outside of this if cvar set */


typedef u32int PIXEL24;
PIXEL24 st2d_8to24table[256];
int shiftmask_fl = 0;
int r_shift, g_shift, b_shift;
uint r_mask, g_mask, b_mask;

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

void shiftmask_init (void)
{
	uint x;

	r_mask = 0xff0000;
	g_mask = 0xff00;
	b_mask = 0xff;
	for(r_shift = -8, x = 1; x < r_mask; x <<= 1)
		r_shift++;
	for(g_shift = -8, x = 1; x < g_mask; x <<= 1)
		g_shift++;
	for(b_shift = -8, x = 1; x < b_mask; x <<= 1)
		b_shift++;
	shiftmask_fl = 1;
}

PIXEL24 rgb24 (int r, int g, int b)
{
	PIXEL24 p = 0;

	if(shiftmask_fl == 0)
		shiftmask_init();
	if(r_shift > 0)
		p = r<<r_shift & r_mask;
	else if(r_shift < 0)
		p = r>>-r_shift & r_mask;
	else
		p |= r & r_mask;
	if(g_shift > 0)
		p |= g<<g_shift & g_mask;
	else if(g_shift < 0)
		p |= g>>-g_shift & g_mask;
	else
		p |= g & g_mask;
	if(b_shift > 0)
		p |= b<<b_shift & b_mask;
	else if(b_shift < 0)
		p |= b>>-b_shift & b_mask;
	else
		p |= b & b_mask;
	return p;
}

void st3_fixup (uchar *data, int x, int y, int width, int height)
{
	int yi;
	uchar *src;
	PIXEL24 *dest;
	register int count, n;

	if(x < 0 || y < 0)
		return;

	for(yi = y; yi < y+height; yi++){
		src = &data[yi*vid.rowbytes];

		// Duff's Device
		count = width;
		n = (count+7) / 8;
		dest = ((PIXEL24 *)src) + x+width-1;
		src += x+width-1;

		switch (count % 8) {
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
void ResetFrameBuffer (void)
{
	if(framebuf != nil){
		free(framebuf);
		framebuf = nil;
	}

	if(d_pzbuffer){
		D_FlushCaches();
		Hunk_FreeToHighMark(X11_highhunkmark);
		d_pzbuffer = nil;
	}
	X11_highhunkmark = Hunk_HighMark();
	// alloc an extra line in case we want to wrap, and allocate the z-buffer
	X11_buffersize = vid.width * vid.height * sizeof(*d_pzbuffer);
	vid_surfcachesize = D_SurfaceCacheForRes(vid.width, vid.height);
	X11_buffersize += vid_surfcachesize;
	if((d_pzbuffer = Hunk_HighAllocName(X11_buffersize, "video")) == nil)
		Sys_Error("Not enough memory for video mode\n");
	vid_surfcache = (byte *)d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);
	D_InitCaches(vid_surfcache, vid_surfcachesize);

	framebuf = malloc(sizeof *framebuf * Dx(screen->r) * Dy(screen->r) * screen->depth/8);
	vid.buffer = framebuf;
	vid.rowbytes = Dx(screen->r) * screen->depth/8;
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);	/* FIXME */
	vid.conbuffer = vid.buffer;
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	center = addpt(screen->r.min, Pt(Dx(screen->r)/2, Dy(screen->r)/2));
	grabout = insetrect(screen->r, Dx(screen->r)/8);
	if(fbim != nil)
		freeimage(fbim);
	/* FIXME: seems to crash later with DBlack rather than DNofill, why? */
	fbim = allocimage(display, Rect(0, 0, vid.width, vid.height), screen->chan, 1, DNofill);
}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again
void VID_Init (uchar */*palette*/)
{
	ignorenext = 0;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	//vid.cbits = VID_CBITS;
	//vid.grades = VID_GRADES;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

	srand(getpid());

	if(initdraw(nil, nil, "quake") < 0)
		Sys_Error("VID_Init:initdraw: %r\n");
	vid.width = Dx(screen->r);
	vid.height = Dy(screen->r);
	ResetFrameBuffer();
	vid.direct = 0;
}

void VID_ShiftPalette (uchar *p)
{
	VID_SetPalette(p);
}

void VID_SetPalette (uchar *palette)
{
	int i;

	for(i = 0; i < 256; i++)
		st2d_8to24table[i] = rgb24(palette[i*3], palette[i*3+1], palette[i*3+2]);
}

void VID_Shutdown (void)
{
	Con_Printf("VID_Shutdown\n");
	free(framebuf);
	freeimage(fbim);
}

/* flush given rectangles from view buffer to the screen */
void VID_Update (vrect_t *rects)
{
	if(config_notify){		/* skip this frame if window resize */
		config_notify = 0;
		if(getwindow(display, Refnone) < 0)
			Sys_Error("VID_Update:getwindow: %r\n");
		vid.width = Dx(screen->r);
		vid.height = Dy(screen->r);
		ResetFrameBuffer();
		vid.recalc_refdef = 1;			// force a surface cache flush
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	scr_fullupdate = 0;	/* force full update if not 8bit (cf. screen.h) */

	while(rects){
		st3_fixup(framebuf, rects->x, rects->y, rects->width, rects->height);
		rects = rects->pnext;
	}
	loadimage(fbim, fbim->r, framebuf, vid.height * vid.rowbytes);
	draw(screen, screen->r, fbim, nil, ZP);
	flushimage(display, 1);
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
	// direct drawing of the "accessing disk" icon isn't supported under Linux
	USED(x, y, pbitmap, width, height);
}

void D_EndDirectRect (int x, int y, int width, int height)
{
	// direct drawing of the "accessing disk" icon isn't supported under Linux
	USED(x, y, width, height);
}
