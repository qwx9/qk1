#include "quakedef.h"

#define LIGHT_MIN	5		// lowest light value we'll allow, to avoid the
							//  need for inner-loop light clamping

affinetridesc_t	r_affinetridesc;

// TODO: these probably will go away with optimized rasterization
finalvert_t			*pfinalverts;
static mdl_t		*pmdl;
static vec3_t		r_plightvec;
static int			r_ambientlight[3];
static float		r_shadelight[3];
static aliashdr_t	*paliashdr;
static float		ziscale;
static model_t		*pmodel;

static vec3_t		alias_forward, alias_right, alias_up;

static maliasskindesc_t	*pskindesc;

static int				a_skinwidth;
static int				r_anumverts;

static float	aliastransform[3][4];

typedef struct {
	int	index0;
	int	index1;
} aedge_t;

static const aedge_t aedges[12] = {
	{0, 1}, {1, 2}, {2, 3}, {3, 0},
	{4, 5}, {5, 6}, {6, 7}, {7, 4},
	{0, 5}, {1, 4}, {2, 7}, {3, 6}
};

const float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

static void R_AliasSetUpTransform (int trivial_accept, view_t *v);
static void R_AliasTransformVector (vec3_t in, vec3_t out);
static void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av, trivertx_t *pverts, stvert_t *pstverts);
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);


/*
================
R_AliasCheckBBox
================
*/
bool R_AliasCheckBBox (view_t *v)
{
	int					i, flags, frame, numv;
	aliashdr_t			*pahdr;
	float				zi, basepts[8][3], v0, v1, frac;
	finalvert_t			*pv0, *pv1, viewpts[16];
	auxvert_t			*pa0, *pa1, viewaux[16];
	maliasframedesc_t	*pframedesc;
	bool			zclipped, zfullyclipped;
	unsigned			anyclip, allclip;
	int					minz;

	// expand, rotate, and translate points into worldspace

	currententity->trivial_accept = 0;
	pmodel = currententity->model;
	pahdr = Mod_Extradata (pmodel);
	pmdl = (mdl_t *)((byte *)pahdr + pahdr->model);

	R_AliasSetUpTransform (0, v);

	// construct the base bounding box for this frame
	frame = currententity->frame;
	// TODO: don't repeat this check when drawing?
	if ((frame >= pmdl->numframes) || (frame < 0))
	{
		Con_DPrintf("No such frame %d %s\n", frame, pmodel->name);
		frame = 0;
	}

	pframedesc = &pahdr->frames[frame];

	// x worldspace coordinates
	basepts[0][0] = basepts[1][0] = basepts[2][0] = basepts[3][0] =
			(float)pframedesc->bboxmin.v[0];
	basepts[4][0] = basepts[5][0] = basepts[6][0] = basepts[7][0] =
			(float)pframedesc->bboxmax.v[0];

	// y worldspace coordinates
	basepts[0][1] = basepts[3][1] = basepts[5][1] = basepts[6][1] =
			(float)pframedesc->bboxmin.v[1];
	basepts[1][1] = basepts[2][1] = basepts[4][1] = basepts[7][1] =
			(float)pframedesc->bboxmax.v[1];

	// z worldspace coordinates
	basepts[0][2] = basepts[1][2] = basepts[4][2] = basepts[5][2] = (float)pframedesc->bboxmin.v[2];
	basepts[2][2] = basepts[3][2] = basepts[6][2] = basepts[7][2] = (float)pframedesc->bboxmax.v[2];

	zclipped = false;
	zfullyclipped = true;

	minz = Q_MAXINT;
	for (i=0; i<8 ; i++)
	{
		R_AliasTransformVector  (&basepts[i][0], &viewaux[i].fv[0]);

		if (viewaux[i].fv[2] < ALIAS_Z_CLIP_PLANE)
		{
			// we must clip points that are closer than the near clip plane
			viewpts[i].flags = ALIAS_Z_CLIP;
			zclipped = true;
		}
		else
		{
			if (viewaux[i].fv[2] < minz)
				minz = viewaux[i].fv[2];
			viewpts[i].flags = 0;
			zfullyclipped = false;
		}
	}


	if (zfullyclipped)
	{
		return false;	// everything was near-z-clipped
	}

	numv = 8;

	if (zclipped)
	{
		// organize points by edges, use edges to get new points (possible trivial reject)
		for (i=0 ; i<12 ; i++)
		{
			// edge endpoints
			pv0 = &viewpts[aedges[i].index0];
			pv1 = &viewpts[aedges[i].index1];
			pa0 = &viewaux[aedges[i].index0];
			pa1 = &viewaux[aedges[i].index1];

			// if one end is clipped and the other isn't, make a new point
			if (pv0->flags ^ pv1->flags)
			{
				frac = (ALIAS_Z_CLIP_PLANE - pa0->fv[2]) /
					   (pa1->fv[2] - pa0->fv[2]);
				viewaux[numv].fv[0] = pa0->fv[0] +
						(pa1->fv[0] - pa0->fv[0]) * frac;
				viewaux[numv].fv[1] = pa0->fv[1] +
						(pa1->fv[1] - pa0->fv[1]) * frac;
				viewaux[numv].fv[2] = ALIAS_Z_CLIP_PLANE;
				viewpts[numv].flags = 0;
				numv++;
			}
		}
	}

	// project the vertices that remain after clipping
	anyclip = 0;
	allclip = ALIAS_XY_CLIP_MASK;

	// TODO: probably should do this loop in ASM, especially if we use floats
	for (i=0 ; i<numv ; i++)
	{
	// we don't need to bother with vertices that were z-clipped
		if (viewpts[i].flags & ALIAS_Z_CLIP)
			continue;

		zi = 1.0 / viewaux[i].fv[2];

		// FIXME: do with chop mode in ASM, or convert to float
		v0 = (viewaux[i].fv[0] * xscale * zi) + xcenter;
		v1 = (viewaux[i].fv[1] * yscale * zi) + ycenter;

		flags = 0;

		if (v0 < r_refdef.fvrectx)
			flags |= ALIAS_LEFT_CLIP;
		if (v1 < r_refdef.fvrecty)
			flags |= ALIAS_TOP_CLIP;
		if (v0 > r_refdef.fvrectright)
			flags |= ALIAS_RIGHT_CLIP;
		if (v1 > r_refdef.fvrectbottom)
			flags |= ALIAS_BOTTOM_CLIP;

		anyclip |= flags;
		allclip &= flags;
	}

	if (allclip)
		return false;	// trivial reject off one side

	currententity->trivial_accept = !anyclip & !zclipped;

	if (currententity->trivial_accept)
	{
		if (minz > (r_aliastransition + (pmdl->size * r_resfudge)))
		{
			currententity->trivial_accept |= 2;
		}
	}

	return true;
}


