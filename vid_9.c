#include <u.h>
#include <libc.h>
#include <draw.h>
#include "quakedef.h"
#include "d_local.h"

extern int scr_fullupdate;

int config_notify;
int ignorenext;
int bits_per_pixel;
viddef_t vid;	// global video state
unsigned short d_8to16table[256];
int d_con_indirect = 0;
uchar *framebuf;	/* draw buffer */
byte current_palette[768];
int X11_highhunkmark;
int X11_buffersize;
int vid_surfcachesize;
void *vid_surfcache;
typedef u16int PIXEL16;
typedef u32int PIXEL24;
PIXEL16 st2d_8to16table[256];
PIXEL24 st2d_8to24table[256];
int shiftmask_fl = 0;
int r_shift, g_shift, b_shift;
uint r_mask, g_mask, b_mask;
Point center;		/* of window */
Rectangle grabout;	/* center mouse outside of this if cvar set */

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

PIXEL16 xlib_rgb16 (int r, int g, int b)
{
	PIXEL16 p = 0;

	if(shiftmask_fl == 0)
		shiftmask_init();
	if(r_shift > 0){
		p = r<<r_shift & r_mask;
	}else if(r_shift < 0){
		p = r>>-r_shift & r_mask;
	}else
		p |= r & r_mask;
	if(g_shift > 0){
		p |= g<<g_shift & g_mask;
	}else if(g_shift < 0){
		p |= g>>-g_shift & g_mask;
	}else
		p |= g & g_mask;
	if(b_shift > 0){
		p |= b<<b_shift & b_mask;
	}else if(b_shift < 0){
		p |= b>>-b_shift & b_mask;
	}else
		p |= b & b_mask;
	return p;
}

PIXEL24 xlib_rgb24 (int r, int g, int b)
{
	PIXEL24 p = 0;

	if(shiftmask_fl == 0)
		shiftmask_init();
	if(r_shift > 0){
		p = r<<r_shift & r_mask;
	}else if(r_shift < 0){
		p = r>>-r_shift & r_mask;
	}else
		p |= r & r_mask;
	if(g_shift > 0){
		p |= g<<g_shift & g_mask;
	}else if(g_shift < 0){
		p |= g>>-g_shift & g_mask;
	}else
		p |= g & g_mask;
	if(b_shift > 0){
		p |= b<<b_shift & b_mask;
	}else if(b_shift < 0){
		p |= b>>-b_shift & b_mask;
	}else
		p |= b & b_mask;
	return p;
}

void st2_fixup (uchar *data, int x, int y, int width, int height)
{
	int yi;
	uchar *src;
	PIXEL16 *dest;
	register int count, n;

	if(x < 0 || y < 0)
		return;

	for(yi = y; yi < y+height; yi++){
		src = &data[yi*vid.rowbytes];

		// Duff's Device
		count = width;
		n = (count+7) / 8;
		dest = ((PIXEL16 *)src) + x+width-1;
		src += x+width-1;

		switch(count % 8){
		case 0:	do{	*dest-- = st2d_8to16table[*src--];
		case 7:		*dest-- = st2d_8to16table[*src--];
		case 6:		*dest-- = st2d_8to16table[*src--];
		case 5:		*dest-- = st2d_8to16table[*src--];
		case 4:		*dest-- = st2d_8to16table[*src--];
		case 3:		*dest-- = st2d_8to16table[*src--];
		case 2:		*dest-- = st2d_8to16table[*src--];
		case 1:		*dest-- = st2d_8to16table[*src--];
			}while(--n > 0);
		}
		//for(xi = x+width-1; xi >= x; xi--)
		//	dest[xi] = st2d_8to16table[src[xi]];
	}
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
		//	dest[xi] = st2d_8to16table[src[xi]];
	}
}

void ResetFrameBuffer (void)
{
	if(framebuf)
		free(framebuf);
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

	d_pzbuffer = Hunk_HighAllocName(X11_buffersize, "video");
	if(d_pzbuffer == nil)
		Sys_Error("Not enough memory for video mode\n");

	vid_surfcache = (byte *)d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);

	D_InitCaches(vid_surfcache, vid_surfcachesize);

	framebuf = malloc(sizeof *framebuf * Dx(screen->r) * Dy(screen->r) * screen->depth/8);
	vid.buffer = framebuf;
	vid.conbuffer = vid.buffer;
}

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void resetscr(void)
{
	ResetFrameBuffer();
	center = addpt(screen->r.min, Pt(Dx(screen->r)/2, Dy(screen->r)/2));
	grabout = insetrect(screen->r, Dx(screen->r)/8);
}

