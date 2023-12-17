#include "quakedef.h"

drawsurf_t	r_drawsurf;

static int sourcetstep;
static int surfrowbytes;	// used by ASM files
static int r_stepback;
static int r_numhblocks, r_numvblocks;
static pixel_t *r_source, *r_sourcemax;
static unsigned *r_lightptr[3];
static int r_lightwidth;
static pixel_t *pbasesource;
static void *prowdestbase;

static void R_DrawSurfaceBlock8(int mip);

static unsigned blocklights[3][18*18];

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (void)
{
	msurface_t *surf;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local, entorigin;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	surf = r_drawsurf.surf;
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		VectorSubtract(cl_dlights[lnum].origin, currententity->origin, entorigin);
		dist = DotProduct (entorigin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = entorigin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight){
					blocklights[0][t*smax + s] += (rad - dist)*256;
					blocklights[1][t*smax + s] += (rad - dist)*256;
					blocklights[2][t*smax + s] += (rad - dist)*256;
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
void R_BuildLightMap (void)
{
	int			smax, tmax;
	int			t;
	int			i, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	msurface_t	*surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		memset(blocklights, 0, sizeof(blocklights));
		return;
	}

	// clear to ambient
	for (i=0 ; i<size ; i++){
		blocklights[0][i] = r_refdef.ambientlight[0]<<8;
		blocklights[1][i] = r_refdef.ambientlight[1]<<8;
		blocklights[2][i] = r_refdef.ambientlight[2]<<8;
	}

	// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction
			for (i=0 ; i<size ; i++){
				blocklights[0][i] += lightmap[i*3+0] * scale;
				blocklights[1][i] += lightmap[i*3+1] * scale;
				blocklights[2][i] += lightmap[i*3+2] * scale;
			}
			lightmap += size * 3;	// skip to next lightmap
		}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights ();

	// bound, invert, and shift
	for (i=0 ; i<size ; i++)
	{
		t = (255*256 - (int)blocklights[0][i]) >> (8 - VID_CBITS);
		blocklights[0][i] = max(t, (1<<6));
		t = (255*256 - (int)blocklights[1][i]) >> (8 - VID_CBITS);
		blocklights[1][i] = max(t, (1<<6));
		t = (255*256 - (int)blocklights[2][i]) >> (8 - VID_CBITS);
		blocklights[2][i] = max(t, (1<<6));
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			fatal ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			fatal ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
===============
R_DrawSurface
===============
*/
void R_DrawSurface (void)
{
	pixel_t	*basetptr;
	int				smax, tmax, twidth;
	int				u, blockdivshift, blocksize;
	int				soffset, basetoffset, texwidth;
	int				horzblockstep;
	pixel_t	*pcolumndest;
	texture_t		*mt;

	// calculate the lightings
	R_BuildLightMap ();

	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;

	r_source = mt->pixels + mt->offsets[r_drawsurf.surfmip];

	// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
	// from a source range of 0 - 255

	texwidth = mt->width >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;

	r_lightwidth = (r_drawsurf.surf->extents[0]>>4)+1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

	// TODO: only needs to be set when there is a display settings change
	horzblockstep = blocksize;

	smax = mt->width >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0];
	basetoffset = r_drawsurf.surf->texturemins[1];

	// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip)
		+ (tmax << 16)) % tmax) * twidth)];

	pcolumndest = r_drawsurf.surfdat;

	for (u=0 ; u<r_numhblocks; u++)
	{
		r_lightptr[0] = blocklights[0] + u;
		r_lightptr[1] = blocklights[1] + u;
		r_lightptr[2] = blocklights[2] + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		R_DrawSurfaceBlock8(r_drawsurf.surfmip);

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}
}


//=============================================================================

inline pixel_t
addlight(pixel_t x, int lr, int lg, int lb)
{
	int r, g, b;

	if((x & 0xff000000U) == 0)
		return x;

	if(currententity != nil && (currententity->effects & EF_ADDITIVE) != 0)
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

static void
R_DrawSurfaceBlock8(int mip)
{
	int b, v, i, j, lightstep[3], light[3], lightleft[3], lightright[3];
	int lightleftstep[3], lightrightstep[3];
	pixel_t	*psource, *prowdest;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
		for(j = 0; j < 3; j++){
			lightleft[j] = r_lightptr[j][0];
			lightright[j] = r_lightptr[j][1];
			r_lightptr[j] += r_lightwidth;
			lightleftstep[j] = (r_lightptr[j][0] - lightleft[j]) >> (4-mip);
			lightrightstep[j] = (r_lightptr[j][1] - lightright[j]) >> (4-mip);
		}

		for (i=0 ; i<16>>mip ; i++)
		{
			for(j = 0; j < 3; j++){
				lightstep[j] = (lightleft[j] - lightright[j]) >> (4-mip);
				light[j] = lightright[j];
			}

			for(b = (16>>mip)-1; b >= 0; b--){
				prowdest[b] = addlight(psource[b], light[0], light[1], light[2]);
				light[0] += lightstep[0];
				light[1] += lightstep[1];
				light[2] += lightstep[2];
			}

			psource += sourcetstep;
			prowdest += surfrowbytes;
			for(j = 0; j < 3; j++){
				lightright[j] += lightrightstep[j];
				lightleft[j] += lightleftstep[j];
			}
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}
