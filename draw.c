#include "quakedef.h"

typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	pixel_t	*ptexpixels;
	int		rowbytes;
} rectdesc_t;

qpic_t *draw_disc;

static rectdesc_t r_rectdesc;
static pixel_t draw_chars[128*128];				// 8*8 graphic characters
static qpic_t *draw_backtile;
Wad *wad_gfx;

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[Npath];
	mem_user_t	cache;
} cachepic_t;

#define	MAX_CACHED_PICS		128
static cachepic_t menu_cachepics[MAX_CACHED_PICS];
static int menu_numcachepics;

/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i, n;
	qpic_t		*q;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			break;

	if (i == menu_numcachepics)
	{
		if (menu_numcachepics == MAX_CACHED_PICS)
			fatal ("menu_numcachepics == MAX_CACHED_PICS");
		menu_numcachepics++;
		strcpy (pic->name, path);
	}

	q = Cache_Check(&pic->cache);

	if(q)
		return q;

	// load the pic from disk
	q = loadcachelmp(path, &pic->cache);
	if(q == nil)
		fatal("Draw_CachePic: %s", lerr());
	q->width = LittleLong(q->width);
	q->height = LittleLong(q->height);
	n = q->width*q->height;
	q = Cache_Realloc(&pic->cache, sizeof(*q)+n*sizeof(pixel_t));
	torgbx((byte*)q->pixels, q->pixels, n);

	return q;
}

