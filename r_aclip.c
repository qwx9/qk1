#include "quakedef.h"

static finalvert_t		fv[2][8];
static auxvert_t		av[8];

void R_AliasProjectFinalVert (finalvert_t *fv, auxvert_t *av);
void R_Alias_clip_top (finalvert_t *pfv0, finalvert_t *pfv1,
	finalvert_t *out);
void R_Alias_clip_bottom (finalvert_t *pfv0, finalvert_t *pfv1,
	finalvert_t *out);
void R_Alias_clip_left (finalvert_t *pfv0, finalvert_t *pfv1,
	finalvert_t *out);
void R_Alias_clip_right (finalvert_t *pfv0, finalvert_t *pfv1,
	finalvert_t *out);


/*
================
R_Alias_clip_z

pfv0 is the unclipped vertex, pfv1 is the z-clipped vertex
================
*/
void R_Alias_clip_z (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;
	auxvert_t	*pav0, *pav1, avout;
	int i;

	pav0 = &av[pfv0 - &fv[0][0]];
	pav1 = &av[pfv1 - &fv[0][0]];

	if (pfv0->v >= pfv1->v)
	{
		scale = (ALIAS_Z_CLIP_PLANE - pav0->fv[2]) / (pav1->fv[2] - pav0->fv[2]);

		avout.fv[0] = pav0->fv[0] + (pav1->fv[0] - pav0->fv[0]) * scale;
		avout.fv[1] = pav0->fv[1] + (pav1->fv[1] - pav0->fv[1]) * scale;
		avout.fv[2] = ALIAS_Z_CLIP_PLANE;

		out->s = pfv0->s + (pfv1->s - pfv0->s) * scale;
		out->t = pfv0->t + (pfv1->t - pfv0->t) * scale;
		for(i = 0; i < 3; i++)
			out->l[i] = pfv0->l[i] + (pfv1->l[i] - pfv0->l[i]) * scale;
	}
	else
	{
		scale = (ALIAS_Z_CLIP_PLANE - pav1->fv[2]) / (pav0->fv[2] - pav1->fv[2]);

		avout.fv[0] = pav1->fv[0] + (pav0->fv[0] - pav1->fv[0]) * scale;
		avout.fv[1] = pav1->fv[1] + (pav0->fv[1] - pav1->fv[1]) * scale;
		avout.fv[2] = ALIAS_Z_CLIP_PLANE;

		out->s = pfv1->s + (pfv0->s - pfv1->s) * scale;
		out->t = pfv1->t + (pfv0->t - pfv1->t) * scale;
		for(i = 0; i < 3; i++)
			out->l[i] = pfv1->l[i] + (pfv0->l[i] - pfv1->l[i]) * scale;
	}

	R_AliasProjectFinalVert (out, &avout);

	if (out->u < r_refdef.aliasvrect.x)
		out->flags |= ALIAS_LEFT_CLIP;
	if (out->v < r_refdef.aliasvrect.y)
		out->flags |= ALIAS_TOP_CLIP;
	if (out->u > r_refdef.aliasvrectright)
		out->flags |= ALIAS_RIGHT_CLIP;
	if (out->v > r_refdef.aliasvrectbottom)
		out->flags |= ALIAS_BOTTOM_CLIP;
}


void R_Alias_clip_left (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;
	int			i;

	if(pfv0->v >= pfv1->v){
		scale = (float)(r_refdef.aliasvrect.x - pfv0->u) / (pfv1->u - pfv0->u);
		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv0->x[i] + (pfv1->x[i] - pfv0->x[i])*scale + 0.5;
	}else{
		scale = (float)(r_refdef.aliasvrect.x - pfv1->u) / (pfv0->u - pfv1->u);
		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv1->x[i] + (pfv0->x[i] - pfv1->x[i])*scale + 0.5;
	}
}


void R_Alias_clip_right (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;
	int			i;

	if(pfv0->v >= pfv1->v){
		scale = (float)(r_refdef.aliasvrectright - pfv0->u) / (pfv1->u - pfv0->u);
		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv0->x[i] + (pfv1->x[i] - pfv0->x[i])*scale + 0.5;
	}else{
		scale = (float)(r_refdef.aliasvrectright - pfv1->u) / (pfv0->u - pfv1->u);
		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv1->x[i] + (pfv0->x[i] - pfv1->x[i])*scale + 0.5;
	}
}


void R_Alias_clip_top (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;
	int			i;

	if(pfv0->v >= pfv1->v){
		scale = (float)(r_refdef.aliasvrect.y - pfv0->v) / (pfv1->v - pfv0->v);
		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv0->x[i] + (pfv1->x[i] - pfv0->x[i])*scale + 0.5;
	}else{
		scale = (float)(r_refdef.aliasvrect.y - pfv1->v) / (pfv0->v - pfv1->v);
		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv1->x[i] + (pfv0->x[i] - pfv1->x[i])*scale + 0.5;
	}
}


