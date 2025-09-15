#include "quakedef.h"

static vec3_t viewlightvec;
static alight_t r_viewlighting = {viewlightvec, {128, 128, 128}, {192, 192, 192}};

void *colormap;
int r_numallocatededges;
int r_numallocatedbasespans;
void *r_basespans;
bool r_recursiveaffinetriangles = true;
float r_aliasuvscale = 1.0;
int r_outofsurfaces;
int r_outofedges;
int r_outofspans;
bool r_frame_not_full, r_dowarp, r_dowarpold, r_viewchanged;
mvertex_t *r_pcurrentvertbase;
int r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
int r_clipflags;
pixel_t *r_warpbuffer;

// screen size info
refdef_t r_refdef;
float xcenter, ycenter;
float xscale, yscale;
float xscaleinv, yscaleinv;
float xscaleshrink, yscaleshrink;
float aliasxscale, aliasyscale, aliasxcenter, aliasycenter;
mplane_t screenedge[4];

// refresh flags
int r_framecount = 1; // so frame counts initialized to 0 don't match
int r_visframecount;

int *pfrustum_indexes[4];
int r_frustum_indexes[4 * 6];

mleaf_t *r_viewleaf, *r_oldviewleaf;

texture_t *r_notexture_mip;

float r_aliastransition, r_resfudge;

int d_lightstylevalue[256]; // 8.8 fraction of base light value

static cvar_t r_drawentities = {"r_drawentities", "1"};
static cvar_t r_drawviewmodel = {"r_drawviewmodel", "1"};
static cvar_t r_aliastransbase = {"r_aliastransbase", "200"};
static cvar_t r_aliastransadj = {"r_aliastransadj", "100"};

cvar_t r_clearcolor = {"r_clearcolor", "2"};
cvar_t r_waterwarp = {"r_waterwarp", "1"};
cvar_t r_fullbright = {"r_fullbright", "0"};
cvar_t r_ambient = {"r_ambient", "0"};
cvar_t r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t r_numsurfs = {"r_numsurfs", "0"};
cvar_t r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t r_numedges = {"r_numedges", "0"};
cvar_t r_part_scale = {"r_part_scale", "1", true};
cvar_t r_wateralpha = {"r_wateralpha", "1", true};
cvar_t r_lavaalpha = {"r_lavaalpha", "1", true};
cvar_t r_slimealpha = {"r_slimealpha", "1", true};

extern cvar_t scr_fov;

static entity_t *ent_reject;

void
R_MissingTexture(texture_t *t)
{
	int w, h, x, y, j;
	pixel_t *dest;

	w = t->width;
	h = t->height;
	dest = t->pixels + t->offsets[0];
	for(y = 0; y < h; y++) {
		for(x = 0; x < w; x++, dest++) {
			if((y < (h / 2)) ^ (x < (w / 2)))
				*dest = 0xff000000;
			else
				*dest = 0xfff0f0f0;
		}
	}
	for(j = 1; j < MIPLEVELS; j++) {
		t->offsets[j] = t->offsets[j - 1] + w * h;
		w /= 2;
		h /= 2;
		pixels_resize(
			t->pixels + t->offsets[0],
			t->pixels + t->offsets[j],
			t->width,
			t->height,
			w,
			h,
			false,
			false
		);
	}
}

void
R_InitTextures(void)
{
	// create a simple checkerboard texture for the default
	r_notexture_mip =
		Hunk_Alloc(sizeof(*r_notexture_mip) + (16 * 16 + 8 * 8 + 4 * 4 + 2 * 2) * sizeof(pixel_t));

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = 0;

	R_MissingTexture(r_notexture_mip);
}

static void
R_LoadSky_f(cmd_t *c)
{
	USED(c);
	if(cl.worldmodel == nil || Cmd_Argc() != 2) {
		Con_Printf("usage: loadsky <skyname>\n");
		return;
	}
	R_LoadSkyBox(Cmd_Argv(1), cl.worldmodel);
}