/*
================
R_AliasTransformVector
================
*/
static void R_AliasTransformVector (vec3_t in, vec3_t out)
{
	out[0] = DotProduct(in, aliastransform[0]) + aliastransform[0][3];
	out[1] = DotProduct(in, aliastransform[1]) + aliastransform[1][3];
	out[2] = DotProduct(in, aliastransform[2]) + aliastransform[2][3];
}


/*
================
R_AliasPreparePoints

General clipped case
================
*/
void R_AliasPreparePoints (trivertx_t *apverts, auxvert_t *auxverts, pixel_t *colormap)
{
	int			i;
	stvert_t	*pstverts;
	finalvert_t	*fv;
	mtriangle_t	*ptri;
	finalvert_t	*pfv[3];
	auxvert_t *av;

	pstverts = (stvert_t *)((byte *)paliashdr + paliashdr->stverts);
	r_anumverts = pmdl->numverts;
 	fv = pfinalverts;
 	av = auxverts;

	for (i=0 ; i<r_anumverts ; i++, fv++, av++, apverts++, pstverts++)
	{
		R_AliasTransformFinalVert (fv, av, apverts, pstverts);
		if (av->fv[2] < ALIAS_Z_CLIP_PLANE)
			fv->flags |= ALIAS_Z_CLIP;
		else
		{
			 R_AliasProjectFinalVert (fv, av);

			if (fv->u < r_refdef.aliasvrect.x)
				fv->flags |= ALIAS_LEFT_CLIP;
			if (fv->v < r_refdef.aliasvrect.y)
				fv->flags |= ALIAS_TOP_CLIP;
			if (fv->u > r_refdef.aliasvrectright)
				fv->flags |= ALIAS_RIGHT_CLIP;
			if (fv->v > r_refdef.aliasvrectbottom)
				fv->flags |= ALIAS_BOTTOM_CLIP;
		}
	}

	// clip and draw all triangles
	r_affinetridesc.numtriangles = 1;

	ptri = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	for (i=0 ; i<pmdl->numtris ; i++, ptri++)
	{
		pfv[0] = &pfinalverts[ptri->vertindex[0]];
		pfv[1] = &pfinalverts[ptri->vertindex[1]];
		pfv[2] = &pfinalverts[ptri->vertindex[2]];

		if ( pfv[0]->flags & pfv[1]->flags & pfv[2]->flags & (ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) )
			continue;		// completely clipped

		if ( ! ( (pfv[0]->flags | pfv[1]->flags | pfv[2]->flags) &
			(ALIAS_XY_CLIP_MASK | ALIAS_Z_CLIP) ) )
		{	// totally unclipped
			r_affinetridesc.pfinalverts = pfinalverts;
			r_affinetridesc.ptriangles = ptri;
			D_PolysetDraw (colormap);
		}
		else
		{	// partially clipped
			R_AliasClipTriangle (ptri, auxverts);
		}
	}
}


