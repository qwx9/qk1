#include "quakedef.h"

#define MAXLEFTCLIPEDGES		100

// !!! if these are changed, they must be changed in asm_draw.h too !!!
#define FULLY_CLIPPED_CACHED	0x80000000
#define FRAMECOUNT_MASK			0x7FFFFFFF

static unsigned int cacheoffset;

int			c_faceclip;					// number of faces clipped

int r_drawflags;

clipplane_t	view_clipplanes[4];

static medge_t			*r_pedge;

static bool r_leftclipped, r_rightclipped;
static bool makeleftedge, makerightedge;
static bool r_nearzionly;

int		sintable[SIN_BUFFER_SIZE];
int		intsintable[SIN_BUFFER_SIZE];

static mvertex_t	r_leftenter, r_leftexit;
static mvertex_t	r_rightenter, r_rightexit;

typedef struct
{
	float	u,v;
	int		ceilv;
} evert_t;

static int r_emitted;
static float r_nearzi;
static float r_u1, r_v1, r_lzi1;
static int r_ceilv1;

static bool	r_lastvertvalid;

int
surfdrawflags(int flags)
{
	if(flags & SURF_TRANS){
		if((flags & SURF_FENCE) == 0 && alphafor(flags) >= 1.0f)
			return 0;
		return DRAW_BLEND;
	}
	return 0;
}

int
entdrawflags(entity_t *e)
{
	if(e != nil){
		if(e->effects & EF_NODRAW)
			return DRAW_NO;
		if(!defalpha(e->alpha))
			return DRAW_BLEND;
		if(e->model != nil && e->model != cl.worldmodel && e->model->blend)
			return DRAW_BLEND;
	}
	return 0;
}

/*
================
R_EmitEdge
================
*/
void R_EmitEdge (mvertex_t *pv0, mvertex_t *pv1)
{
	edge_t	*edge, *pcheck;
	int		u_check;
	float	u, u_step;
	vec3_t	local, transformed;
	float	*world;
	int		v, v2, ceilv0;
	float	lzi0, u0, v0;

	if (r_lastvertvalid)
	{
		u0 = r_u1;
		v0 = r_v1;
		lzi0 = r_lzi1;
		ceilv0 = r_ceilv1;
	}
	else
	{
		world = &pv0->position[0];

		// transform and project
		VectorSubtract (world, modelorg, local);
		TransformVector (local, transformed);

		if (transformed[2] < NEAR_CLIP)
			transformed[2] = NEAR_CLIP;

		lzi0 = 1.0 / transformed[2];

		// FIXME: build x/yscale into transform?
		u0 = xcenter + xscale*lzi0*transformed[0];
		u0 = clamp(u0, r_refdef.fvrectx_adj, r_refdef.fvrectright_adj);
		v0 = ycenter - yscale*lzi0*transformed[1];
		v0 = clamp(v0, r_refdef.fvrecty_adj, r_refdef.fvrectbottom_adj);
		ceilv0 = (int)ceil(v0);
	}

	world = &pv1->position[0];

	// transform and project
	VectorSubtract (world, modelorg, local);
	TransformVector (local, transformed);

	if (transformed[2] < NEAR_CLIP)
		transformed[2] = NEAR_CLIP;

	r_lzi1 = 1.0 / transformed[2];

	r_u1 = xcenter + xscale*r_lzi1*transformed[0];
	r_u1 = clamp(r_u1, r_refdef.fvrectx_adj, r_refdef.fvrectright_adj);
	r_v1 = ycenter - yscale*r_lzi1*transformed[1];
	r_v1 = clamp(r_v1, r_refdef.fvrecty_adj, r_refdef.fvrectbottom_adj);
	if (r_lzi1 > lzi0)
		lzi0 = r_lzi1;
	if (lzi0 > r_nearzi)	// for mipmap finding
		r_nearzi = lzi0;

	// for right edges, all we want is the effect on 1/z
	if (r_nearzionly)
		return;

	r_emitted = 1;
	r_ceilv1 = (int)ceil(r_v1);


	// create the edge
	if (ceilv0 == r_ceilv1)
	{
		// we cache unclipped horizontal edges as fully clipped
		if (cacheoffset != 0x7FFFFFFF)
		{
			cacheoffset = FULLY_CLIPPED_CACHED |
					(r_framecount & FRAMECOUNT_MASK);
		}

		return;		// horizontal edge
	}

	edge = edge_p++;
	edge->owner = r_pedge;
	edge->nearzi = lzi0;

	if (ceilv0 <= r_ceilv1)
	{
		// trailing edge (go from p1 to p2)
		v = ceilv0;
		v2 = r_ceilv1 - 1;

		edge->surfs[0] = surface_p - surfaces;
		edge->surfs[1] = 0;

		u_step = ((r_u1 - u0) / (r_v1 - v0));
		u = u0 + ((float)v - v0) * u_step;
	}
	else
	{
		// leading edge (go from p2 to p1)
		v2 = ceilv0 - 1;
		v = r_ceilv1;

		edge->surfs[0] = 0;
		edge->surfs[1] = surface_p - surfaces;

		u_step = (u0 - r_u1) / (v0 - r_v1);
		u = r_u1 + ((float)v - r_v1) * u_step;
	}

	edge->u_step = u_step*0x100000;
	edge->u = u*0x100000 + 0xFFFFF;
	/* with all the clamping before this one shouldn't be needed but kept to be 100% sure */
	edge->u = clamp(edge->u, r_refdef.vrect_x_adj_shift20, r_refdef.vrectright_adj_shift20);

	// sort the edge in normally
	u_check = edge->u;
	if (edge->surfs[0])
		u_check++;	// sort trailers after leaders

	if (!newedges[v] || newedges[v]->u >= u_check)
	{
		edge->next = newedges[v];
		newedges[v] = edge;
	}
	else
	{
		pcheck = newedges[v];
		while (pcheck->next && pcheck->next->u < u_check)
			pcheck = pcheck->next;
		edge->next = pcheck->next;
		pcheck->next = edge;
	}

	edge->nextremove = removeedges[v2];
	removeedges[v2] = edge;
}