void
R_Init(void)
{
	R_InitTurb();
	R_InitFog();

	Cmd_AddCommand("pointfile", loadpoints);
	Cmd_AddCommand("loadsky", R_LoadSky_f);
	Cmd_AddCommand("sky", R_LoadSky_f)->hidden = true; // for AD

	Cvar_RegisterVariable(&r_ambient);
	Cvar_RegisterVariable(&r_clearcolor);
	Cvar_RegisterVariable(&r_waterwarp);
	Cvar_RegisterVariable(&r_fullbright);
	Cvar_RegisterVariable(&r_drawentities);
	Cvar_RegisterVariable(&r_drawviewmodel);
	Cvar_RegisterVariable(&r_reportsurfout);
	Cvar_RegisterVariable(&r_numsurfs);
	Cvar_RegisterVariable(&r_reportedgeout);
	Cvar_RegisterVariable(&r_numedges);
	Cvar_RegisterVariable(&r_aliastransbase);
	Cvar_RegisterVariable(&r_aliastransadj);
	Cvar_RegisterVariable(&r_part_scale);
	Cvar_RegisterVariable(&r_wateralpha);
	Cvar_RegisterVariable(&r_lavaalpha);
	Cvar_RegisterVariable(&r_slimealpha);

	r_refdef.view.clipplanes[0].leftedge = true;
	r_refdef.view.clipplanes[1].rightedge = true;
	r_refdef.view.clipplanes[1].leftedge = r_refdef.view.clipplanes[2].leftedge =
		r_refdef.view.clipplanes[3].leftedge = false;
	r_refdef.view.clipplanes[0].rightedge = r_refdef.view.clipplanes[2].rightedge =
		r_refdef.view.clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	R_InitParticles();
	D_Init();
}

