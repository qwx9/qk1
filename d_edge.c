#include "quakedef.h"

float scale_for_mip;

// FIXME: should go away
extern void R_RotateBmodel (entity_t *entity);
extern void R_TransformFrustum (void);

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
		pdest = dvars.viewbuffer + span->v*dvars.width;
		pz = dvars.zbuffer + span->v*dvars.width;
		memset(pz, 0xfe, span->count*sizeof(*pz));
		u2 = span->u + span->count - 1;
		for(u = span->u; u <= u2; u++)
			pdest[u] = color;
	}
}


static void
D_CalcGradients(int miplevel, msurface_t *pface, vec3_t transformed_modelorg)
{
	float mipscale;
	vec3_t p_temp1, p_saxis, p_taxis;
	float t;

	mipscale = 1.0 / (float)(1 << miplevel);

	TransformVector (pface->texinfo->vecs[0], p_saxis);
	TransformVector (pface->texinfo->vecs[1], p_taxis);

	t = xscaleinv * mipscale;

	dvars.sdivzstepu = p_saxis[0] * t;
	dvars.tdivzstepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	dvars.sdivzstepv = -p_saxis[1] * t;
	dvars.tdivzstepv = -p_taxis[1] * t;

	dvars.sdivzorigin = p_saxis[2] * mipscale - xcenter * dvars.sdivzstepu - ycenter * dvars.sdivzstepv;
	dvars.tdivzorigin = p_taxis[2] * mipscale - xcenter * dvars.tdivzstepu - ycenter * dvars.tdivzstepv;

	VectorScale (transformed_modelorg, mipscale, p_temp1);

	t = 0x10000*mipscale;
	dvars.sadjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
			((pface->texturemins[0] << 16) >> miplevel)
			+ pface->texinfo->vecs[0][3]*t;
	dvars.tadjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
			((pface->texturemins[1] << 16) >> miplevel)
			+ pface->texinfo->vecs[1][3]*t;

	// -1 (-epsilon) so we never wander off the edge of the texture
	dvars.bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
	dvars.bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}

void dospan_turb(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, byte alpha, uzint *pz, int izi, int izistep);
void dospan_alpha(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, byte alpha, uzint *pz, int izi, int izistep);
void dospan(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, byte alpha, uzint *pz, int izi, int izistep);

void
D_DrawSurfaces (void)
{
	surf_t *s;
	msurface_t *pface;
	surfcache_t *pcurrentcache;
	vec3_t local_modelorg, transformed_modelorg, world_transformed_modelorg;
	int miplevel;
	byte alpha;
	bool blend;

	currententity = cl_entities;
	TransformVector(modelorg, transformed_modelorg);
	VectorCopy(transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	for(s = &surfaces[1]; s < surface_p; s++){
		if(!s->spans)
			continue;

		if((surfdrawflags(s->flags) | entdrawflags(s->entity)) ^ r_drawflags)
			continue;
		alpha = 255;
		if(enthasalpha(s->entity) && s->entity->alpha != 255)
			alpha = s->entity->alpha;
		else if(s->flags & SURF_TRANS)
			alpha *= alphafor(s->flags);
		if(alpha < 1)
			alpha = 255;

		blend = (s->flags & SURF_FENCE) || (r_drawflags & DRAW_BLEND);

		dvars.zistepu = s->d_zistepu;
		dvars.zistepv = s->d_zistepv;
		dvars.ziorigin = s->d_ziorigin;

		if(insubmodel(s)){
			currententity = s->entity;
			VectorSubtract(r_origin, currententity->origin, local_modelorg);
			TransformVector(local_modelorg, transformed_modelorg);
			R_RotateBmodel(currententity);
		}

		pface = s->data;
		if(s->flags & SURF_DRAWSKY){
			D_DrawSkyScans8(s->spans);
		}else if(s->flags & SURF_DRAWBACKGROUND){
			D_DrawSolidSurface(s, q1pal[(int)r_clearcolor.value & 0xFF]);
		}else if(s->flags & SURF_DRAWTURB){
			D_CalcGradients(0, pface, transformed_modelorg);
			D_DrawSpans(s->spans, pface->texinfo->texture->pixels, 64, alpha, SPAN_TURB);
		}else{
			miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);
			if(s->flags & SURF_FENCE)
				miplevel = max(miplevel-1, 0);

			pcurrentcache = D_CacheSurface(pface, miplevel);
			D_CalcGradients(miplevel, pface, transformed_modelorg);
			D_DrawSpans(s->spans, pcurrentcache->pixels, pcurrentcache->width, alpha,
				(alpha == 255 && s->flags & SURF_FENCE) ? SPAN_FENCE : (blend ? SPAN_BLEND : SPAN_SOLID)
			);
		}

		if(insubmodel(s)){
			currententity = cl_entities;
			VectorCopy(world_transformed_modelorg, transformed_modelorg);
			VectorCopy(base_vpn, vpn);
			VectorCopy(base_vup, vup);
			VectorCopy(base_vright, vright);
			VectorCopy(base_modelorg, modelorg);
			R_TransformFrustum();
		}
	}
}