void VID_Init (unsigned char *palette)
{
	int pnum;

	ignorenext = 0;
	//vid.width = 320;
	//vid.height = 200;
	vid.width = -1;
	vid.height = -1;
	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.numpages = 2;
	vid.colormap = host_colormap;
	//vid.cbits = VID_CBITS;
	//vid.grades = VID_GRADES;
	vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

	srand(getpid());	/* glibc srandom() affects rand() too, right? */

	if(pnum = COM_CheckParm("-winsize")){
		if(pnum >= com_argc-2)
			Sys_Error("VID: -winsize <width> <height>\n");
		vid.width = Q_atoi(com_argv[pnum+1]);
		vid.height = Q_atoi(com_argv[pnum+2]);
		if(!vid.width || !vid.height)
			Sys_Error("VID: Bad window width/height\n");
	}
	if(pnum = COM_CheckParm("-width")){
		if(pnum >= com_argc-1)
			Sys_Error("VID: -width <width>\n");
		vid.width = Q_atoi(com_argv[pnum+1]);
		if(!vid.width)
			Sys_Error("VID: Bad window width\n");
	}
	if(pnum = COM_CheckParm("-height")){
		if(pnum >= com_argc-1)
			Sys_Error("VID: -height <height>\n");
		vid.height = Q_atoi(com_argv[pnum+1]);
		if(!vid.height)
			Sys_Error("VID: Bad window height\n");
	}

	if(initdraw(nil, nil, "quake") < 0)
		Sys_Error("VID_Init:initdraw");

	if(vid.width == -1)
		vid.width = Dx(screen->r);
	if(vid.height == -1)
		vid.height = Dy(screen->r);

	/* TODO: resize properly if -winsize,.. params provided, draw centered */

	if(screen->chan == CMAP8)
		VID_SetPalette(palette);
	resetscr();
	vid.rowbytes = Dx(screen->r) * screen->depth/8;
	vid.buffer = framebuf;
	vid.direct = 0;
	vid.conbuffer = framebuf;
	vid.conrowbytes = vid.rowbytes;
	vid.conwidth = vid.width;
	vid.conheight = vid.height;
	/* FIXME */
	vid.aspect = (float)vid.height / (float)vid.width * (320.0/240.0);

	draw(screen, screen->r, display->black, nil, ZP);
}

void VID_ShiftPalette (unsigned char *p)
{
	VID_SetPalette(p);
}

void VID_SetPalette (unsigned char *palette)
{
	int i;

	for(i = 0; i < 256; i++){
		st2d_8to16table[i] = xlib_rgb16(palette[i*3], palette[i*3+1], palette[i*3+2]);
		st2d_8to24table[i] = xlib_rgb24(palette[i*3], palette[i*3+1], palette[i*3+2]);
	}
	if(screen->chan == 8){
		if(palette != current_palette)
			memcpy(current_palette, palette, 768);
		/* FIXME
		XColor colors[256];
		for(i = 0 ; i < 256 ; i++){
			colors[i].pixel = i;
			colors[i].flags = DoRed|DoGreen|DoBlue;
			colors[i].red = palette[i*3] * 257;
			colors[i].green = palette[i*3+1] * 257;
			colors[i].blue = palette[i*3+2] * 257;
		}
		XStoreColors(x_disp, x_cmap, colors, 256);
		*/
	}

}

void VID_Shutdown (void)
{
	Con_Printf("VID_Shutdown\n");
}

/* flush given rectangles from view buffer to the screen */
void VID_Update (vrect_t *rects)
{
	Rectangle svr, dr;
	Image *im;

	if(config_notify){		/* skip this frame if window resize */
		config_notify = 0;
		if(getwindow(display, Refnone) < 0)
			Sys_Error("VID_Update:getwindow");
		vid.width = Dx(screen->r);
		vid.height = Dy(screen->r);
		resetscr();
		vid.rowbytes = vid.width * screen->depth/8;
		vid.buffer = framebuf;
		vid.conbuffer = framebuf;
		vid.conwidth = vid.width;
		vid.conheight = vid.height;
		vid.conrowbytes = vid.rowbytes;
		vid.recalc_refdef = 1;			// force a surface cache flush
		Con_CheckResize();
		Con_Clear_f();
		return;
	}

	if(screen->chan != CMAP8)	/* force full update if not 8bit */
		scr_fullupdate = 0;

	/* FIXME: seems to crash later with DBlack rather than DNofill, why? */
	im = allocimage(display, Rect(0, 0, vid.width, vid.height), screen->chan, 1, DNofill);

	svr = screen->clipr;
	while(rects){
		if(screen->chan == RGB16)
			st2_fixup(framebuf, rects->x, rects->y,
				rects->width, rects->height);
		/* FIXME: ??? just assume truecolor? (not RGB16 or CMAP8) */
		else if(screen->chan == RGB24 || screen->chan == RGBA32 || screen->chan == XRGB32)
			st3_fixup(framebuf, rects->x, rects->y,
				rects->width, rects->height);

		/* FIXME */
		//loadimage(im, im->r, &framebuf[rects->x*vid.rowbytes];
		loadimage(im, im->r, framebuf, vid.height * vid.rowbytes);
		dr = Rpt(addpt(screen->r.min, Pt(rects->x, rects->y)),
			addpt(screen->r.min, Pt(rects->x+rects->width, rects->y+rects->height)));
		//replclipr(screen, 0, dr);	/* FIXME: necessary? */
		draw(screen, dr, im, nil, ZP);
		rects = rects->pnext;
	}
	replclipr(screen, 0, svr);
	freeimage(im);
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