/*
================
R_AliasSetUpTransform
================
*/
static void R_AliasSetUpTransform (int trivial_accept, view_t *v)
{
	int		i;
	float	rotationmatrix[3][4], t2matrix[3][4];
	static float	tmatrix[3][4];
	static float	viewmatrix[3][4];
	vec3_t	angles;

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity

	angles[ROLL] = currententity->angles[ROLL];
	angles[PITCH] = -currententity->angles[PITCH];
	angles[YAW] = currententity->angles[YAW];
	AngleVectors (angles, alias_forward, alias_right, alias_up);

	tmatrix[0][0] = pmdl->scale[0];
	tmatrix[1][1] = pmdl->scale[1];
	tmatrix[2][2] = pmdl->scale[2];

	tmatrix[0][3] = pmdl->scale_origin[0];
	tmatrix[1][3] = pmdl->scale_origin[1];
	tmatrix[2][3] = pmdl->scale_origin[2];

// TODO: can do this with simple matrix rearrangement

	for (i=0 ; i<3 ; i++)
	{
		t2matrix[i][0] = alias_forward[i];
		t2matrix[i][1] = -alias_right[i];
		t2matrix[i][2] = alias_up[i];
	}

	t2matrix[0][3] = -v->modelorg[0];
	t2matrix[1][3] = -v->modelorg[1];
	t2matrix[2][3] = -v->modelorg[2];

	// FIXME: can do more efficiently than full concatenation
	R_ConcatTransforms (t2matrix, tmatrix, rotationmatrix);

	// TODO: should be global, set when vright, etc., set
	VectorCopy (v->right, viewmatrix[0]);
	VectorCopy (v->up, viewmatrix[1]);
	VectorInverse (viewmatrix[1]);
	VectorCopy (v->pn, viewmatrix[2]);

	//	viewmatrix[0][3] = 0;
	//	viewmatrix[1][3] = 0;
	//	viewmatrix[2][3] = 0;

	R_ConcatTransforms (viewmatrix, rotationmatrix, aliastransform);

	// do the scaling up of x and y to screen coordinates as part of the transform
	// for the unclipped case (it would mess up clipping in the clipped case).
	// Also scale down z, so 1/z is scaled 31 bits for free, and scale down x and y
	// correspondingly so the projected x and y come out right
	// FIXME: make this work for clipped case too?
	if (trivial_accept)
	{
		for (i=0 ; i<4 ; i++)
		{
			aliastransform[0][i] *= aliasxscale *
					(1.0 / ((float)0x8000 * 0x10000));
			aliastransform[1][i] *= aliasyscale *
					(1.0 / ((float)0x8000 * 0x10000));
			aliastransform[2][i] *= 1.0 / ((float)0x8000 * 0x10000);

		}
	}
}


/*
================
R_AliasTransformFinalVert
================
*/
static void R_AliasTransformFinalVert (finalvert_t *fv, auxvert_t *av, trivertx_t *pverts, stvert_t *pstverts)
{
	float	lightcos;

	av->fv[0] = DotProduct_(pverts->v, aliastransform[0]) + aliastransform[0][3];
	av->fv[1] = DotProduct_(pverts->v, aliastransform[1]) + aliastransform[1][3];
	av->fv[2] = DotProduct_(pverts->v, aliastransform[2]) + aliastransform[2][3];

	fv->s = pstverts->s;
	fv->t = pstverts->t;

	fv->flags = pstverts->onseam;

	// lighting
	lightcos = DotProduct(r_avertexnormals[pverts->lightnormalindex], r_plightvec);
	fv->l[0] = r_ambientlight[0];
	fv->l[1] = r_ambientlight[1];
	fv->l[2] = r_ambientlight[2];

	if (lightcos < 0)
	{
		fv->l[0] += r_shadelight[0] * lightcos;
		fv->l[1] += r_shadelight[1] * lightcos;
		fv->l[2] += r_shadelight[2] * lightcos;

		// clamp; because we limited the minimum ambient and shading light, we
		// don't have to clamp low light, just bright
		fv->l[0] = max(0, fv->l[0]);
		fv->l[1] = max(0, fv->l[1]);
		fv->l[2] = max(0, fv->l[2]);
	}
}


