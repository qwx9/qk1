#include "quakedef.h"

void		*colormap;
static vec3_t viewlightvec;
static alight_t r_viewlighting = {{128,128,128}, {192,192,192}, viewlightvec};
int			r_numallocatededges;
int			r_numallocatedbasespans;
void		*r_basespans;
bool	r_recursiveaffinetriangles = true;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;
int			r_outofspans;

bool	r_dowarp, r_dowarpold, r_viewchanged;

mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
bool	r_surfsonstack;
int			r_clipflags;

pixel_t *r_warpbuffer;

//
// view origin
//
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

//
// screen size info
//
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		screenwidth;

float	pixelAspect;
float	screenAspect;
float	xOrigin, yOrigin;

static float verticalFieldOfView;

mplane_t	screenedge[4];

//
// refresh flags
//
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

float		r_aliastransition, r_resfudge;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

void R_MarkLeaves (void);

static cvar_t r_drawentities = {"r_drawentities","1"};
static cvar_t r_drawviewmodel = {"r_drawviewmodel","1"};

cvar_t	r_clearcolor = {"r_clearcolor","2"};
cvar_t	r_waterwarp = {"r_waterwarp","1"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_ambient = {"r_ambient", "0"};
cvar_t	r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t	r_numsurfs = {"r_numsurfs", "0"};
cvar_t	r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t	r_numedges = {"r_numedges", "0"};
static cvar_t r_aliastransbase = {"r_aliastransbase", "200"};
static cvar_t r_aliastransadj = {"r_aliastransadj", "100"};
cvar_t	r_part_scale = {"r_part_scale", "1", true};
cvar_t	r_wateralpha = {"r_wateralpha", "1", true};
cvar_t	r_lavaalpha = {"r_lavaalpha", "1", true};
cvar_t	r_slimealpha = {"r_slimealpha", "1", true};

extern cvar_t	scr_fov;

void CreatePassages (void);
void SetVisibilityByPassages (void);

static entity_t *ent_reject;

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	pixel_t	*dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_Alloc((16*16+8*8+4*4+2*2)*sizeof(pixel_t) + sizeof *r_notexture_mip);

	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = 0;
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;

	for (m=0 ; m<4 ; m++)
	{
		dest = r_notexture_mip->pixels + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++, dest++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest = 0;
				else
					*dest = 0xf0f0f0f0;
			}
	}
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{
	R_InitTurb();
	R_InitFog();

	Cmd_AddCommand("pointfile", loadpoints);

	Cvar_RegisterVariable (&r_ambient);
	Cvar_RegisterVariable (&r_clearcolor);
	Cvar_RegisterVariable (&r_waterwarp);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_reportsurfout);
	Cvar_RegisterVariable (&r_numsurfs);
	Cvar_RegisterVariable (&r_reportedgeout);
	Cvar_RegisterVariable (&r_numedges);
	Cvar_RegisterVariable (&r_aliastransbase);
	Cvar_RegisterVariable (&r_aliastransadj);
	Cvar_RegisterVariable (&r_part_scale);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_lavaalpha);
	Cvar_RegisterVariable (&r_slimealpha);

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge =
			view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge =
			view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	R_InitParticles ();

	D_Init ();
}

void R_SetupSurfaces(void);

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = nil;

	r_numallocatedbasespans = MAXSPANS;
	r_cnumsurfs = MAXSURFACES;
	r_numallocatededges = MAXEDGES;
	r_basespans = Hunk_Alloc(r_numallocatedbasespans * sizeof(espan_t));
	surfaces = Hunk_Alloc(r_cnumsurfs * sizeof *surfaces);
	r_edges = Hunk_Alloc(r_numallocatededges * sizeof *r_edges);

	r_viewleaf = nil;
	R_ClearParticles ();
	R_ResetFog();
	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;
	r_dowarpold = false;
	r_viewchanged = false;
}


