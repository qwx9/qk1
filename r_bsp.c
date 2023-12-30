#include "quakedef.h"

//
// current entity info
//
bool		insubmodel;
entity_t		*currententity;
vec3_t			modelorg, base_modelorg;
								// modelorg is the viewpoint reletive to
								// the currently rendering entity
vec3_t			r_entorigin;	// the currently rendering entity in world
								// coordinates

static float	entity_rotation[3][3];

int				r_currentbkey;

typedef struct nodereject_t nodereject_t;

struct nodereject_t {
	mnode_t *node;
	int clipflags;
};

static nodereject_t *node_rejects;
static int num_node_rejects, max_node_rejects = 128;

typedef enum {touchessolid, drawnode, nodrawnode} solidstate_t;

#define MAX_BMODEL_VERTS	6000
#define MAX_BMODEL_EDGES	12000
static mvertex_t bverts[MAX_BMODEL_VERTS];
static bedge_t bedges[MAX_BMODEL_EDGES];

static mvertex_t	*pbverts;
static bedge_t		*pbedges;
static int			numbverts, numbedges;

static mvertex_t	*pfrontenter, *pfrontexit;

static bool		makeclippededge;


//===========================================================================

/*
================
R_EntityRotate
================
*/
void R_EntityRotate (vec3_t vec)
{
	vec3_t	tvec;

	VectorCopy (vec, tvec);
	vec[0] = DotProduct (entity_rotation[0], tvec);
	vec[1] = DotProduct (entity_rotation[1], tvec);
	vec[2] = DotProduct (entity_rotation[2], tvec);
}


/*
================
R_RotateBmodel
================
*/
void R_RotateBmodel (entity_t *e)
{
	float	angle, s, c, temp1[3][3], temp2[3][3], temp3[3][3];

	// TODO: should use a look-up table
	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: could cache lazily, stored in the entity
	// TODO: share work with R_SetUpAliasTransform

	// yaw
	angle = e->angles[YAW];
	angle = angle * M_PI*2 / 360;
	s = sinf(angle);
	c = cosf(angle);

	temp1[0][0] = c;
	temp1[0][1] = s;
	temp1[0][2] = 0;
	temp1[1][0] = -s;
	temp1[1][1] = c;
	temp1[1][2] = 0;
	temp1[2][0] = 0;
	temp1[2][1] = 0;
	temp1[2][2] = 1;


	// pitch
	angle = e->angles[PITCH];
	angle = angle * M_PI*2 / 360;
	s = sinf(angle);
	c = cosf(angle);

	temp2[0][0] = c;
	temp2[0][1] = 0;
	temp2[0][2] = -s;
	temp2[1][0] = 0;
	temp2[1][1] = 1;
	temp2[1][2] = 0;
	temp2[2][0] = s;
	temp2[2][1] = 0;
	temp2[2][2] = c;

	R_ConcatRotations (temp2, temp1, temp3);

	// roll
	angle = e->angles[ROLL];
	angle = angle * M_PI*2 / 360;
	s = sinf(angle);
	c = cosf(angle);

	temp1[0][0] = 1;
	temp1[0][1] = 0;
	temp1[0][2] = 0;
	temp1[1][0] = 0;
	temp1[1][1] = c;
	temp1[1][2] = s;
	temp1[2][0] = 0;
	temp1[2][1] = -s;
	temp1[2][2] = c;

	R_ConcatRotations (temp1, temp3, entity_rotation);

	// rotate modelorg and the transformation matrix
	R_EntityRotate (modelorg);
	R_EntityRotate (vpn);
	R_EntityRotate (vright);
	R_EntityRotate (vup);

	R_TransformFrustum ();
}