/*
================
R_AliasTransformAndProjectFinalVerts
================
*/
void R_AliasTransformAndProjectFinalVerts (finalvert_t *fv, stvert_t *pstverts, trivertx_t *pverts)
{
	int			i;
	float		lightcos, zi;

	for (i=0 ; i<r_anumverts ; i++, fv++, pverts++, pstverts++)
	{
		// transform and project
		zi = 1.0 / (DotProduct_(pverts->v, aliastransform[2]) + aliastransform[2][3]);

		// x, y, and z are scaled down by 1/2**31 in the transform, so 1/z is
		// scaled up by 1/2**31, and the scaling cancels out for x and y in the
		// projection
		fv->zi = zi;

		fv->u = ((DotProduct_(pverts->v, aliastransform[0]) + aliastransform[0][3]) * zi) + aliasxcenter;
		fv->v = ((DotProduct_(pverts->v, aliastransform[1]) + aliastransform[1][3]) * zi) + aliasycenter;

		fv->s = pstverts->s;
		fv->t = pstverts->t;
		fv->flags = pstverts->onseam;

		// lighting
		lightcos = DotProduct(r_avertexnormals[pverts->lightnormalindex], r_plightvec);
		fv->l[0] = r_ambientlight[0];
		fv->l[1] = r_ambientlight[1];
		fv->l[2] = r_ambientlight[2];

		if (lightcos < 0)
		{
			fv->l[0] += r_shadelight[0] * lightcos;
			fv->l[1] += r_shadelight[1] * lightcos;
			fv->l[2] += r_shadelight[2] * lightcos;

			// clamp; because we limited the minimum ambient and shading light, we
			// don't have to clamp low light, just bright
			fv->l[0] = max(0, fv->l[0]);
			fv->l[1] = max(0, fv->l[1]);
			fv->l[2] = max(0, fv->l[2]);
		}
	}
}


/*
================
R_AliasProjectFinalVert
================
*/
void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av)
{
	float	zi;

	// project points
	zi = 1.0 / av->fv[2];

	fv->zi = zi * ziscale;

	fv->u = (av->fv[0] * aliasxscale * zi) + aliasxcenter;
	fv->v = (av->fv[1] * aliasyscale * zi) + aliasycenter;
}


/*
================
R_AliasPrepareUnclippedPoints
================
*/
void R_AliasPrepareUnclippedPoints (trivertx_t *pverts, pixel_t *colormap)
{
	stvert_t	*pstverts;
	finalvert_t	*fv;

	pstverts = (stvert_t *)((byte *)paliashdr + paliashdr->stverts);
	r_anumverts = pmdl->numverts;
	// FIXME: just use pfinalverts directly?
	fv = pfinalverts;

	R_AliasTransformAndProjectFinalVerts (fv, pstverts, pverts);

	if (r_affinetridesc.drawtype)
		D_PolysetDrawFinalVerts (fv, r_anumverts, colormap, currententity->alpha);

	r_affinetridesc.pfinalverts = pfinalverts;
	r_affinetridesc.ptriangles = (mtriangle_t *)((byte *)paliashdr + paliashdr->triangles);
	r_affinetridesc.numtriangles = pmdl->numtris;

	D_PolysetDraw (colormap);
}

