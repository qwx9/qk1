#include "quakedef.h"

static int iskyspeed = 8;
static int iskyspeed2 = 2;
static float skyspeed2;

/* FIXME TBD skyboxes
static int planes[6][2] = {
	{2, -128},
	{0, -128},
	{2, +128},
	{1, +128},
	{0, +128},
	{1, -128},
};
static int surfedges[6][4] = {
	{+1, +2, +3, +4},
	{-1, +5, +6, +7},
	{+8, +9, -6, +10},
	{-2, -7, -9, +11},
	{+12, -3, -11, -8},
	{-12, -10, -5, -4},
};
static int edges[12][2] = {
	{1, 2},
	{2, 3},
	{3, 4},
	{4, 1},
	{1, 5},
	{5, 6},
	{6, 2},
	{7, 8},
	{8, 6},
	{5, 7},
	{8, 3},
	{7, 4},
};
static int faces[6] = {0, 0, 2, 2, 2, 0};
static vec3_t vecs[6][2] = {
	{{0, -1, 0}, {-1, 0, 0}},
	{{0, +1, 0}, {0, 0, -1}},
	{{0, -1, 0}, {+1, 0, 0}},
	{{+1, 0, 0}, {0, 0, -1}},
	{{0, -1, 0}, {0, 0, -1}},
	{{-1, 0, 0}, {0, 0, -1}},
};
static vec3_t verts[8] = {
	{-1, -1, -1},
	{-1, +1, -1},
	{+1, +1, -1},
	{+1, -1, -1},
	{-1, -1, +1},
	{-1, +1, +1},
	{+1, -1, +1},
	{+1, +1, +1},
};

static msurface_t r_skyfaces[6];
static mvertex_t r_skyverts[8];
static medge_t r_skyedges[12];
static mtexinfo_t r_skytexinfo[6];
static mplane_t r_skyplanes[6];
static int r_skysurfedges[6 * 4];
*/

static void
skyrecalc(void)
{
	skyvars.smask = (skyvars.w - 1) << 16;
	skyvars.tmask = (skyvars.h - 1) << 16;
	skyvars.tshift = 16 - qctz(skyvars.w);
}

void
R_InitSky(texture_t *mt)
{
	int x, y, w, n;
	pixel_t *src, p;

	if(mt == nil) {
		skyvars.w = r_notexture_mip->width;
		skyvars.h = r_notexture_mip->height;
		skyvars.source[0] = skyvars.source[1] = r_notexture_mip->pixels;
	} else {
		src = mt->pixels + mt->offsets[0];
		w = mt->width;
		skyvars.h = mt->height;
		skyvars.w = w / 2;
		n = skyvars.w * skyvars.h;
		skyvars.source[0] = Hunk_Alloc(n * sizeof(pixel_t) * 2);
		skyvars.source[1] = skyvars.source[0] + n;
		for(y = 0; y < skyvars.h; y++) {
			for(x = 0; x < skyvars.w; x++) {
				skyvars.source[0][y * skyvars.w + x] = src[y * w + skyvars.w + x];
				if((p = src[y * w + x]) & 0xffffff)
					skyvars.source[1][y * skyvars.w + x] = p;
			}
		}
	}
	skyrecalc();
}

void
R_SetSkyFrame(void)
{
	int g, s1, s2;
	float temp;

	skyvars.speed = iskyspeed;
	skyspeed2 = iskyspeed2;

	g = GreatestCommonDivisor(iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = skyvars.h * s1 * s2;

	skyvars.time = cl.time - ((int)(cl.time / temp) * temp);
}