void
R_NewMap(void)
{
	int i;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for(i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = nil;

	r_numallocatedbasespans = MAXSPANS;
	r_cnumsurfs = MAXSURFACES;
	r_numallocatededges = MAXEDGES;
	r_basespans = Hunk_Alloc(r_numallocatedbasespans * sizeof(espan_t));
	surfaces = Hunk_Alloc(r_cnumsurfs * sizeof(*surfaces));
	r_edges = Hunk_Alloc(r_numallocatededges * sizeof(*r_edges));

	r_viewleaf = nil;
	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;
	r_dowarpold = false;
	r_viewchanged = false;

	if(!sv.active)
		R_InitSky(nil);
	R_ClearParticles();
	R_ApplyFog(cl.worldmodel);
	R_LoadSkyBox(nil, cl.worldmodel);
}

static void
R_SetVrect(vrect_t *pvrectin, vrect_t *pvrect)
{
	int h;
	float size;

	size = 1;

	h = pvrectin->height;
	pvrect->width = pvrectin->width;
	if(pvrect->width < 96) {
		size = 96.0 / pvrectin->width;
		pvrect->width = 96; // min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if(pvrect->height > pvrectin->height)
		pvrect->height = pvrectin->height;

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width) / 2;
	pvrect->y = (h - pvrect->height) / 2;
}

/*
 * Called every time the vid structure or r_refdef changes.
 * Guaranteed to be called before the first refresh
 */
void
R_ViewChanged(vrect_t *pvrect, float pixelAspect)
{
	int i, ow, oh;
	float res_scale, verticalFieldOfView, screenAspect;

	r_viewchanged = true;

	ow = r_refdef.vrect.width;
	oh = r_refdef.vrect.height;
	R_SetVrect(pvrect, &r_refdef.vrect);
	if(ow != r_refdef.vrect.width || oh != r_refdef.vrect.height)
		vid.recalc_refdef = true;

	r_refdef.horizontalFieldOfView = 2.0 * tanf(r_refdef.fov_x / 360 * M_PI);
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x << 20) + (1 << 19) - 1;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright << 20) + (1 << 19) - 1;
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x + r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y + r_refdef.aliasvrect.height;

	screenAspect = r_refdef.vrect.width * pixelAspect / r_refdef.vrect.height;
	// 320*200 1.0 pixelAspect = 1.6 screenAspect
	// 320*240 1.0 pixelAspect = 1.3333 screenAspect
	// proper 320*200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

	// values for perspective projection
	// if math were exact, the values would range from 0.5 to to range+0.5
	// hopefully they wll be in the 0.000001 to range+.999999 and truncate
	// the polygon rasterization will never render in the first row or column
	// but will definately render in the [range] row and column, so adjust the
	// buffer origin to get an exact edge to edge fill
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) + r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) + r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink * pixelAspect;

	// left side clip
	screenedge[0].normal[0] = -1.0 / (r_refdef.xOrigin * r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

	// right side clip
	screenedge[1].normal[0] = 1.0 / ((1.0 - r_refdef.xOrigin) * r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

	// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (r_refdef.yOrigin * verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0 - r_refdef.yOrigin) * verticalFieldOfView);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for(i = 0; i < 4; i++)
		VectorNormalize(screenedge[i].normal);

	res_scale = sqrtf((float)(r_refdef.vrect.width * r_refdef.vrect.height) / (320.0 * 152.0)) *
				(2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

	D_ViewChanged();
}

static void
R_MarkLeaves(void)
{
	byte *vis;
	mnode_t *node;
	int i, sz;

	if(r_oldviewleaf == r_viewleaf)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel, &sz);

	for(i = 0; i < cl.worldmodel->numleafs; i++) {
		if(vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
			do {
				if(node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while(node);
		}
	}
}

static int
R_BmodelCheckBBox(model_t *clmodel, float *minmaxs, view_t *v)
{
	int i, *pindex, clipflags;
	vec3_t acceptpt, rejectpt;
	double d;

	clipflags = 0;

	if(currententity->angles[0] || currententity->angles[1] || currententity->angles[2]) {
		for(i = 0; i < 4; i++) {
			d = DotProduct(currententity->origin, v->clipplanes[i].normal);
			d -= v->clipplanes[i].dist;

			if(d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if(d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	} else {
		for(i = 0; i < 4; i++) {
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the sign bit
			// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct(rejectpt, v->clipplanes[i].normal);
			d -= v->clipplanes[i].dist;

			if(d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct(acceptpt, v->clipplanes[i].normal);
			d -= v->clipplanes[i].dist;

			if(d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}

static int
R_DrawEntity(entity_t *e, view_t *view)
{
	int j, k, clipflags, add;
	alight_t lighting;
	vec3_t dist, lightvec = {-1, 0, 0}; // FIXME: remove and do real lighting
	float minmaxs[6];
	model_t *clmodel;
	view_t v;

	if(!r_drawentities.value || e == &cl_entities[cl.viewentity])
		return 1;
	if((entdrawflags(e) ^ r_drawflags) != 0)
		return 0;
	if(e->alpha == DEFAULT_ALPHA)
		e->alpha = 0xff;

	currententity = e;
	memmove(&v, view, sizeof(v));

	switch(e->model->type) {
	case mod_sprite:
		VectorCopy(e->origin, r_entorigin);
		VectorSubtract(r_refdef.view.org, r_entorigin, v.modelorg);
		R_DrawSprite(&v);
		break;

	case mod_alias:
		VectorCopy(e->origin, r_entorigin);
		VectorSubtract(r_refdef.view.org, r_entorigin, v.modelorg);

		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		if(!R_AliasCheckBBox(&v))
			break;

		R_LightPoint(currententity->origin, lighting.ambientlight);
		lighting.shadelight[0] = lighting.ambientlight[0];
		lighting.shadelight[1] = lighting.ambientlight[1];
		lighting.shadelight[2] = lighting.ambientlight[2];
		lighting.plightvec = lightvec;

		for(k = 0; k < MAX_DLIGHTS; k++) {
			if(cl_dlights[k].die < cl.time || cl_dlights[k].radius == 0)
				continue;
			VectorSubtract(e->origin, cl_dlights[k].origin, dist);
			add = cl_dlights[k].radius - Length(dist);
			if(add > 0) {
				lighting.ambientlight[0] += add;
				lighting.ambientlight[1] += add;
				lighting.ambientlight[2] += add;
			}
		}

		// clamp lighting so it doesn't overbright as much
		for(j = 0; j < 3; j++) {
			lighting.ambientlight[j] = min(lighting.ambientlight[j], 128);
			if(lighting.ambientlight[j] + lighting.shadelight[j] > 192)
				lighting.shadelight[j] = 192 - lighting.ambientlight[j];
		}

		R_AliasDrawModel(&lighting, &v);
		break;

	case mod_brush:
		r_dlightframecount = r_framecount;
		clmodel = e->model;

		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		for(j = 0; j < 3; j++) {
			minmaxs[j] = currententity->origin[j] + clmodel->mins[j];
			minmaxs[3 + j] = currententity->origin[j] + clmodel->maxs[j];
		}

		clipflags = R_BmodelCheckBBox(clmodel, minmaxs, &v);
		if(clipflags == BMODEL_FULLY_CLIPPED)
			break;

		VectorCopy(e->origin, r_entorigin);
		VectorSubtract(r_refdef.view.org, r_entorigin, v.modelorg);
		r_pcurrentvertbase = clmodel->vertexes;
		R_RotateBmodel(e, &v); // FIXME: stop transforming twice

		// calculate dynamic lighting for bmodel if it's not an
		// instanced model
		if(clmodel->firstmodelsurface != 0) {
			for(k = 0; k < MAX_DLIGHTS; k++) {
				if(cl_dlights[k].die < cl.time || cl_dlights[k].radius == 0)
					continue;
				R_MarkLights(
					&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->hulls[0].firstclipnode
				);
			}
		}

		r_pefragtopnode = nil;

		for(j = 0; j < 3; j++) {
			r_emins[j] = minmaxs[j];
			r_emaxs[j] = minmaxs[3 + j];
		}

		R_SplitEntityOnNode2(cl.worldmodel->nodes);

		if(r_pefragtopnode) {
			e->topnode = r_pefragtopnode;

			if(r_drawflags & DRAW_BLEND)
				R_BeginEdgeFrame();
			if(r_pefragtopnode->contents >= 0) {
				// not a leaf; has to be clipped to the world BSP
				r_clipflags = clipflags;
				R_DrawSolidClippedSubmodelPolygons(clmodel, &v);
			} else {
				// falls entirely in one leaf, so we just put all the
				// edges in the edge list and let 1/z sorting handle
				// drawing order
				R_DrawSubmodelPolygons(clmodel, &v, clipflags);
			}
			if(r_drawflags & DRAW_BLEND)
				R_ScanEdges(&v);

			e->topnode = nil;
		}
		break;
	}

	return 1;
}

static void
R_DrawViewModel(view_t *view)
{
	// FIXME: remove and do real lighting
	vec3_t dist, lightvec = {-1, 0, 0}, q;
	int lnum, i;
	float add;
	dlight_t *dl;
	view_t v;

	if(!r_drawviewmodel.value)
		return;

	if(cl.items & IT_INVISIBILITY)
		return;

	if(cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if(!currententity->model)
		return;

	memmove(&v, view, sizeof(v));
	VectorCopy(currententity->origin, r_entorigin);
	VectorSubtract(v.org, r_entorigin, v.modelorg);
	if(scr_fov.value > 90.0) {
		// adjust a little bit so it doesn't look as if the weapon
		// is floating in space on its own
		VectorAdd(v.pn, v.up, q);
		VectorScale(q, (scr_fov.value - 90.0) / 10.0, q);
		VectorAdd(v.modelorg, q, v.modelorg);
	}

	VectorCopy(v.up, viewlightvec);
	VectorInverse(viewlightvec);

	R_LightPoint(currententity->origin, r_viewlighting.ambientlight);
	// always give some light on gun
	r_viewlighting.ambientlight[0] = max(r_viewlighting.ambientlight[0], 24);
	r_viewlighting.ambientlight[1] = max(r_viewlighting.ambientlight[1], 24);
	r_viewlighting.ambientlight[2] = max(r_viewlighting.ambientlight[2], 24);
	r_viewlighting.shadelight[0] = r_viewlighting.ambientlight[0];
	r_viewlighting.shadelight[1] = r_viewlighting.ambientlight[1];
	r_viewlighting.shadelight[2] = r_viewlighting.ambientlight[2];

	// add dynamic lights
	for(lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		dl = &cl_dlights[lnum];
		if(dl->die < cl.time || !dl->radius)
			continue;

		VectorSubtract(currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if(add > 0) {
			r_viewlighting.ambientlight[0] += add;
			r_viewlighting.ambientlight[1] += add;
			r_viewlighting.ambientlight[2] += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	for(i = 0; i < 3; i++) {
		r_viewlighting.ambientlight[i] = min(r_viewlighting.ambientlight[i], 128);
		if(r_viewlighting.ambientlight[i] + r_viewlighting.shadelight[i] > 192)
			r_viewlighting.shadelight[i] = 192 - r_viewlighting.ambientlight[i];
	}

	r_viewlighting.plightvec = lightvec;

	R_AliasDrawModel(&r_viewlighting, &v);
}

static void
R_RenderSolidBrushes(view_t *v)
{
	entity_t *e;
	int i;

	R_BeginEdgeFrame();
	R_RenderWorld(v);
	for(i = 0; i < cl_numvisedicts; i++) {
		e = cl_visedicts[i];
		if(e->model != nil && e->model->type == mod_brush && !R_DrawEntity(e, v)) {
			e->last_reject = ent_reject;
			ent_reject = e;
		}
	}
	R_ScanEdges(v);
}

/* r_refdef must be set before the first call */
void
R_RenderView(void)
{
	entity_t *e;
	int i;
	view_t *v;

	R_SetupFrame();
	R_MarkLeaves(); // done here so we know if we're in water

	if(!cl_entities[0].model || !cl.worldmodel)
		fatal("R_RenderView: NULL worldmodel");
	ent_reject = nil;
	v = &r_refdef.view;

	R_RenderSolidBrushes(v);
	for(i = 0; i < cl_numvisedicts; i++) {
		e = cl_visedicts[i];
		if(e->model->type != mod_brush && !R_DrawEntity(e, v)) {
			e->last_reject = ent_reject;
			ent_reject = e;
		}
	}

	R_DrawViewModel(v);
	r_drawflags = DRAW_BLEND;
	R_DrawParticles();

	R_RenderBlendedBrushes(v);
	// FIXME(sigrid): these need to be sorted with blended brushes and *all* drawn back to front
	if(cl_numvisedicts > 0) {
		for(i = cl_numvisedicts, e = ent_reject; e != nil && i > 0; e = e->last_reject) {
			if(!enthasalpha(e))
				R_DrawEntity(e, v);
			else
				cl_visedicts[--i] = e;
		}
		for(; i < cl_numvisedicts; i++)
			R_DrawEntity(cl_visedicts[i], v);
	}
	r_drawflags = 0;

	if(r_dowarp)
		D_WarpScreen();

	V_SetContentsColor(r_viewleaf->contents);

	if(r_reportsurfout.value && r_outofsurfaces)
		Con_Printf("Short %d surfaces\n", r_outofsurfaces);

	if(r_reportedgeout.value && r_outofedges)
		Con_Printf("Short roughly %d edges\n", r_outofedges * 2 / 3);
}

void
R_InitTurb(void)
{
	int i;

	for(i = 0; i < SIN_BUFFER_SIZE; i++) {
		sintable[i] = AMP + sinf(i * M_PI * 2 / TURB_CYCLE) * AMP;
		intsintable[i] = AMP2 + sinf(i * M_PI * 2 / TURB_CYCLE) * AMP2; // AMP2, not 20
	}
}