/*
================
R_ClipEdge
================
*/
void R_ClipEdge (mvertex_t *pv0, mvertex_t *pv1, clipplane_t *clip)
{
	float		d0, d1, f;
	mvertex_t	clipvert;

	for(; clip != nil; clip = clip->next){
		d0 = DotProduct (pv0->position, clip->normal) - clip->dist;
		d1 = DotProduct (pv1->position, clip->normal) - clip->dist;

		if (d0 >= 0)
		{
			// point 0 is unclipped
			if (d1 >= 0)
			{
			// both points are unclipped
				continue;
			}

			// only point 1 is clipped

			// we don't cache clipped edges
			cacheoffset = 0x7FFFFFFF;

			f = d0 / (d0 - d1);
			clipvert.position[0] = pv0->position[0] +
					f * (pv1->position[0] - pv0->position[0]);
			clipvert.position[1] = pv0->position[1] +
					f * (pv1->position[1] - pv0->position[1]);
			clipvert.position[2] = pv0->position[2] +
					f * (pv1->position[2] - pv0->position[2]);

			if (clip->leftedge)
			{
				r_leftclipped = true;
				r_leftexit = clipvert;
			}
			else if (clip->rightedge)
			{
				r_rightclipped = true;
				r_rightexit = clipvert;
			}

			R_ClipEdge (pv0, &clipvert, clip->next);
			return;
		}
		else
		{
			// point 0 is clipped
			if (d1 < 0)
			{
				// both points are clipped
				// we do cache fully clipped edges
				if (!r_leftclipped)
					cacheoffset = FULLY_CLIPPED_CACHED |
							(r_framecount & FRAMECOUNT_MASK);
				return;
			}

			// only point 0 is clipped
			r_lastvertvalid = false;

			// we don't cache partially clipped edges
			cacheoffset = 0x7FFFFFFF;

			f = d0 / (d0 - d1);
			clipvert.position[0] = pv0->position[0] +
					f * (pv1->position[0] - pv0->position[0]);
			clipvert.position[1] = pv0->position[1] +
					f * (pv1->position[1] - pv0->position[1]);
			clipvert.position[2] = pv0->position[2] +
					f * (pv1->position[2] - pv0->position[2]);

			if (clip->leftedge)
			{
				r_leftclipped = true;
				r_leftenter = clipvert;
			}
			else if (clip->rightedge)
			{
				r_rightclipped = true;
				r_rightenter = clipvert;
			}

			R_ClipEdge (&clipvert, pv1, clip->next);
			return;
		}
	}

	// add the edge
	R_EmitEdge (pv0, pv1);
}