/*
===============
R_AliasSetupSkin
===============
*/
void R_AliasSetupSkin (void)
{
	int					skinnum;
	int					i, numskins;
	maliasskingroup_t	*paliasskingroup;
	float				*pskinintervals, fullskininterval;
	float				skintargettime, skintime;

	skinnum = currententity->skinnum;
	if ((skinnum >= pmdl->numskins) || (skinnum < 0))
	{
		Con_DPrintf("R_AliasSetupSkin: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	pskindesc = ((maliasskindesc_t *)
			((byte *)paliashdr + paliashdr->skindesc)) + skinnum;
	a_skinwidth = pmdl->skinwidth;

	if (pskindesc->type == ALIAS_SKIN_GROUP)
	{
		paliasskingroup = (maliasskingroup_t *)((byte *)paliashdr +
				pskindesc->skin);
		pskinintervals = (float *)
				((byte *)paliashdr + paliasskingroup->intervals);
		numskins = paliasskingroup->numskins;
		fullskininterval = pskinintervals[numskins-1];

		skintime = cl.time + currententity->syncbase;

		// when loading in Mod_LoadAliasSkinGroup, we guaranteed all interval
		// values are positive, so we don't have to worry about division by 0
		skintargettime = skintime -
				((int)(skintime / fullskininterval)) * fullskininterval;

		for (i=0 ; i<(numskins-1) ; i++)
		{
			if (pskinintervals[i] > skintargettime)
				break;
		}

		pskindesc = &paliasskingroup->skindescs[i];
	}

	r_affinetridesc.pskindesc = pskindesc;
	r_affinetridesc.pskin = (void *)((byte *)paliashdr + pskindesc->skin);
	r_affinetridesc.skinwidth = a_skinwidth;
	r_affinetridesc.seamfixupX16 =  (a_skinwidth >> 1) << 16;
	r_affinetridesc.skinheight = pmdl->skinheight;
}

/*
================
R_AliasSetupLighting
================
*/
void R_AliasSetupLighting (alight_t *plighting)
{
	int i;

	// guarantee that no vertex will ever be lit below LIGHT_MIN, so we don't have
	// to clamp off the bottom
	for(i = 0; i < 3; i++){
		r_ambientlight[i] = plighting->ambientlight[i];
		if (r_ambientlight[i] < LIGHT_MIN)
			r_ambientlight[i] = LIGHT_MIN;
		r_ambientlight[i] = (255 - r_ambientlight[i]) << VID_CBITS;
		if (r_ambientlight[i] < LIGHT_MIN)
			r_ambientlight[i] = LIGHT_MIN;
		r_shadelight[i] = plighting->shadelight[i];
		if (r_shadelight[i] < 0)
			r_shadelight[i] = 0;
		r_shadelight[i] *= VID_GRADES;
	}

	// rotate the lighting vector into the model's frame of reference
	r_plightvec[0] = DotProduct(plighting->plightvec, alias_forward);
	r_plightvec[1] = -DotProduct(plighting->plightvec, alias_right);
	r_plightvec[2] = DotProduct(plighting->plightvec, alias_up);
}

/*
=================
R_AliasSetupFrame

set r_apverts
=================
*/
trivertx_t *R_AliasSetupFrame (void)
{
	int				frame;
	int				i, numframes;
	maliasgroup_t	*paliasgroup;
	float			*pintervals, fullinterval, targettime, time;

	frame = currententity->frame;
	if ((frame >= pmdl->numframes) || (frame < 0))
	{
		Con_DPrintf("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	if (paliashdr->frames[frame].type == ALIAS_SINGLE)
	{
		return (trivertx_t *)
				((byte *)paliashdr + paliashdr->frames[frame].frame);
	}

	paliasgroup = (maliasgroup_t *)
				((byte *)paliashdr + paliashdr->frames[frame].frame);
	pintervals = (float *)((byte *)paliashdr + paliasgroup->intervals);
	numframes = paliasgroup->numframes;
	fullinterval = pintervals[numframes-1];

	time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadAliasGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
	targettime = time - ((int)(time / fullinterval)) * fullinterval;

	for (i=0 ; i<(numframes-1) ; i++)
	{
		if (pintervals[i] > targettime)
			break;
	}

	return (trivertx_t *)
				((byte *)paliashdr + paliasgroup->frames[i].frame);
}


/*
================
R_AliasDrawModel
================
*/
void
R_AliasDrawModel(alight_t *plighting, view_t *v)
{
	static finalvert_t finalverts[MAXALIASVERTS];
	static auxvert_t auxverts[MAXALIASVERTS];
	trivertx_t *pverts;

	// cache align
	pfinalverts = finalverts;

	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);
	pmdl = (mdl_t *)((byte *)paliashdr + paliashdr->model);

	R_AliasSetupSkin();
	R_AliasSetUpTransform(currententity->trivial_accept, v);
	R_AliasSetupLighting(plighting);
	pverts = R_AliasSetupFrame ();

	if (!currententity->colormap)
		fatal ("R_AliasDrawModel: !currententity->colormap");

	r_affinetridesc.drawtype = (currententity->trivial_accept == 3) &&
			r_recursiveaffinetriangles;

	if (r_affinetridesc.drawtype)
		D_PolysetUpdateTables ();		// FIXME: precalc...

	if (currententity != &cl.viewent)
		ziscale = (float)0x8000 * (float)0x10000;
	else
		ziscale = (float)0x8000 * (float)0x10000 * 3.0;

	if (currententity->trivial_accept)
		R_AliasPrepareUnclippedPoints (pverts, currententity->colormap);
	else
		R_AliasPreparePoints (pverts, auxverts, currententity->colormap);
}