/*
===============
R_SetVrect
===============
*/
void R_SetVrect (vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
	int		h;
	float	size;

	if(cl.intermission)
		lineadj = 0;
	size = 1;

	h = pvrectin->height - lineadj;
	pvrect->width = pvrectin->width;
	if (pvrect->width < 96)
	{
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;	// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if (pvrect->height > pvrectin->height - lineadj)
		pvrect->height = pvrectin->height - lineadj;

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width)/2;
	pvrect->y = (h - pvrect->height)/2;

	{
		if (lcd_x.value)
		{
			pvrect->y >>= 1;
			pvrect->height >>= 1;
		}
	}
}


/*
===============
R_ViewChanged

Called every time the vid structure or r_refdef changes.
Guaranteed to be called before the first refresh
===============
*/
void R_ViewChanged (vrect_t *pvrect, int lineadj, float aspect)
{
	int		i, ow, oh;
	float	res_scale;

	r_viewchanged = true;

	ow = r_refdef.vrect.width;
	oh = r_refdef.vrect.height;
	R_SetVrect (pvrect, &r_refdef.vrect, lineadj);
	if(ow != r_refdef.vrect.width || oh != r_refdef.vrect.height)
		vid.recalc_refdef = true;

	r_refdef.horizontalFieldOfView = 2.0 * tanf(r_refdef.fov_x/360*M_PI);
	r_refdef.fvrectx = (float)r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float)r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x<<20) + (1<<19) - 1;
	r_refdef.fvrecty = (float)r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float)r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright<<20) + (1<<19) - 1;
	r_refdef.fvrectright = (float)r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float)r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float)r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float)r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float)r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int)(r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int)(r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int)(r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int)(r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x +
			r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y +
			r_refdef.aliasvrect.height;

	pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;

	screenAspect = r_refdef.vrect.width*pixelAspect /
			r_refdef.vrect.height;
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
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) +
			r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) +
			r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width-6)/r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink*pixelAspect;

	// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin*r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

	// right side clip
	screenedge[1].normal[0] =
			1.0 / ((1.0-xOrigin)*r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

	// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin*verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0-yOrigin)*verticalFieldOfView);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for (i=0 ; i<4 ; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrtf((float)(r_refdef.vrect.width * r_refdef.vrect.height) /
			          (320.0 * 152.0)) *
			(2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

	D_ViewChanged ();
}


/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i, sz;

	if (r_oldviewleaf == r_viewleaf)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel, &sz);

	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