void R_Alias_clip_bottom (finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out)
{
	float		scale;
	int			i;

	if(pfv0->v >= pfv1->v){
		scale = (float)(r_refdef.aliasvrectbottom - pfv0->v) / (pfv1->v - pfv0->v);

		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv0->x[i] + (pfv1->x[i] - pfv0->x[i])*scale + 0.5;
	}else{
		scale = (float)(r_refdef.aliasvrectbottom - pfv1->v) / (pfv0->v - pfv1->v);

		for (i=0 ; i<nelem(out->x) ; i++)
			out->x[i] = pfv1->x[i] + (pfv0->x[i] - pfv1->x[i])*scale + 0.5;
	}
}


int R_AliasClip (finalvert_t *in, finalvert_t *out, int flag, int count,
	void(*clip)(finalvert_t *pfv0, finalvert_t *pfv1, finalvert_t *out) )
{
	int			i,j,k;
	int			flags, oldflags;

	j = count-1;
	k = 0;
	for (i=0 ; i<count ; j = i, i++)
	{
		oldflags = in[j].flags & flag;
		flags = in[i].flags & flag;

		if (flags && oldflags)
			continue;
		if (oldflags ^ flags)
		{
			clip (&in[j], &in[i], &out[k]);
			out[k].flags = 0;
			if (out[k].u < r_refdef.aliasvrect.x)
				out[k].flags |= ALIAS_LEFT_CLIP;
			if (out[k].v < r_refdef.aliasvrect.y)
				out[k].flags |= ALIAS_TOP_CLIP;
			if (out[k].u > r_refdef.aliasvrectright)
				out[k].flags |= ALIAS_RIGHT_CLIP;
			if (out[k].v > r_refdef.aliasvrectbottom)
				out[k].flags |= ALIAS_BOTTOM_CLIP;
			k++;
		}
		if (!flags)
		{
			out[k] = in[i];
			k++;
		}
	}

	return k;
}


/*
================
R_AliasClipTriangle
================
*/
void R_AliasClipTriangle (mtriangle_t *ptri, auxvert_t *auxverts)
{
	int					i, k, pingpong;
	static mtriangle_t	mtri;
	unsigned			clipflags;

	memset(fv, 0, sizeof(fv));

	// copy vertexes and fix seam texture coordinates
	if (ptri->facesfront)
	{
		fv[0][0] = pfinalverts[ptri->vertindex[0]];
		fv[0][1] = pfinalverts[ptri->vertindex[1]];
		fv[0][2] = pfinalverts[ptri->vertindex[2]];
	}
	else
	{
		for (i=0 ; i<3 ; i++)
		{
			fv[0][i] = pfinalverts[ptri->vertindex[i]];

			if (!ptri->facesfront && (fv[0][i].flags & ALIAS_ONSEAM) )
				fv[0][i].s += r_affinetridesc.seamfixupX16;
		}
	}

	// clip
	clipflags = fv[0][0].flags | fv[0][1].flags | fv[0][2].flags;

	if (clipflags & ALIAS_Z_CLIP)
	{
		for (i=0 ; i<3 ; i++)
			av[i] = auxverts[ptri->vertindex[i]];

		k = R_AliasClip (fv[0], fv[1], ALIAS_Z_CLIP, 3, R_Alias_clip_z);
		if (k == 0)
			return;

		pingpong = 1;
		clipflags = fv[1][0].flags | fv[1][1].flags | fv[1][2].flags;
	}
	else
	{
		pingpong = 0;
		k = 3;
	}

	if (clipflags & ALIAS_LEFT_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_LEFT_CLIP, k, R_Alias_clip_left);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	if (clipflags & ALIAS_RIGHT_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_RIGHT_CLIP, k, R_Alias_clip_right);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	if (clipflags & ALIAS_BOTTOM_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_BOTTOM_CLIP, k, R_Alias_clip_bottom);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	if (clipflags & ALIAS_TOP_CLIP)
	{
		k = R_AliasClip (fv[pingpong], fv[pingpong ^ 1], ALIAS_TOP_CLIP, k, R_Alias_clip_top);
		if (k == 0)
			return;

		pingpong ^= 1;
	}

	for (i=0 ; i<k ; i++)
	{
		if (fv[pingpong][i].u < r_refdef.aliasvrect.x)
			fv[pingpong][i].u = r_refdef.aliasvrect.x;
		else if (fv[pingpong][i].u > r_refdef.aliasvrectright)
			fv[pingpong][i].u = r_refdef.aliasvrectright;

		if (fv[pingpong][i].v < r_refdef.aliasvrect.y)
			fv[pingpong][i].v = r_refdef.aliasvrect.y;
		else if (fv[pingpong][i].v > r_refdef.aliasvrectbottom)
			fv[pingpong][i].v = r_refdef.aliasvrectbottom;

		fv[pingpong][i].flags = 0;
	}

	// draw triangles
	mtri.facesfront = ptri->facesfront;
	r_affinetridesc.ptriangles = &mtri;
	r_affinetridesc.pfinalverts = fv[pingpong];

	// FIXME: do all at once as trifan?
	mtri.vertindex[0] = 0;
	for (i=1 ; i<k-1 ; i++)
	{
		mtri.vertindex[1] = i;
		mtri.vertindex[2] = i+1;
		D_PolysetDraw (currententity->colormap);
	}
}