/*
================
R_EmitCachedEdge
================
*/
void R_EmitCachedEdge (void)
{
	edge_t		*pedge_t;

	pedge_t = (edge_t *)((uintptr)r_edges + r_pedge->cachededgeoffset);

	if (!pedge_t->surfs[0])
		pedge_t->surfs[0] = surface_p - surfaces;
	else
		pedge_t->surfs[1] = surface_p - surfaces;

	if (pedge_t->nearzi > r_nearzi)	// for mipmap finding
		r_nearzi = pedge_t->nearzi;

	r_emitted = 1;
}

float alphafor(int flags);

/*
================
R_RenderFace
================
*/
int R_RenderFace (msurface_t *fa, int clipflags)
{
	int			i, lindex;
	unsigned	mask;
	mplane_t	*pplane;
	float		distinv;
	vec3_t		p_normal;
	medge_t		*pedges, tedge;
	clipplane_t	*pclip;

	if((surfdrawflags(fa->flags) | entdrawflags(currententity)) ^ r_drawflags)
		return 0;

	// skip out if no more surfs
	if (surface_p >= surf_max){
		r_outofsurfaces++;
		return 1;
	}

	// ditto if not enough edges left, or switch to auxedges if possible
	if (edge_p + fa->numedges + 4 >= edge_max){
		r_outofedges += fa->numedges;
		return 1;
	}

	c_faceclip++;

	// set up clip planes
	pclip = nil;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

	// push the edges through
	r_emitted = 0;
	r_nearzi = 0;
	r_nearzionly = false;
	makeleftedge = makerightedge = false;
	pedges = currententity->model->edges;
	r_lastvertvalid = false;

	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = currententity->model->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];

			// if the edge is cached, we can just reuse the edge
			if (!insubmodel)
			{
				if (r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED)
				{
					if ((int)(r_pedge->cachededgeoffset & FRAMECOUNT_MASK) == r_framecount)
					{
						r_lastvertvalid = false;
						continue;
					}
				}
				else
				{
					if ((((uintptr)edge_p - (uintptr)r_edges) > r_pedge->cachededgeoffset) &&
						(((edge_t *)((uintptr)r_edges + r_pedge->cachededgeoffset))->owner == r_pedge))
					{
						R_EmitCachedEdge ();
						r_lastvertvalid = false;
						continue;
					}
				}
			}

			// assume it's cacheable
			cacheoffset = (byte *)edge_p - (byte *)r_edges;
			r_leftclipped = r_rightclipped = false;
			R_ClipEdge (&r_pcurrentvertbase[r_pedge->v[0]],
						&r_pcurrentvertbase[r_pedge->v[1]],
						pclip);
			r_pedge->cachededgeoffset = cacheoffset;

			if (r_leftclipped)
				makeleftedge = true;
			if (r_rightclipped)
				makerightedge = true;
			r_lastvertvalid = true;
		}
		else
		{
			lindex = -lindex;
			r_pedge = &pedges[lindex];
			// if the edge is cached, we can just reuse the edge
			if (!insubmodel)
			{
				if (r_pedge->cachededgeoffset & FULLY_CLIPPED_CACHED)
				{
					if ((int)(r_pedge->cachededgeoffset & FRAMECOUNT_MASK) == r_framecount)
					{
						r_lastvertvalid = false;
						continue;
					}
				}
				else
				{
				// it's cached if the cached edge is valid and is owned
				// by this medge_t
					if ((((uintptr)edge_p - (uintptr)r_edges) > r_pedge->cachededgeoffset) &&
						(((edge_t *)((uintptr)r_edges + r_pedge->cachededgeoffset))->owner == r_pedge))
					{
						R_EmitCachedEdge ();
						r_lastvertvalid = false;
						continue;
					}
				}
			}

			// assume it's cacheable
			cacheoffset = (byte *)edge_p - (byte *)r_edges;
			r_leftclipped = r_rightclipped = false;
			R_ClipEdge (&r_pcurrentvertbase[r_pedge->v[1]],
						&r_pcurrentvertbase[r_pedge->v[0]],
						pclip);
			r_pedge->cachededgeoffset = cacheoffset;

			if (r_leftclipped)
				makeleftedge = true;
			if (r_rightclipped)
				makerightedge = true;
			r_lastvertvalid = true;
		}
	}

	// if there was a clip off the left edge, add that edge too
	// FIXME: faster to do in screen space?
	// FIXME: share clipped edges?
	if (makeleftedge)
	{
		r_pedge = &tedge;
		r_lastvertvalid = false;
		R_ClipEdge (&r_leftexit, &r_leftenter, pclip->next);
	}

	// if there was a clip off the right edge, get the right r_nearzi
	if (makerightedge)
	{
		r_pedge = &tedge;
		r_lastvertvalid = false;
		r_nearzionly = true;
		R_ClipEdge (&r_rightexit, &r_rightenter, view_clipplanes[1].next);
	}

	// if no edges made it out, return without posting the surface
	if (!r_emitted)
		return 1;

	surface_p->data = (void *)fa;
	surface_p->nearzi = r_nearzi;
	surface_p->flags = fa->flags;
	surface_p->insubmodel = insubmodel;
	surface_p->spanstate = 0;
	surface_p->entity = currententity;
	surface_p->key = r_currentkey++;
	surface_p->spans = nil;

	pplane = fa->plane;
	// FIXME: cache this?
	TransformVector (pplane->normal, p_normal);
	// FIXME: cache this?
	distinv = 1.0 / (pplane->dist - DotProduct (modelorg, pplane->normal));

	surface_p->d_zistepu = p_normal[0] * xscaleinv * distinv;
	surface_p->d_zistepv = -p_normal[1] * yscaleinv * distinv;
	surface_p->d_ziorigin = p_normal[2] * distinv -
			xcenter * surface_p->d_zistepu -
			ycenter * surface_p->d_zistepv;

	surface_p++;

	return 1;
}


