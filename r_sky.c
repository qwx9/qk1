#include "quakedef.h"

static int iskyspeed = 8;
static int iskyspeed2 = 2;
static float skyspeed2;
static int r_skymade;

static void
skyrecalc(void)
{
	skyvars.smask = (skyvars.w - 1)<<16;
	skyvars.tmask = (skyvars.h - 1)<<16;
	skyvars.tshift = 16 - qctz(skyvars.w);
}

/*
=============
R_InitSky
==============
*/
void R_InitSky (texture_t *mt)
{
	int x, y, w, n;
	pixel_t *src, p;

	if(mt == nil){
		skyvars.w = r_notexture_mip->width;
		skyvars.h = r_notexture_mip->height;
		skyvars.source[0] = skyvars.source[1] = r_notexture_mip->pixels;
	}else{
		src = mt->pixels + mt->offsets[0];
		w = mt->width;
		skyvars.h = mt->height;
		skyvars.w = w/2;
		n = skyvars.w*skyvars.h;
		skyvars.source[0] = Hunk_Alloc(n*sizeof(pixel_t)*2);
		skyvars.source[1] = skyvars.source[0] + n;
		for(y=0 ; y<skyvars.h; y++){
			for(x=0 ; x<skyvars.w; x++){
				skyvars.source[0][y*skyvars.w + x] = src[y*w + skyvars.w + x];
				if((p = src[y*w + x]) & 0xffffff)
					skyvars.source[1][y*skyvars.w + x] = p;
			}
		}
	}
	skyrecalc();
}

/*
=============
R_SetSkyFrame
==============
*/
void R_SetSkyFrame (void)
{
	int		g, s1, s2;
	float	temp;

	skyvars.speed = iskyspeed;
	skyspeed2 = iskyspeed2;

	g = GreatestCommonDivisor (iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = skyvars.h * s1 * s2;

	skyvars.time = cl.time - ((int)(cl.time / temp) * temp);

	r_skymade = 0;
}


