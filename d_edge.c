#include "quakedef.h"

static int	miplevel;

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

// FIXME: clean this up
static void
D_DrawSolidSurface(surf_t *surf, pixel_t color)
{
	espan_t *span;
	pixel_t *pdest;
	int u, u2;

	for(span = surf->spans; span; span=span->pnext){
		pdest = dvars.viewbuffer + screenwidth*span->v;
		u2 = span->u + span->count - 1;
		for(u = span->u; u <= u2; u++)
			pdest[u] = color;
	}
}


static void
D_CalcGradients(msurface_t *pface, vec3_t transformed_modelorg)
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

void
D_DrawSurfaces (void)
{
	surf_t *s;
	msurface_t *pface;
	surfcache_t *pcurrentcache;
	vec3_t world_transformed_modelorg, local_modelorg, transformed_modelorg;
	byte alpha;
	bool blend;

	currententity = &cl_entities[0];
	TransformVector(modelorg, transformed_modelorg);
	VectorCopy(transformed_modelorg, world_transformed_modelorg);

	// TODO: could preset a lot of this at mode set time
	for(s = &surfaces[1]; s < surface_p; s++){
		if (!s->spans)
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

		blend = (s->flags & SURF_TRANS) || (r_drawflags & DRAW_BLEND);

		dvars.zistepu = s->d_zistepu;
		dvars.zistepv = s->d_zistepv;
		dvars.ziorigin = s->d_ziorigin;

		if(s->insubmodel){
			// FIXME: we don't want to do all this for every polygon!
			// TODO: store once at start of frame
			currententity = s->entity;	//FIXME: make this passed in to
										// R_RotateBmodel()
			VectorSubtract(r_origin, currententity->origin, local_modelorg);
			TransformVector(local_modelorg, transformed_modelorg);

			R_RotateBmodel(s->entity);	// FIXME: don't mess with the frustum,
								// make entity passed in
		}

		if(s->flags & SURF_DRAWSKY){
			D_DrawSkyScans8(s->spans);
			dvars.ziorigin = -0.8;
			D_DrawZSpans(s->spans);
		}else if(s->flags & SURF_DRAWBACKGROUND){
			// set up a gradient for the background surface that places it
			// effectively at infinity distance from the viewpoint
			dvars.zistepu = 0;
			dvars.zistepv = 0;
			dvars.ziorigin = -0.9;

			D_DrawSolidSurface(s, q1pal[(int)r_clearcolor.value & 0xFF]);
			D_DrawZSpans(s->spans);
		}else if(s->flags & SURF_DRAWTURB){
			pface = s->data;
			miplevel = 0;
			dvars.cacheblock = pface->texinfo->texture->pixels + pface->texinfo->texture->offsets[0];
			dvars.cachewidth = 64;

			D_CalcGradients (pface, transformed_modelorg);
			Turbulent8 (s->spans, alpha);
			if(!blend)
				D_DrawZSpans (s->spans);
		}else{
			pface = s->data;
			miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);
			if(s->flags & SURF_FENCE)
				miplevel = max(miplevel-1, 0);

			// FIXME: make this passed in to D_CacheSurface
			pcurrentcache = D_CacheSurface(pface, miplevel);

			dvars.cacheblock = pcurrentcache->pixels;
			dvars.cachewidth = pcurrentcache->width;

			D_CalcGradients(pface, transformed_modelorg);

			D_DrawSpans16(s->spans, blend, alpha);
			if(!blend)
				D_DrawZSpans(s->spans);
		}

		if(s->insubmodel){
			// restore the old drawing state
			// FIXME: we don't want to do this every time!
			// TODO: speed up
			currententity = &cl_entities[0];
			VectorCopy(world_transformed_modelorg, transformed_modelorg);
			VectorCopy(base_vpn, vpn);
			VectorCopy(base_vup, vup);
			VectorCopy(base_vright, vright);
			VectorCopy(base_modelorg, modelorg);
			R_TransformFrustum();
		}
	}
}

