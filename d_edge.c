#include "quakedef.h"

static int	miplevel;

float scale_for_mip;

// FIXME: should go away
extern void R_RotateBmodel (void);
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
		pdest = d_viewbuffer + screenwidth*span->v;
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
	d_sdivzstepu = p_saxis[0] * t;
	d_tdivzstepu = p_taxis[0] * t;

	t = yscaleinv * mipscale;
	d_sdivzstepv = -p_saxis[1] * t;
	d_tdivzstepv = -p_taxis[1] * t;

	d_sdivzorigin = p_saxis[2] * mipscale - xcenter * d_sdivzstepu -
			ycenter * d_sdivzstepv;
	d_tdivzorigin = p_taxis[2] * mipscale - xcenter * d_tdivzstepu -
			ycenter * d_tdivzstepv;

	VectorScale (transformed_modelorg, mipscale, p_temp1);

	t = 0x10000*mipscale;
	sadjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
			((pface->texturemins[0] << 16) >> miplevel)
			+ pface->texinfo->vecs[0][3]*t;
	tadjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
			((pface->texturemins[1] << 16) >> miplevel)
			+ pface->texinfo->vecs[1][3]*t;

	// -1 (-epsilon) so we never wander off the edge of the texture
	bbextents = ((pface->extents[0] << 16) >> miplevel) - 1;
	bbextentt = ((pface->extents[1] << 16) >> miplevel) - 1;
}

void
D_DrawSurfaces (void)
{
	surf_t *s;
	msurface_t *pface;
	surfcache_t *pcurrentcache;
	vec3_t world_transformed_modelorg, local_modelorg, transformed_modelorg;
	byte alpha;

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

		d_zistepu = s->d_zistepu;
		d_zistepv = s->d_zistepv;
		d_ziorigin = s->d_ziorigin;

		if(s->flags & SURF_DRAWSKY){
			D_DrawSkyScans8(s->spans);
			D_DrawZSpans(s->spans);
		}else if(s->flags & SURF_DRAWBACKGROUND){
			// set up a gradient for the background surface that places it
			// effectively at infinity distance from the viewpoint
			d_zistepu = 0;
			d_zistepv = 0;
			d_ziorigin = -0.9;

			D_DrawSolidSurface(s, q1pal[(int)r_clearcolor.value & 0xFF]);
			D_DrawZSpans(s->spans);
		}else if(s->flags & SURF_DRAWTURB){
			pface = s->data;
			miplevel = 0;
			cacheblock = pface->texinfo->texture->pixels + pface->texinfo->texture->offsets[0];
			cachewidth = 64;

			if(s->insubmodel){
				// FIXME: we don't want to do all this for every polygon!
				// TODO: store once at start of frame
				currententity = s->entity;	//FIXME: make this passed in to
											// R_RotateBmodel()
				VectorSubtract(r_origin, currententity->origin, local_modelorg);
				TransformVector(local_modelorg, transformed_modelorg);

				R_RotateBmodel();	// FIXME: don't mess with the frustum,
									// make entity passed in
			}

			D_CalcGradients (pface, transformed_modelorg);
			Turbulent8 (s->spans, alpha);
			D_DrawZSpans (s->spans);

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
		else
		{
			if(s->insubmodel){
				// FIXME: we don't want to do all this for every polygon!
				// TODO: store once at start of frame
				currententity = s->entity;	//FIXME: make this passed in to
											// R_RotateBmodel()
				VectorSubtract(r_origin, currententity->origin, local_modelorg);
				TransformVector(local_modelorg, transformed_modelorg);
				R_RotateBmodel();	// FIXME: don't mess with the frustum,
									// make entity passed in
			}

			pface = s->data;
			if(s->flags & SURF_FENCE)
				miplevel = 0;
			else
				miplevel = D_MipLevelForScale(s->nearzi * scale_for_mip * pface->texinfo->mipadjust);

			// FIXME: make this passed in to D_CacheSurface
			pcurrentcache = D_CacheSurface(pface, miplevel);

			cacheblock = pcurrentcache->pixels;
			cachewidth = pcurrentcache->width;

			D_CalcGradients(pface, transformed_modelorg);

			D_DrawSpans16(s->spans, s->flags & SURF_FENCE, alpha);
			if((r_drawflags & DRAW_BLEND) == 0)
				D_DrawZSpans(s->spans);

			if(s->insubmodel){
				//
				// restore the old drawing state
				// FIXME: we don't want to do this every time!
				// TODO: speed up
				//
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
}