/*
================
R_RenderBmodelFace
================
*/
void R_RenderBmodelFace (bedge_t *pedges, msurface_t *psurf)
{
	int			i;
	unsigned	mask;
	mplane_t	*pplane;
	float		distinv;
	vec3_t		p_normal;
	clipplane_t	*pclip;
	static medge_t tedge;

	// skip out if no more surfs
	if (surface_p >= surf_max)
	{
		r_outofsurfaces++;
		return;
	}

	// ditto if not enough edges left, or switch to auxedges if possible
	if ((edge_p + psurf->numedges + 4) >= edge_max)
	{
		r_outofedges += psurf->numedges;
		return;
	}

	c_faceclip++;

	// this is a dummy to give the caching mechanism someplace to write to
	r_pedge = &tedge;

	// set up clip planes
	pclip = nil;

	for (i=3, mask = 0x08 ; i>=0 ; i--, mask >>= 1)
	{
		if (r_clipflags & mask)
		{
			view_clipplanes[i].next = pclip;
			pclip = &view_clipplanes[i];
		}
	}

	// push the edges through
	r_emitted = 0;
	r_nearzi = 0;
	r_nearzionly = false;
	makeleftedge = makerightedge = false;
	// FIXME: keep clipped bmodel edges in clockwise order so last vertex caching
	// can be used?
	r_lastvertvalid = false;

	for ( ; pedges ; pedges = pedges->pnext)
	{
		r_leftclipped = r_rightclipped = false;
		R_ClipEdge (pedges->v[0], pedges->v[1], pclip);

		if (r_leftclipped)
			makeleftedge = true;
		if (r_rightclipped)
			makerightedge = true;
	}

	// if there was a clip off the left edge, add that edge too
	// FIXME: faster to do in screen space?
	// FIXME: share clipped edges?
	if (makeleftedge)
	{
		r_pedge = &tedge;
		R_ClipEdge (&r_leftexit, &r_leftenter, pclip->next);
	}

	// if there was a clip off the right edge, get the right r_nearzi
	if (makerightedge)
	{
		r_pedge = &tedge;
		r_nearzionly = true;
		R_ClipEdge (&r_rightexit, &r_rightenter, view_clipplanes[1].next);
	}

	// if no edges made it out, return without posting the surface
	if (!r_emitted)
		return;

	surface_p->data = (void *)psurf;
	surface_p->nearzi = r_nearzi;
	surface_p->flags = psurf->flags;
	surface_p->insubmodel = true;
	surface_p->spanstate = 0;
	surface_p->entity = currententity;
	surface_p->key = r_currentbkey;
	surface_p->spans = nil;

	pplane = psurf->plane;
	// FIXME: cache this?
	TransformVector (pplane->normal, p_normal);
	// FIXME: cache this?
	distinv = 1.0 / (pplane->dist - DotProduct (modelorg, pplane->normal));

	surface_p->d_zistepu = p_normal[0] * xscaleinv * distinv;
	surface_p->d_zistepv = -p_normal[1] * yscaleinv * distinv;
	surface_p->d_ziorigin = p_normal[2] * distinv -
			xcenter * surface_p->d_zistepu -
			ycenter * surface_p->d_zistepv;

	surface_p++;
}
