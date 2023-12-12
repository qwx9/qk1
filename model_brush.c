#include "quakedef.h"

void BSP_MakeHull0(model_t *mod);
model_t *Mod_FindName (char *name);

int BSP_LoadClipnodes(model_t *mod, byte *in, int sz);
int BSP_LoadEdges(model_t *mod, byte *in, int sz);
int BSP_LoadEntities(model_t *mod, byte *in, int sz);
int BSP_LoadFaces(model_t *mod, byte *in, int sz);
int BSP_LoadLeafs(model_t *mod, byte *in, int sz);
int BSP_LoadLighting(model_t *mod, byte *in, int sz);
int BSP_LoadMarksurfaces(model_t *mod, byte *in, int sz);
int BSP_LoadNodes(model_t *mod, byte *in, int sz);
int BSP_LoadPlanes(model_t *mod, byte *in, int sz);
int BSP_LoadSubmodels(model_t *mod, byte *in, int sz);
int BSP_LoadSurfedges(model_t *mod, byte *in, int sz);
int BSP_LoadTexinfo(model_t *mod, byte *in, int sz);
int BSP_LoadTextures(model_t *mod, byte *in, int sz);
int BSP_LoadVertexes(model_t *mod, byte *in, int sz);
int BSP_LoadVisibility(model_t *mod, byte *in, int sz);

int BSP2_LoadClipnodes(model_t *mod, byte *in, int sz);
int BSP2_LoadEdges(model_t *mod, byte *in, int sz);
int BSP2_LoadFaces(model_t *mod, byte *in, int sz);
int BSP2_LoadLeafs(model_t *mod, byte *in, int sz);
int BSP2_LoadMarksurfaces(model_t *mod, byte *in, int sz);
int BSP2_LoadNodes(model_t *mod, byte *in, int sz);

int BSP30_LoadEntities(model_t *mod, byte *in, int sz);
int BSP30_LoadLighting(model_t *mod, byte *in, int sz);
int BSP30_LoadFaces(model_t *mod, byte *in, int sz);
int BSP30_LoadTextures(model_t *mod, byte *in, int sz);

static float
RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;
	float fmin, fmax;

	for(i=0 ; i<3 ; i++){
		fmin = fabs(mins[i]);
		fmax = fabs(maxs[i]);
		corner[i] = max(fmin, fmax);
	}

	return Length(corner);
}

void
Mod_LoadBrushModel(model_t *mod, byte *in0, int total)
{
	int i, j, off, sz;
	model_t *submod;
	byte *in;
	submodel_t *bm;
	char name[16];
	int (*loadf[HEADER_LUMPS])(model_t *, byte *, int) = {
		[LUMP_ENTITIES] = BSP_LoadEntities,
		[LUMP_VERTEXES] = BSP_LoadVertexes,
		[LUMP_EDGES] = BSP_LoadEdges,
		[LUMP_SURFEDGES] = BSP_LoadSurfedges,
		[LUMP_TEXTURES] = BSP_LoadTextures,
		[LUMP_LIGHTING] = BSP_LoadLighting,
		[LUMP_PLANES] = BSP_LoadPlanes,
		[LUMP_TEXINFO] = BSP_LoadTexinfo,
		[LUMP_FACES] = BSP_LoadFaces,
		[LUMP_MARKSURFACES] = BSP_LoadMarksurfaces,
		[LUMP_VISIBILITY] = BSP_LoadVisibility,
		[LUMP_LEAFS] = BSP_LoadLeafs,
		[LUMP_NODES] = BSP_LoadNodes,
		[LUMP_CLIPNODES] = BSP_LoadClipnodes,
		[LUMP_MODELS] = BSP_LoadSubmodels,
	};
	static const int order[HEADER_LUMPS] = {
		LUMP_ENTITIES,
		LUMP_VERTEXES,
		LUMP_EDGES,
		LUMP_SURFEDGES,
		LUMP_TEXTURES,
		LUMP_LIGHTING,
		LUMP_PLANES,
		LUMP_TEXINFO,
		LUMP_FACES,
		LUMP_MARKSURFACES,
		LUMP_VISIBILITY,
		LUMP_LEAFS,
		LUMP_NODES,
		LUMP_CLIPNODES,
		LUMP_MODELS,
	};

	in = in0;
	mod->type = mod_brush;
	mod->ver = le32(in);
	mod->leafs = nil;
	if(mod->ver == BSPVERSION){
		// all set
	}else if(mod->ver == BSP30VERSION){
		loadf[LUMP_ENTITIES] = BSP30_LoadEntities,
		loadf[LUMP_FACES] = BSP30_LoadFaces;
		loadf[LUMP_LIGHTING] = BSP30_LoadLighting;
		loadf[LUMP_TEXTURES] = BSP30_LoadTextures;
	}else if(mod->ver == BSP2VERSION){
		loadf[LUMP_EDGES] = BSP2_LoadEdges;
		loadf[LUMP_FACES] = BSP2_LoadFaces;
		loadf[LUMP_MARKSURFACES] = BSP2_LoadMarksurfaces;
		loadf[LUMP_LEAFS] = BSP2_LoadLeafs;
		loadf[LUMP_NODES] = BSP2_LoadNodes;
		loadf[LUMP_CLIPNODES] = BSP2_LoadClipnodes;
	}else{
		werrstr("unsupported version: %d", mod->ver);
		goto err;
	}

	if(total < 4+nelem(loadf)*2*4){
		werrstr("truncated: total=%d", total);
		goto err;
	}

	for(i = 0; i < nelem(loadf); i++){
		in = in0+4+2*4*order[i];
		off = le32(in);
		sz = le32(in);
		if(off < 0 || sz < 0 || off+sz > total){
			werrstr("invalid lump off=%d sz=%d total=%d", off, sz, total);
			goto err;
		}
		if(loadf[order[i]](mod, in0+off, sz) != 0)
			goto err;
	}

	BSP_MakeHull0(mod);

	mod->numframes = 2;		// regular and alternate animation
	mod->flags = 0;

	// set up the submodels (FIXME: this is confusing)
	for(i = 0; i < mod->numsubmodels; i++){
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for(j = 1; j < MAX_MAP_HULLS; j++){
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		mod->blend = false;
		for(j = bm->firstface; j < bm->firstface+bm->numfaces; j++){
			if(mod->surfaces[j].flags & SURF_TRANS){
				mod->blend = true;
				break;
			}
		}

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);
		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);
		mod->numleafs = bm->visleafs;

		if(i < mod->numsubmodels-1){
			snprint(name, sizeof(name), "*%d", i+1);
			submod = Mod_FindName(name);
			*submod = *mod;
			strcpy(submod->name, name);
			mod = submod;
		}
	}

	return;
err:
	Host_Error("Mod_LoadBrushModel: %s: %s", mod->name, lerr());
}
