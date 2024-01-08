#include "quakedef.h"

/*
===============
R_AddDynamicLights
===============
*/
static void
R_AddDynamicLights(entity_t *e, drawsurf_t *ds)
{
	msurface_t *ms;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local, entorigin;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	ms = ds->m;
	smax = (ms->extents[0]>>4)+1;
	tmax = (ms->extents[1]>>4)+1;
	tex = ms->texinfo;

	for(lnum = 0; lnum < MAX_DLIGHTS; lnum++){
		if((ms->dlightbits & (1<<lnum)) == 0)
			continue; // not lit by this light

		rad = cl_dlights[lnum].radius;
		VectorSubtract(cl_dlights[lnum].origin, e->origin, entorigin);
		dist = DotProduct(entorigin, ms->plane->normal) - ms->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if(rad < minlight)
			continue;
		minlight = rad - minlight;

		for(i = 0; i < 3; i++)
			impact[i] = entorigin[i] - ms->plane->normal[i]*dist;

		local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= ms->texturemins[0];
		local[1] -= ms->texturemins[1];

		for(t = 0; t < tmax; t++){
			td = local[1] - t*16;
			if(td < 0)
				td = -td;
			for(s = 0; s < smax ; s++){
				sd = local[0] - s*16;
				if(sd < 0)
					sd = -sd;
				if(sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if(dist < minlight){
					ds->blocklights[0][t*smax + s] += (rad - dist)*256;
					ds->blocklights[1][t*smax + s] += (rad - dist)*256;
					ds->blocklights[2][t*smax + s] += (rad - dist)*256;
				}
			}
		}
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
static void
R_BuildLightMap(entity_t *e, drawsurf_t *ds)
{
	int			smax, tmax;
	int			t;
	int			i, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	msurface_t	*ms;

	ms = ds->m;
	smax = (ms->extents[0]>>4)+1;
	tmax = (ms->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = ms->samples;

	if(r_fullbright.value || !cl.worldmodel->lightdata){
		memset(ds->blocklights, 0, sizeof(ds->blocklights));
		return;
	}

	// clear to ambient
	for(i = 0; i < size; i++){
		ds->blocklights[0][i] = r_refdef.ambientlight[0]<<8;
		ds->blocklights[1][i] = r_refdef.ambientlight[1]<<8;
		ds->blocklights[2][i] = r_refdef.ambientlight[2]<<8;
	}

	// add all the lightmaps
	if(lightmap){
		for(maps = 0; maps < MAXLIGHTMAPS && ms->styles[maps] != 255; maps++){
			scale = ds->lightadj[maps];	// 8.8 fraction
			for(i = 0; i < size; i++){
				ds->blocklights[0][i] += lightmap[i*3+0] * scale;
				ds->blocklights[1][i] += lightmap[i*3+1] * scale;
				ds->blocklights[2][i] += lightmap[i*3+2] * scale;
			}
			lightmap += size * 3;	// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if(ms->dlightframe == r_framecount)
		R_AddDynamicLights(e, ds);

	// bound, invert, and shift
	for(i = 0; i < size; i++){
		t = (255*256 - (int)ds->blocklights[0][i]) >> (8 - VID_CBITS);
		ds->blocklights[0][i] = max(t, (1<<6));
		t = (255*256 - (int)ds->blocklights[1][i]) >> (8 - VID_CBITS);
		ds->blocklights[1][i] = max(t, (1<<6));
		t = (255*256 - (int)ds->blocklights[2][i]) >> (8 - VID_CBITS);
		ds->blocklights[2][i] = max(t, (1<<6));
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *
R_TextureAnimation(entity_t *e, texture_t *base)
{
	int relative, count;

	if(e->frame && base->alternate_anims)
		base = base->alternate_anims;

	if(!base->anim_total)
		return base;

	relative = (int)(cl.time*10) % base->anim_total;

	count = 0;
	while(base->anim_min > relative || base->anim_max <= relative){
		base = base->anim_next;
		if(!base)
			fatal("R_TextureAnimation: broken cycle");
		if(++count > 100)
			fatal("R_TextureAnimation: infinite cycle");
	}

	return base;
}

#define fullbright 1
#define additive 1
#define addlight addlight_f1_a1
#define DrawSurfaceBlock_m0 DrawSurfaceBlock_f1_a1_m0
#define DrawSurfaceBlock_m1 DrawSurfaceBlock_f1_a1_m1
#define DrawSurfaceBlock_m2 DrawSurfaceBlock_f1_a1_m2
#define DrawSurfaceBlock_m3 DrawSurfaceBlock_f1_a1_m3
#include "r_surf_x.h"
#undef fullbright
#undef additive
#undef addlight
#undef DrawSurfaceBlock_m0
#undef DrawSurfaceBlock_m1
#undef DrawSurfaceBlock_m2
#undef DrawSurfaceBlock_m3

#define fullbright 0
#define additive 1
#define addlight addlight_f0_a1
#define DrawSurfaceBlock_m0 DrawSurfaceBlock_f0_a1_m0
#define DrawSurfaceBlock_m1 DrawSurfaceBlock_f0_a1_m1
#define DrawSurfaceBlock_m2 DrawSurfaceBlock_f0_a1_m2
#define DrawSurfaceBlock_m3 DrawSurfaceBlock_f0_a1_m3
#include "r_surf_x.h"
#undef fullbright
#undef additive
#undef addlight
#undef DrawSurfaceBlock_m0
#undef DrawSurfaceBlock_m1
#undef DrawSurfaceBlock_m2
#undef DrawSurfaceBlock_m3

#define fullbright 1
#define additive 0
#define addlight addlight_f1_a0
#define DrawSurfaceBlock_m0 DrawSurfaceBlock_f1_a0_m0
#define DrawSurfaceBlock_m1 DrawSurfaceBlock_f1_a0_m1
#define DrawSurfaceBlock_m2 DrawSurfaceBlock_f1_a0_m2
#define DrawSurfaceBlock_m3 DrawSurfaceBlock_f1_a0_m3
#include "r_surf_x.h"
#undef fullbright
#undef additive
#undef addlight
#undef DrawSurfaceBlock_m0
#undef DrawSurfaceBlock_m1
#undef DrawSurfaceBlock_m2
#undef DrawSurfaceBlock_m3

#define fullbright 0
#define additive 0
#define addlight addlight_f0_a0
#define DrawSurfaceBlock_m0 DrawSurfaceBlock_f0_a0_m0
#define DrawSurfaceBlock_m1 DrawSurfaceBlock_f0_a0_m1
#define DrawSurfaceBlock_m2 DrawSurfaceBlock_f0_a0_m2
#define DrawSurfaceBlock_m3 DrawSurfaceBlock_f0_a0_m3
#include "r_surf_x.h"
#undef fullbright
#undef additive
#undef addlight
#undef DrawSurfaceBlock_m0
#undef DrawSurfaceBlock_m1
#undef DrawSurfaceBlock_m2
#undef DrawSurfaceBlock_m3

typedef void (*drawfunc)(pixel_t *psource, pixel_t *prowdest, pixel_t *sourcemax, int deststep, int tstep, int stepback, unsigned *lp[4], unsigned lw, int nb);

static const drawfunc drawsurf[2/*fullbright*/][2/*additive*/][4/*mipmap*/] = {
	{
		{
			DrawSurfaceBlock_f0_a0_m0, DrawSurfaceBlock_f0_a0_m1,
			DrawSurfaceBlock_f0_a0_m2, DrawSurfaceBlock_f0_a0_m3,
		},
		{
			DrawSurfaceBlock_f0_a1_m0, DrawSurfaceBlock_f0_a1_m1,
			DrawSurfaceBlock_f0_a1_m2, DrawSurfaceBlock_f0_a1_m3,
		},
	},
	{
		{
			DrawSurfaceBlock_f1_a0_m0, DrawSurfaceBlock_f1_a0_m1,
			DrawSurfaceBlock_f1_a0_m2, DrawSurfaceBlock_f1_a0_m3,
		},
		{
			DrawSurfaceBlock_f1_a1_m0, DrawSurfaceBlock_f1_a1_m1,
			DrawSurfaceBlock_f1_a1_m2, DrawSurfaceBlock_f1_a1_m3,
		},
	},
};

/*
===============
R_DrawSurface
===============
*/
void
R_DrawSurface(entity_t *e, drawsurf_t *ds)
{
	pixel_t	*basetptr;
	int				smax, tmax, twidth, lightwidth;
	int				u, blockdivshift, blocksize;
	int				soffset, basetoffset, texwidth;
	int				horzblockstep;
	int				r_numhblocks, r_numvblocks;
	pixel_t	*pcolumndest;
	texture_t		*mt;
	msurface_t		*ms;
	drawfunc draw;
	unsigned *lp[3];
	int sourcetstep, stepback;
	pixel_t *source, *sourcemax;

	// calculate the lightings
	R_BuildLightMap(e, ds);

	mt = ds->texture;
	ms = ds->m;

	draw = drawsurf[mt->drawsurf][e != nil && (e->effects & EF_ADDITIVE) != 0][ds->mip];

	source = mt->pixels + mt->offsets[ds->mip];

	// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
	// from a source range of 0 - 255

	texwidth = mt->width >> ds->mip;

	blocksize = 16 >> ds->mip;
	blockdivshift = 4 - ds->mip;

	lightwidth = (ms->extents[0]>>4)+1;

	r_numhblocks = ds->width >> blockdivshift;
	r_numvblocks = ds->height >> blockdivshift;

	// TODO: only needs to be set when there is a display settings change
	horzblockstep = blocksize;

	smax = mt->width >> ds->mip;
	twidth = texwidth;
	tmax = mt->height >> ds->mip;
	sourcetstep = texwidth;
	stepback = tmax * twidth;
	sourcemax = source + (tmax * smax);

	soffset = ms->texturemins[0];
	basetoffset = ms->texturemins[1];

	// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> ds->mip) + (smax << 16)) % smax;
	basetptr = &source[((((basetoffset >> ds->mip) + (tmax << 16)) % tmax) * twidth)];

	for(u = 0, pcolumndest = ds->dat; u < r_numhblocks; u++, pcolumndest += horzblockstep){
		lp[0] = ds->blocklights[0]+u;
		lp[1] = ds->blocklights[1]+u;
		lp[2] = ds->blocklights[2]+u;

		draw(
			pcolumndest,
			basetptr+soffset, sourcemax,
			ds->width, sourcetstep, stepback,
			lp, lightwidth, r_numvblocks
		);

		soffset = soffset + blocksize;
		if(soffset >= smax)
			soffset = 0;
	}
}

pixel_t
addlight(entity_t *e, pixel_t x, int lr, int lg, int lb)
{
	int r, g, b;

	if((x & 0xff000000U) == 0)
		return x;

	if(e != nil && (e->effects & EF_ADDITIVE) != 0)
		return x;

	r = (x>>16) & 0xff;
	g = (x>>8)  & 0xff;
	b = (x>>0)  & 0xff;

	r = (r * ((64<<8)-(lr & 0xffff))) >> (8+VID_CBITS);
	g = (g * ((64<<8)-(lg & 0xffff))) >> (8+VID_CBITS);
	b = (b * ((64<<8)-(lb & 0xffff))) >> (8+VID_CBITS);
	x = (x & 0xff000000) | r<<16 | g<<8 | b<<0;

	return x;
}
