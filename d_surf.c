#include "quakedef.h"

static float surfscale;
bool r_cache_thrash;		 // set if surface cache is thrashing

surfcache_t *sc_rover;

static surfcache_t *sc_base;
static int sc_size;

#define GUARDSIZE	   4

int	 D_SurfaceCacheForRes (int width, int height)
{
	int			 size, pix;

	size = SURFCACHE_SIZE_AT_320X200;

	pix = width*height*sizeof(pixel_t);
	if (pix > 64000)
		size += (pix-64000)*3;


	return size;
}

static void
D_CheckCacheGuard(void)
{
	byte	*s;
	int			 i;

	s = (byte *)sc_base + sc_size;
	for (i=0 ; i<GUARDSIZE ; i++)
		if (s[i] != (byte)i)
			fatal ("D_CheckCacheGuard: failed");
}

static void
D_ClearCacheGuard(void)
{
	byte	*s;
	int			 i;

	s = (byte *)sc_base + sc_size;
	for (i=0 ; i<GUARDSIZE ; i++)
		s[i] = (byte)i;
}


/*
================
D_InitCaches

================
*/
void D_InitCaches (void *buffer, int size)
{
	Con_Printf ("%dk surface cache\n", size/1024);

	sc_size = size - GUARDSIZE;
	sc_base = (surfcache_t *)buffer;
	sc_rover = sc_base;

	sc_base->next = nil;
	sc_base->owner = nil;
	sc_base->size = sc_size;

	D_ClearCacheGuard ();
}


/*
==================
D_FlushCaches
==================
*/
void D_FlushCaches (void)
{
	surfcache_t	 *c;

	if (!sc_base)
		return;

	for (c = sc_base ; c ; c = c->next)
	{
		if (c->owner)
			*c->owner = nil;
	}

	sc_rover = sc_base;
	sc_base->next = nil;
	sc_base->owner = nil;
	sc_base->size = sc_size;
}

/*
=================
D_SCAlloc
=================
*/
static surfcache_t *
D_SCAlloc(int width, int size)
{
	surfcache_t			 *new;
	bool				wrapped_this_time;

	if(width < 0 || size <= 0)
		Host_Error("D_SCAlloc: width=%d size=%d\n", width, size);

	size = (sizeof(surfcache_t) + size + 3) & ~3;
	if(size > sc_size)
		Host_Error("D_SCAlloc: %d > cache size",size);

	// if there is not size bytes after the rover, reset to the start
	wrapped_this_time = false;

	if ( !sc_rover || (intptr)sc_rover-(intptr)sc_base > (intptr)sc_size-(intptr)size)
	{
		if (sc_rover)
		{
			wrapped_this_time = true;
		}
		sc_rover = sc_base;
	}

	// colect and free surfcache_t blocks until the rover block is large enough
	new = sc_rover;
	if (sc_rover->owner)
		*sc_rover->owner = nil;

	while (new->size < size)
	{
		// free another
		sc_rover = sc_rover->next;
		if (!sc_rover)
			fatal ("D_SCAlloc: hit the end of memory");
		if (sc_rover->owner)
			*sc_rover->owner = nil;

		new->size += sc_rover->size;
		new->next = sc_rover->next;
	}

	// create a fragment out of any leftovers
	if (new->size - size > 256)
	{
		sc_rover = (surfcache_t *)( (byte *)new + size);
		sc_rover->size = new->size - size;
		sc_rover->next = new->next;
		sc_rover->width = 0;
		sc_rover->owner = nil;
		new->next = sc_rover;
		new->size = size;
	}
	else
		sc_rover = new->next;

	new->width = width;
	// DEBUG
	if (width > 0)
		new->height = (size - sizeof(*new)) / width / sizeof(pixel_t);

	new->owner = nil;			  // should be set properly after return

	if (d_roverwrapped)
	{
		if (wrapped_this_time || (sc_rover >= d_initial_rover))
			r_cache_thrash = true;
	}
	else if (wrapped_this_time)
	{
		d_roverwrapped = true;
	}

D_CheckCacheGuard ();   // DEBUG
	return new;
}

/*
================
D_CacheSurface
================
*/
surfcache_t *
D_CacheSurface(entity_t *e, msurface_t *ms, drawsurf_t *ds, int miplevel)
{
	surfcache_t *cache;

	// if the surface is animating or flashing, flush the cache
	ds->texture = R_TextureAnimation(e, ms->texinfo->texture);
	ds->lightadj[0] = d_lightstylevalue[ms->styles[0]];
	ds->lightadj[1] = d_lightstylevalue[ms->styles[1]];
	ds->lightadj[2] = d_lightstylevalue[ms->styles[2]];
	ds->lightadj[3] = d_lightstylevalue[ms->styles[3]];

	// see if the cache holds apropriate data
	cache = ms->cachespots[miplevel];

	if(e == cl_entities || e->model->name[0] == '*')
	if(cache && !cache->dlight && ms->dlightframe != r_framecount
			&& cache->texture == ds->texture
			&& cache->lightadj[0] == ds->lightadj[0]
			&& cache->lightadj[1] == ds->lightadj[1]
			&& cache->lightadj[2] == ds->lightadj[2]
			&& cache->lightadj[3] == ds->lightadj[3] )
		return cache;

	// determine shape of surface
	surfscale = 1.0 / (1<<miplevel);
	ds->mip = miplevel;
	ds->width = ms->extents[0] >> miplevel;
	ds->height = ms->extents[1] >> miplevel;

	// allocate memory if needed
	if(cache == nil){	 // if a texture just animated, don't reallocate it
		cache = D_SCAlloc(ds->width, ds->width * ds->height * sizeof(pixel_t));
		ms->cachespots[miplevel] = cache;
		cache->owner = &ms->cachespots[miplevel];
		cache->mipscale = surfscale;
	}

	// draw and light the surface texture
	cache->dlight = ms->dlightframe == r_framecount;
	ds->dat = cache->pixels;
	cache->texture = ds->texture;
	cache->lightadj[0] = ds->lightadj[0];
	cache->lightadj[1] = ds->lightadj[1];
	cache->lightadj[2] = ds->lightadj[2];
	cache->lightadj[3] = ds->lightadj[3];
	ds->m = ms;
	R_DrawSurface(e, ds);

	return ms->cachespots[miplevel];
}