qpic_t *
Draw_PicFromWad(char *name)
{
	return W_ReadQpic(wad_gfx, name, nil);
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int i;
	if(W_ReadPixels(wad_gfx, "conchars", draw_chars, nelem(draw_chars)) < 0)
		fatal("Draw_Init: %s", lerr());
	/* black is transparent */
	for(i = 0; i < nelem(draw_chars); i++){
		if((draw_chars[i] & 0xffffff) == 0)
			draw_chars[i] = 0;
	}
	if((draw_disc = Draw_PicFromWad("disc")) == nil)
		fatal("Draw_Init: %s", lerr());
	if((draw_backtile = Draw_PicFromWad("backtile")) == nil)
		fatal("Draw_Init: %s", lerr());
	r_rectdesc.width = draw_backtile->width;
	r_rectdesc.height = draw_backtile->height;
	r_rectdesc.ptexpixels = draw_backtile->pixels;
	r_rectdesc.rowbytes = draw_backtile->width;
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	pixel_t *dest, *source;
	int drawline, row, col;

	num &= 255;

	if (y <= -8)
		return;			// totally off screen

#ifdef PARANOID
	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		fatal ("Con_DrawCharacter: (%d, %d)", x, y);
	if (num < 0 || num > 255)
		fatal ("Con_DrawCharacter: char %d", num);
#endif

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else
		drawline = 8;
	dest = vid.conbuffer + y*vid.conrowbytes + x;
	while(drawline--){
		if(source[0]) dest[0] = source[0];
		if(source[1]) dest[1] = source[1];
		if(source[2]) dest[2] = source[2];
		if(source[3]) dest[3] = source[3];
		if(source[4]) dest[4] = source[4];
		if(source[5]) dest[5] = source[5];
		if(source[6]) dest[6] = source[6];
		if(source[7]) dest[7] = source[7];
		source += 128;
		dest += vid.conrowbytes;
	}
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	pixel_t *dest, *source;
	int px, py;

	if ((x < 0) ||
		(x + pic->width > vid.width) ||
		(y < 0) ||
		(y + pic->height > vid.height))
	{
		fatal ("Draw_Pic: bad coordinates");
	}
	source = pic->pixels;
	dest = vid.buffer + y * vid.rowbytes + x;
	for(py = 0; py < pic->height; py++){
		for(px = 0; px < pic->width; px++){
			if(opaque(source[px]))
				dest[px] = source[px];
		}
		dest += vid.rowbytes;
		source += pic->width;
	}
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	pixel_t *dest, *source, tpix;
	int v, u;

	if (x < 0 || y < 0 || x+pic->width > vid.width || y+pic->height > vid.height)
		fatal ("Draw_TransPic: bad coordinates");

	source = pic->pixels;
	dest = vid.buffer + y * vid.rowbytes + x;
	if(pic->width & 7){	// general
		for(v=0; v<pic->height; v++){
			for(u=0; u<pic->width; u++)
				if(opaque(tpix = source[u]))
					dest[u] = tpix;
			dest += vid.rowbytes;
			source += pic->width;
		}
	}else{	// unwound
		for(v=0; v<pic->height; v++){
			for(u=0; u<pic->width; u+=8){
				if(opaque(tpix = source[u]))
					dest[u] = tpix;
				if(opaque(tpix = source[u+1]))
					dest[u+1] = tpix;
				if(opaque(tpix = source[u+2]))
					dest[u+2] = tpix;
				if(opaque(tpix = source[u+3]))
					dest[u+3] = tpix;
				if(opaque(tpix = source[u+4]))
					dest[u+4] = tpix;
				if(opaque(tpix = source[u+5]))
					dest[u+5] = tpix;
				if(opaque(tpix = source[u+6]))
					dest[u+6] = tpix;
				if(opaque(tpix = source[u+7]))
					dest[u+7] = tpix;
			}
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}


/*
=============
Draw_TransPicTranslate
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	pixel_t	*dest, *source, tpix;
	int				v, u;

	if (x < 0 || y < 0 || x+pic->width > vid.width || y+pic->height > vid.height)
		fatal ("Draw_TransPic: bad coordinates");

	source = pic->pixels;
	dest = vid.buffer + y * vid.rowbytes + x;
	if (pic->width & 7){	// general
		for(v=0; v<pic->height; v++){
			for(u=0; u<pic->width; u++)
				if(opaque(tpix = source[u]))
					dest[u] = tpix;
			dest += vid.rowbytes;
			source += pic->width;
		}
	}else{	// unwound
		for(v=0; v<pic->height; v++){
			for(u=0; u<pic->width; u+=8){
				if(opaque(tpix = source[u]))
					dest[u] = tpix;
				if(opaque(tpix = source[u+1]))
					dest[u+1] = tpix;
				if(opaque(tpix = source[u+2]))
					dest[u+2] = tpix;
				if(opaque(tpix = source[u+3]))
					dest[u+3] = tpix;
				if(opaque(tpix = source[u+4]))
					dest[u+4] = tpix;
				if(opaque(tpix = source[u+5]))
					dest[u+5] = tpix;
				if(opaque(tpix = source[u+6]))
					dest[u+6] = tpix;
				if(opaque(tpix = source[u+7]))
					dest[u+7] = tpix;
			}
			dest += vid.rowbytes;
			source += pic->width;
		}
	}
}


static void Draw_CharToConback (int num, pixel_t *dest)
{
	int row, col, drawline, x;
	pixel_t	*source;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if((source[x] & 0xffffff) != 0)
				dest[x] = source[x];
		source += 128;
		dest += 320;
	}

}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	int				x, y, v, n;
	pixel_t			*src, *dest;
	int				f, fstep;
	qpic_t			*conback;
	char			ver[100];

	conback = Draw_CachePic ("gfx/conback.lmp");

	// hack the version number directly into the pic
	sprint (ver, "(9)quake %4.2f", (float)VERSION);
	dest = conback->pixels + 320*186 + 320 - 11 - 8*strlen(ver);

	n = strlen(ver);
	for (x=0 ; x<n ; x++)
		Draw_CharToConback (ver[x], dest+(x<<3));
	dest = vid.conbuffer;
	for(y=0; y<lines; y++, dest+=vid.conrowbytes){
		v = (vid.conheight - lines + y) * 200 / vid.conheight;
		src = conback->pixels + v * 320;
		if(vid.conwidth == 320)
			memcpy(dest, src, vid.conwidth*sizeof(pixel_t));
		else{
			f = 0;
			fstep = 320 * 0x10000 / vid.conwidth;
			for(x=0; x<(int)vid.conwidth; x+=4){
				dest[x] = src[f>>16]; f += fstep;
				dest[x+1] = src[f>>16]; f += fstep;
				dest[x+2] = src[f>>16]; f += fstep;
				dest[x+3] = src[f>>16]; f += fstep;
			}
		}
	}
}


/*
==============
R_DrawRect8
==============
*/
void R_DrawRect8 (vrect_t *prect, int rowbytes, pixel_t *psrc, int transparent)
{
	pixel_t	t, *pdest;
	int		i, j, srcdelta, destdelta;

	pdest = vid.buffer + (prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes - prect->width;

	if (transparent)
	{
		for (i=0 ; i<prect->height ; i++){
			for (j=0 ; j<prect->width ; j++){
				if(opaque(t = *psrc))
					*pdest = t;
				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}else{
		for (i=0 ; i<prect->height ; i++){
			memcpy (pdest, psrc, prect->width*sizeof(pixel_t));
			psrc += rowbytes;
			pdest += vid.rowbytes;
		}
	}
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	int				width, height, tileoffsetx, tileoffsety;
	pixel_t			*psrc;
	vrect_t			vr;

	r_rectdesc.rect.x = x;
	r_rectdesc.rect.y = y;
	r_rectdesc.rect.width = w;
	r_rectdesc.rect.height = h;

	vr.y = r_rectdesc.rect.y;
	height = r_rectdesc.rect.height;

	tileoffsety = vr.y % r_rectdesc.height;

	while (height > 0)
	{
		vr.x = r_rectdesc.rect.x;
		width = r_rectdesc.rect.width;

		if (tileoffsety != 0)
			vr.height = r_rectdesc.height - tileoffsety;
		else
			vr.height = r_rectdesc.height;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0)
		{
			if (tileoffsetx != 0)
				vr.width = r_rectdesc.width - tileoffsetx;
			else
				vr.width = r_rectdesc.width;

			if (vr.width > width)
				vr.width = width;

			psrc = r_rectdesc.ptexpixels +
					(tileoffsety * r_rectdesc.rowbytes) + tileoffsetx;
			R_DrawRect8(&vr, r_rectdesc.rowbytes, psrc, 0);
			vr.x += vr.width;
			width -= vr.width;
			tileoffsetx = 0;	// only the left tile can be left-clipped
		}

		vr.y += vr.height;
		height -= vr.height;
		tileoffsety = 0;		// only the top tile can be top-clipped
	}
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, pixel_t c)
{
	pixel_t			*dest;
	int				u, v;

	dest = vid.buffer + y*vid.rowbytes + x;
	for(v=0; v<h; v++, dest+=vid.rowbytes)
		for(u=0; u<w; u++)
			dest[u] = c;
}

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	int			x,y;
	pixel_t		*pbuf;

	for (y=0 ; y<vid.height ; y++)
	{
		int	t;

		pbuf = vid.buffer + vid.rowbytes*y;
		t = (y & 1) << 1;

		for (x=0 ; x<vid.width ; x++)
		{
			if ((x & 3) != t)
				pbuf[x] = 0;
		}
	}
}