/*
================
R_RecursiveClipBPoly
================
*/
void R_RecursiveClipBPoly (bedge_t *pedges, mnode_t *pnode, msurface_t *psurf)
{
	bedge_t		*psideedges[2], *pnextedge, *ptedge;
	int			i, side, lastside;
	float		dist, frac, lastdist;
	mplane_t	*splitplane, tplane;
	mvertex_t	*pvert, *plastvert, *ptvert;
	mnode_t		*pn;

next:
	psideedges[0] = psideedges[1] = nil;

	makeclippededge = false;

	// transform the BSP plane into model space
	// FIXME: cache these?
	splitplane = pnode->plane;
	tplane.dist = splitplane->dist - DotProduct(r_entorigin, splitplane->normal);
	tplane.normal[0] = DotProduct (entity_rotation[0], splitplane->normal);
	tplane.normal[1] = DotProduct (entity_rotation[1], splitplane->normal);
	tplane.normal[2] = DotProduct (entity_rotation[2], splitplane->normal);

	// clip edges to BSP plane
	for ( ; pedges ; pedges = pnextedge)
	{
		pnextedge = pedges->pnext;

		// set the status for the last point as the previous point
		// FIXME: cache this stuff somehow?
		plastvert = pedges->v[0];
		lastdist = DotProduct (plastvert->position, tplane.normal) -
				   tplane.dist;

		lastside = lastdist <= 0;
		pvert = pedges->v[1];
		dist = DotProduct (pvert->position, tplane.normal) - tplane.dist;
		side = dist <= 0;


		if (side != lastside)
		{
			// clipped
			if (numbverts >= MAX_BMODEL_VERTS)
				return;

			// generate the clipped vertex
			frac = lastdist / (lastdist - dist);
			ptvert = &pbverts[numbverts++];
			ptvert->position[0] = plastvert->position[0] +
					frac * (pvert->position[0] -
					plastvert->position[0]);
			ptvert->position[1] = plastvert->position[1] +
					frac * (pvert->position[1] -
					plastvert->position[1]);
			ptvert->position[2] = plastvert->position[2] +
					frac * (pvert->position[2] -
					plastvert->position[2]);

			// split into two edges, one on each side, and remember entering
			// and exiting points
			// FIXME: share the clip edge by having a winding direction flag?
			if (numbedges >= (MAX_BMODEL_EDGES - 1))
			{
				Con_Printf ("Out of edges for bmodel\n");
				return;
			}

			ptedge = &pbedges[numbedges];
			ptedge->pnext = psideedges[lastside];
			psideedges[lastside] = ptedge;
			ptedge->v[0] = plastvert;
			ptedge->v[1] = ptvert;

			ptedge = &pbedges[numbedges + 1];
			ptedge->pnext = psideedges[side];
			psideedges[side] = ptedge;
			ptedge->v[0] = ptvert;
			ptedge->v[1] = pvert;

			numbedges += 2;

			if (side == 0)
			{
				// entering for front, exiting for back
				pfrontenter = ptvert;
				makeclippededge = true;
			}
			else
			{
				pfrontexit = ptvert;
				makeclippededge = true;
			}
		}
		else
		{
		// add the edge to the appropriate side
			pedges->pnext = psideedges[side];
			psideedges[side] = pedges;
		}
	}

	// if anything was clipped, reconstitute and add the edges along the clip
	// plane to both sides (but in opposite directions)
	if (makeclippededge)
	{
		if (numbedges >= (MAX_BMODEL_EDGES - 2))
		{
			Con_Printf ("Out of edges for bmodel\n");
			return;
		}

		ptedge = &pbedges[numbedges];
		ptedge->pnext = psideedges[0];
		psideedges[0] = ptedge;
		ptedge->v[0] = pfrontexit;
		ptedge->v[1] = pfrontenter;

		ptedge = &pbedges[numbedges + 1];
		ptedge->pnext = psideedges[1];
		psideedges[1] = ptedge;
		ptedge->v[0] = pfrontenter;
		ptedge->v[1] = pfrontexit;

		numbedges += 2;
	}

	// draw or recurse further
	for (i=0 ; i<2 ; i++)
	{
		if (psideedges[i])
		{
			// draw if we've reached a non-solid leaf, done if all that's left is a
			// solid leaf, and continue down the tree if it's not a leaf
			pn = pnode->children[i];

			// we're done with this branch if the node or leaf isn't in the PVS
			if (pn->visframe == r_visframecount)
			{
				if (pn->contents < 0)
				{
					if (pn->contents != CONTENTS_SOLID)
					{
						r_currentbkey = ((mleaf_t *)pn)->key;
						R_RenderBmodelFace (psideedges[i], psurf);
					}
				}
				else if(i == 1){ // last, can skip the call
					pedges = psideedges[i];
					pnode = pnode->children[i];
					goto next;
				}else{
					R_RecursiveClipBPoly (psideedges[i], pnode->children[i], psurf);
				}
			}
		}
	}
}


/*
================
R_DrawSolidClippedSubmodelPolygons
================
*/
void R_DrawSolidClippedSubmodelPolygons (model_t *pmodel)
{
	int			i, j, lindex, o;
	vec_t		dot;
	msurface_t	*psurf;
	int			numsurfaces;
	mplane_t	*pplane;
	bedge_t		*pbedge;
	medge_t		*pedge, *pedges;

	if(entdrawflags(currententity) ^ r_drawflags)
		return;

	// FIXME: use bounding-box-based frustum clipping info?

	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
	numsurfaces = pmodel->nummodelsurfaces;
	pedges = pmodel->edges;

	for (i=0 ; i<numsurfaces ; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if ((dot > 0) ^ !!(psurf->flags & SURF_PLANEBACK))
		{
			// FIXME: use bounding-box-based frustum clipping info?

			// copy the edges to bedges, flipping if necessary so always
			// clockwise winding
			// FIXME: if edges and vertices get caches, these assignments must move
			// outside the loop, and overflow checking must be done here
			pbverts = bverts;
			pbedges = bedges;
			numbverts = numbedges = 0;

			if (psurf->numedges > 0)
			{
				pbedge = &bedges[numbedges];
				numbedges += psurf->numedges;

				for (j=0 ; j<psurf->numedges ; j++)
				{
				   lindex = pmodel->surfedges[psurf->firstedge+j];

					if ((o = (lindex < 0)))
						lindex = -lindex;
					pedge = &pedges[lindex];
					pbedge[j].v[0] = &r_pcurrentvertbase[pedge->v[o]];
					pbedge[j].v[1] = &r_pcurrentvertbase[pedge->v[!o]];
					pbedge[j].pnext = &pbedge[j+1];
				}

				pbedge[j-1].pnext = nil;	// mark end of edges

				R_RecursiveClipBPoly (pbedge, currententity->topnode, psurf);
			}
			else
			{
				fatal ("no edges in bmodel");
			}
		}
	}
}


