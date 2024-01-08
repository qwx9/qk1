#include "quakedef.h"

float scale_for_mip;

// FIXME: should go away
void R_RotateBmodel(entity_t *entity, view_t *v);

static int
D_MipLevelForScale(float scale)
{
	int lmiplevel;

	if(scale >= d_scalemip[0])
		lmiplevel = 0;
	else if(scale >= d_scalemip[1])
		lmiplevel = 1;
	else if(scale >= d_scalemip[2])
		lmiplevel = 2;
	else
		lmiplevel = 3;

	return max(d_minmip, lmiplevel);
}

static void
D_DrawSolidSurface(surf_t *surf, pixel_t color)
{
	espan_t *span;
	pixel_t *pdest;
	uzint *pz;
	int u, u2;

	for(span = surf->spans; span; span=span->pnext){
		pdest = dvars.fb + span->v*dvars.w;
		pz = dvars.zb + span->v*dvars.w;
		memset(pz, 0xfe, span->count*sizeof(*pz));
		u2 = span->u + span->count - 1;
		for(u = span->u; u <= u2; u++)
			pdest[u] = color;
	}
}


static void
D_CalcGradients(int miplevel, msurface_t *pface, vec3_t transformed_modelorg, view_t *v, texvars_t *tv)
{
	vec3_t p_temp1, p_saxis, p_taxis;
	float mipscale;
	float t;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector(pface->texinfo->vecs[0], p_saxis, v);
	TransformVector(pface->texinfo->vecs[1], p_taxis, v);

	t = xscaleinv * mipscale;

	tv->s.divz.stepu = p_saxis[0] * t;
	tv->t.divz.stepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	tv->s.divz.stepv = -p_saxis[1] * t;
	tv->t.divz.stepv = -p_taxis[1] * t;

	tv->s.divz.origin = p_saxis[2] * mipscale - xcenter * tv->s.divz.stepu - ycenter * tv->s.divz.stepv;
	tv->t.divz.origin = p_taxis[2] * mipscale - xcenter * tv->t.divz.stepu - ycenter * tv->t.divz.stepv;

	VectorScale(transformed_modelorg, mipscale, p_temp1);

	t = 0x10000*mipscale;
	tv->s.adjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
			((pface->texturemins[0] << 16) >> miplevel)
			+ pface->texinfo->vecs[0][3]*t;
	tv->t.adjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
			((pface->texturemins[1] << 16) >> miplevel)
			+ pface->texinfo->vecs[1][3]*t;

	// -1 (-epsilon) so we never wander off the edge of the texture
	tv->s.bbextent = ((pface->extents[0] << 16) >> miplevel) - 1;
	tv->t.bbextent = ((pface->extents[1] << 16) >> miplevel) - 1;
}

void
D_DrawSurfaces(view_t *v0)
{
	vec3_t local_modelorg, transformed_modelorg, world_transformed_modelorg;
	surfcache_t *pcurrentcache;
	drawsurf_t ds = {0};
	msurface_t *pface;
	int miplevel;
	entity_t *e;
	texvars_t t;
	surf_t *s;
	byte alpha;
	bool blend;
	view_t v;

	memmove(&v, v0, sizeof(v));
	TransformVector(v.modelorg, transformed_modelorg, &v);
	VectorCopy(transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	for(s = &surfaces[1]; s < surface_p; s++){
		if(!s->spans)
			continue;

		e = s->entity;
		if((surfdrawflags(s->flags) | entdrawflags(e)) ^ r_drawflags)
			continue;
		alpha = 255;
		if(enthasalpha(e) && e->alpha != 255)
			alpha = e->alpha;
		else if(s->flags & SURF_TRANS)
			alpha *= alphafor(s->flags);
		if(alpha < 1)
			alpha = 255;

		t.z.stepu = s->d_zistepu;
		t.z.stepv = s->d_zistepv;
		t.z.origin = s->d_ziorigin;

		if(insubmodel(s)){
			VectorSubtract(v.org, e->origin, local_modelorg);
			TransformVector(local_modelorg, transformed_modelorg, &v);
			R_RotateBmodel(e, &v);
		}

		pface = s->data;
		if(s->flags & SURF_DRAWSKY){
			D_DrawSkyScans8(s->spans);
		}else if(s->flags & SURF_DRAWBACKGROUND){
			D_DrawSolidSurface(s, q1pal[(int)r_clearcolor.value & 0xFF]);
		}else if(s->flags & SURF_DRAWTURB){
			t.p = pface->texinfo->texture->pixels;
			t.w = 64;
			D_CalcGradients(0, pface, transformed_modelorg, &v, &t);
			D_DrawSpans(s->spans, &t, alpha, SPAN_TURB);
		}else{
			miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);
			if(s->flags & SURF_FENCE)
				miplevel = max(miplevel-1, 0);

			pcurrentcache = D_CacheSurface(s->entity, pface, &ds, miplevel);
			t.p = pcurrentcache->pixels;
			t.w = pcurrentcache->width;
			D_CalcGradients(miplevel, pface, transformed_modelorg, &v, &t);
			blend = (s->flags & SURF_FENCE) || (r_drawflags & DRAW_BLEND);
			D_DrawSpans(s->spans, &t, alpha,
				(alpha == 255 && (s->flags & SURF_FENCE))
					? SPAN_FENCE
					: (blend ? SPAN_BLEND : SPAN_SOLID)
			);
		}

		if(insubmodel(s)){
			VectorCopy(world_transformed_modelorg, transformed_modelorg);
			memmove(&v, v0, sizeof(v));
		}
	}
}