static int
R_BmodelCheckBBox(model_t *clmodel, float *minmaxs)
{
	int			i, *pindex, clipflags;
	vec3_t		acceptpt, rejectpt;
	double		d;

	clipflags = 0;

	if (currententity->angles[0] || currententity->angles[1]
		|| currententity->angles[2])
	{
		for (i=0 ; i<4 ; i++)
		{
			d = DotProduct (currententity->origin, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1<<i);
		}
	}
	else
	{
		for (i=0 ; i<4 ; i++)
		{
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the sign bit
			// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3+0]];
			acceptpt[1] = minmaxs[pindex[3+1]];
			acceptpt[2] = minmaxs[pindex[3+2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1<<i);
		}
	}

	return clipflags;
}

static int
R_DrawEntity(entity_t *e)
{
	int			j, k, clipflags;
	alight_t	lighting;
	float		lightvec[3] = {-1, 0, 0}; // FIXME: remove and do real lighting
	float		minmaxs[6];
	vec3_t		dist, oldorigin;
	int		add;
	model_t		*clmodel;

	if(!r_drawentities.value || e == &cl_entities[cl.viewentity])
		return 1;
	if((entdrawflags(e) ^ r_drawflags) != 0)
		return 0;

	currententity = e;
	VectorCopy(modelorg, oldorigin);

	switch(e->model->type){
	case mod_sprite:
		VectorCopy(e->origin, r_entorigin);
		VectorSubtract(r_origin, r_entorigin, modelorg);
		R_DrawSprite();
		break;

	case mod_alias:
		VectorCopy(e->origin, r_entorigin);
		VectorSubtract(r_origin, r_entorigin, modelorg);

		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		if(!R_AliasCheckBBox())
			break;

		R_LightPoint(currententity->origin, lighting.ambientlight);
		lighting.shadelight[0] = lighting.ambientlight[0];
		lighting.shadelight[1] = lighting.ambientlight[1];
		lighting.shadelight[2] = lighting.ambientlight[2];
		lighting.plightvec = lightvec;

		for(k = 0; k < MAX_DLIGHTS; k++){
			if(cl_dlights[k].die < cl.time || cl_dlights[k].radius == 0)
				continue;
			VectorSubtract(e->origin, cl_dlights[k].origin, dist);
			add = cl_dlights[k].radius - Length(dist);
			if(add > 0){
				lighting.ambientlight[0] += add;
				lighting.ambientlight[1] += add;
				lighting.ambientlight[2] += add;
			}
		}

		// clamp lighting so it doesn't overbright as much
		for(j = 0; j < 3; j++){
			lighting.ambientlight[j] = min(lighting.ambientlight[j], 128);
			if(lighting.ambientlight[j] + lighting.shadelight[j] > 192)
				lighting.shadelight[j] = 192 - lighting.ambientlight[j];
		}

		R_AliasDrawModel(&lighting);
		break;

	case mod_brush:
		insubmodel = true;
		r_dlightframecount = r_framecount;
		clmodel = e->model;

		// see if the bounding box lets us trivially reject, also sets
		// trivial accept status
		for(j = 0; j < 3; j++){
			minmaxs[j] = currententity->origin[j] + clmodel->mins[j];
			minmaxs[3+j] = currententity->origin[j] + clmodel->maxs[j];
		}

		clipflags = R_BmodelCheckBBox(clmodel, minmaxs);
		if(clipflags == BMODEL_FULLY_CLIPPED)
			break;

		VectorCopy(e->origin, r_entorigin);
		VectorSubtract(r_origin, r_entorigin, modelorg);
		r_pcurrentvertbase = clmodel->vertexes;
		R_RotateBmodel(); // FIXME: stop transforming twice

		// calculate dynamic lighting for bmodel if it's not an
		// instanced model
		if(clmodel->firstmodelsurface != 0){
			for(k = 0; k < MAX_DLIGHTS; k++){
				if(cl_dlights[k].die < cl.time || cl_dlights[k].radius == 0)
					continue;
				R_MarkLights(&cl_dlights[k], 1<<k, clmodel->nodes + clmodel->hulls[0].firstclipnode);
			}
		}

		r_pefragtopnode = nil;

		for(j = 0; j < 3; j++){
			r_emins[j] = minmaxs[j];
			r_emaxs[j] = minmaxs[3+j];
		}

		R_SplitEntityOnNode2(cl.worldmodel->nodes);

		if(r_pefragtopnode){
			e->topnode = r_pefragtopnode;

			if(r_drawflags & DRAW_BLEND)
				R_BeginEdgeFrame();
			if(r_pefragtopnode->contents >= 0){
				// not a leaf; has to be clipped to the world BSP
				r_clipflags = clipflags;
				R_DrawSolidClippedSubmodelPolygons(clmodel);
			}else{
				// falls entirely in one leaf, so we just put all the
				// edges in the edge list and let 1/z sorting handle
				// drawing order
				R_DrawSubmodelPolygons(clmodel, clipflags);
			}
			if(r_drawflags & DRAW_BLEND)
				R_ScanEdges();

			e->topnode = nil;
		}
		break;
	}

	// put back world rotation and frustum clipping
	// FIXME: R_RotateBmodel should just work off base_vxx
	VectorCopy(base_vpn, vpn);
	VectorCopy(base_vup, vup);
	VectorCopy(base_vright, vright);
	VectorCopy(base_modelorg, modelorg);
	VectorCopy(oldorigin, modelorg);
	R_TransformFrustum();

	return 1;
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	// FIXME: remove and do real lighting
	float		lightvec[3] = {-1, 0, 0};
	int			lnum, i;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;

	if (!r_drawviewmodel.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	VectorCopy (vup, viewlightvec);
	VectorInverse (viewlightvec);

	R_LightPoint(currententity->origin, r_viewlighting.ambientlight);
	// always give some light on gun
	r_viewlighting.ambientlight[0] = max(r_viewlighting.ambientlight[0], 24);
	r_viewlighting.ambientlight[1] = max(r_viewlighting.ambientlight[1], 24);
	r_viewlighting.ambientlight[2] = max(r_viewlighting.ambientlight[2], 24);
	r_viewlighting.shadelight[0] = r_viewlighting.ambientlight[0];
	r_viewlighting.shadelight[1] = r_viewlighting.ambientlight[1];
	r_viewlighting.shadelight[2] = r_viewlighting.ambientlight[2];

	// add dynamic lights
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if(dl->die < cl.time || !dl->radius)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0){
			r_viewlighting.ambientlight[0] += add;
			r_viewlighting.ambientlight[1] += add;
			r_viewlighting.ambientlight[2] += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	for(i = 0; i < 3; i++){
		r_viewlighting.ambientlight[i] = min(r_viewlighting.ambientlight[i], 128);
		if(r_viewlighting.ambientlight[i] + r_viewlighting.shadelight[i] > 192)
			r_viewlighting.shadelight[i] = 192 - r_viewlighting.ambientlight[i];
	}

	r_viewlighting.plightvec = lightvec;

	R_AliasDrawModel(&r_viewlighting);
}

/*
================
R_EdgeDrawing
================
*/
void R_EdgeDrawing (void)
{
	entity_t *e;
	int i;

	R_BeginEdgeFrame();
	if(r_drawflags & DRAW_BLEND)
		R_RenderWorldRejects();
	else{
		R_RenderWorld();
		for(i = 0; i < cl_numvisedicts; i++){
			e = cl_visedicts[i];
			if(e->model != nil && e->model->type == mod_brush && !R_DrawEntity(e)){
				e->last_reject = ent_reject;
				ent_reject = e;
			}
		}
	}
	R_ScanEdges();
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	entity_t *e;
	int i;

	R_SetupFrame ();
	R_MarkLeaves ();	// done here so we know if we're in water

	if (!cl_entities[0].model || !cl.worldmodel)
		fatal ("R_RenderView: NULL worldmodel");
	ent_reject = nil;
	R_EdgeDrawing ();

	for(i = 0; i < cl_numvisedicts; i++){
		e = cl_visedicts[i];
		if(e->model->type != mod_brush && !R_DrawEntity(e)){
			e->last_reject = ent_reject;
			ent_reject = e;
		}
	}

	R_DrawViewModel ();
	R_DrawParticles ();
	r_drawflags = DRAW_BLEND;
	R_EdgeDrawing();
	// FIXME(sigrid): these need to be sorted and drawn back to front
	if(cl_numvisedicts > 0){
		for(i = cl_numvisedicts, e = ent_reject; e != nil && i > 0; e = e->last_reject)
			cl_visedicts[--i] = e;
		for(; i < cl_numvisedicts; i++)
			R_DrawEntity(cl_visedicts[i]);
	}
	r_drawflags = 0;

	R_DrawFog();

	if (r_dowarp)
		D_WarpScreen ();

	V_SetContentsColor (r_viewleaf->contents);

	if (r_reportsurfout.value && r_outofsurfaces)
		Con_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout.value && r_outofedges)
		Con_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);
}

/*
================
R_InitTurb
================
*/
void R_InitTurb (void)
{
	int i;

	for(i = 0; i < SIN_BUFFER_SIZE; i++){
		sintable[i] = AMP + sinf(i*M_PI*2/CYCLE)*AMP;
		intsintable[i] = AMP2 + sinf(i*M_PI*2/CYCLE)*AMP2;	// AMP2, not 20
	}
}

