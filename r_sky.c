#include "quakedef.h"

static int iskyspeed = 8;
static int iskyspeed2 = 2;
static float skyspeed2;

float skyspeed, skytime;

pixel_t *r_skysource[2];

static int r_skymade;
int skyw, skyh;

/*
=============
R_InitSky
==============
*/
void R_InitSky (texture_t *mt)
{
	int x, y, w, n;
	pixel_t *src;

	src = (pixel_t*)((byte *)mt + mt->offsets[0]);
	w = mt->width;
	skyh = mt->height;
	if(w == skyh){ // probably without a mask?
		skyw = w;
		n = skyw*skyh;
		r_skysource[0] = Hunk_Alloc(n*sizeof(pixel_t));
		r_skysource[1] = r_skysource[0];
		memmove(r_skysource[0], src, n);
		return;
	}
	skyw = w/2;
	n = skyw*skyh;
	r_skysource[0] = Hunk_Alloc(n*sizeof(pixel_t)*2);
	r_skysource[1] = r_skysource[0] + n;
	for(y=0 ; y<skyh; y++){
		for(x=0 ; x<skyw; x++){
			r_skysource[0][y*skyw + x] = src[y*w + skyw + x];
			r_skysource[1][y*skyw + x] = src[y*w + x];
		}
	}
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

	skyspeed = iskyspeed;
	skyspeed2 = iskyspeed2;

	g = GreatestCommonDivisor (iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = skyh * s1 * s2;

	skytime = cl.time - ((int)(cl.time / temp) * temp);

	r_skymade = 0;
}