/*
================
R_DrawSubmodelPolygons
================
*/
void R_DrawSubmodelPolygons (model_t *pmodel, int clipflags)
{
	int			i;
	vec_t		dot;
	msurface_t	*psurf;
	int			numsurfaces;
	mplane_t	*pplane;

	// FIXME: use bounding-box-based frustum clipping info?

	if(entdrawflags(currententity) ^ r_drawflags)
		return;

	psurf = &pmodel->surfaces[pmodel->firstmodelsurface];
	numsurfaces = pmodel->nummodelsurfaces;

	for (i=0 ; i<numsurfaces ; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if ((dot > 0) ^ !!(psurf->flags & SURF_PLANEBACK))
		{
			r_currentkey = ((mleaf_t *)currententity->topnode)->key;

			// FIXME: use bounding-box-based frustum clipping info?
			R_RenderFace (psurf, clipflags);
		}
	}
}


/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node, int clipflags)
{
	int			i, c, side, *pindex, rejected;
	vec3_t		acceptpt, rejectpt;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	double		d, dot;

again:
	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	// cull the clipping planes if not trivial accept
	// FIXME: the compiler is doing a lousy job of optimizing here; it could be
	//  twice as fast in ASM
	if (clipflags)
	{
		for (i=0 ; i<4 ; i++)
		{
			if (! (clipflags & (1<<i)) )
				continue;	// don't need to clip against it

			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the sign bit
			// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = node->minmaxs[pindex[0]];
			rejectpt[1] = node->minmaxs[pindex[1]];
			rejectpt[2] = node->minmaxs[pindex[2]];

			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return;

			acceptpt[0] = node->minmaxs[pindex[3+0]];
			acceptpt[1] = node->minmaxs[pindex[3+1]];
			acceptpt[2] = node->minmaxs[pindex[3+2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d >= 0)
				clipflags &= ~(1<<i);	// node is entirely on screen
		}
	}

	// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c && mark)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
		{
			R_StoreEfrags (&pleaf->efrags);
		}

		pleaf->key = r_currentkey;
		r_currentkey++;		// all bmodels in a leaf share the same key
	}
	else
	{
		// node is just a decision point, so go down the apropriate sides

		// find which side of the node we are on
		plane = node->plane;
		dot = plane->type <= PLANE_Z ? modelorg[plane->type] : DotProduct(modelorg, plane->normal);
		dot -= plane->dist;
		side = dot < 0;

		// recurse down the children, front side first
		R_RecursiveWorldNode (node->children[side], clipflags);

		// draw stuff
		c = node->numsurfaces;

		if(c){
			surf = cl.worldmodel->surfaces + node->firstsurface;
			rejected = 0;

			if(dot < -BACKFACE_EPSILON){
				do{
					if((surf->flags & SURF_PLANEBACK) && surf->visframe == r_framecount)
						rejected |= !R_RenderFace(surf, clipflags);
					surf++;
				}while(--c);
			}else if(dot > BACKFACE_EPSILON){
				do{
					if(!(surf->flags & SURF_PLANEBACK) && surf->visframe == r_framecount)
						rejected |= !R_RenderFace(surf, clipflags);
					surf++;
				}while(--c);
			}

			if(rejected){
				if(node_rejects == nil || num_node_rejects >= max_node_rejects){
					max_node_rejects *= 2;
					node_rejects = realloc(node_rejects, sizeof(*node_rejects)*max_node_rejects);
				}
				node_rejects[num_node_rejects].node = node;
				node_rejects[num_node_rejects++].clipflags = clipflags;
			}

			// all surfaces on the same node share the same sequence number
			r_currentkey++;
		}

		// recurse down the back side
		node = node->children[!side];
		goto again;
	}
}



/*
================
R_RenderWorld
================
*/
void R_RenderWorld (void)
{
	model_t		*clmodel;

	currententity = &cl_entities[0];
	VectorCopy (r_origin, modelorg);
	clmodel = currententity->model;
	r_pcurrentvertbase = clmodel->vertexes;

	num_node_rejects = 0;
	R_RecursiveWorldNode (clmodel->nodes, 15);
}

void
R_RenderWorldRejects(void)
{
	model_t *clmodel;
	nodereject_t *rej;
	msurface_t *surf;
	unsigned i;

	currententity = &cl_entities[0];
	VectorCopy (r_origin, modelorg);
	clmodel = currententity->model;
	r_pcurrentvertbase = clmodel->vertexes;

	for(rej = node_rejects; rej < node_rejects+num_node_rejects; rej++){
		surf = cl.worldmodel->surfaces + rej->node->firstsurface;
		for(i = 0; i < rej->node->numsurfaces; i++, surf++)
			R_RenderFace(surf, rej->clipflags);
	}
}
