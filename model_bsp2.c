#include "quakedef.h"

void BSP_SetParent(mnode_t *node, mnode_t *parent);
void BSP_MakeHull0(model_t *mod);
int BSP_CalcSurfaceExtents(model_t *mod, msurface_t *s);

int
BSP2_LoadEdges(model_t *mod, byte *in, int sz)
{
	medge_t *out;
	int i;
	static const int elsz = 2*4;

	if(sz % elsz){
		werrstr("BSP2_LoadEdges: funny lump size");
		return -1;
	}
	mod->numedges = sz / elsz;
	// FIXME(sigrid): why +1?
	mod->edges = out = Hunk_Alloc((mod->numedges+1) * sizeof(*out));

	for(i = 0; i < mod->numedges; i++, out++){
		out->v[0] = le32u(in);
		out->v[1] = le32u(in);
	}
	return 0;
}

int
BSP2_LoadLeafs(model_t *mod, byte *in, int sz)
{
	mleaf_t *out;
	int i, j, p;
	static const int elsz = 4+4+3*4+3*4+4+4+Namb;

	if(sz % elsz){
		werrstr("BSP2_LoadLeafs: funny lump size");
		return -1;
	}
	mod->numleafs = sz / elsz;
	mod->leafs = out = Hunk_Alloc(mod->numleafs * sizeof(*out));

	for(i = 0; i < mod->numleafs; i++, out++){
		out->contents = le32(in);
		out->compressed_vis = (p = le32(in)) < 0 ? nil : mod->visdata + p;

		for(j = 0; j < 3; j++)
			out->minmaxs[0+j] = f32(in);
		for(j = 0; j < 3; j++)
			out->minmaxs[3+j] = f32(in);

		out->firstmarksurface = mod->marksurfaces + le32u(in);
		out->nummarksurfaces = le32u(in);

		memmove(out->ambient_sound_level, in, Namb);
		in += Namb;
	}
	return 0;
}

int
BSP2_LoadFaces(model_t *mod, byte *in, int sz)
{
	msurface_t *out;
	int i, surfnum;
	static const int elsz = 4+4+4+4+4+MAXLIGHTMAPS+4;

	if(sz % elsz){
		werrstr("BSP2_LoadFaces: funny lump size");
		return -1;
	}
	mod->numsurfaces = sz / elsz;
	mod->surfaces = out = Hunk_Alloc(mod->numsurfaces * sizeof(*out));

	for(surfnum = 0; surfnum < mod->numsurfaces; surfnum++, out++){
		out->plane = mod->planes + le32u(in);
		out->flags = le32u(in) ? SURF_PLANEBACK : 0;
		out->firstedge = le32u(in);
		out->numedges = le32u(in);
		out->texinfo = mod->texinfo + le32u(in);

		if(BSP_CalcSurfaceExtents(mod, out) != 0)
			return -1;

		// lighting info

		memmove(out->styles, in, MAXLIGHTMAPS);
		in += MAXLIGHTMAPS;
		out->samples = (i = le32(in)) < 0 ? nil : mod->lightdata + i*3;

		// set the drawing flags flag

		if(strncmp(out->texinfo->texture->name, "sky", 3) == 0)
			out->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
		else if(out->texinfo->texture->name[0] == '*'){	// turbulent
			out->flags |= SURF_DRAWTURB | SURF_DRAWTILED | SURF_TRANS;
			for(i = 0; i < 2; i++){
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			if(strstr(out->texinfo->texture->name, "lava") != nil)
				out->flags |= SURF_LAVA;
			if(strstr(out->texinfo->texture->name, "slime") != nil)
				out->flags |= SURF_SLIME;
			if(strstr(out->texinfo->texture->name, "tele") != nil){
				out->flags |= SURF_TELE;
				out->flags &= ~SURF_TRANS;
			}
		}else if(out->texinfo->texture->name[0] == '{')
			out->flags |= SURF_TRANS | SURF_FENCE;
	}
	return 0;
}

int
BSP2_LoadNodes(model_t *mod, byte *in, int sz)
{
	int i, j, p;
	mnode_t *out;
	static const int elsz = 4+2*4+3*4+3*4+4+4;

	if(sz % elsz){
		werrstr("BSP_LoadNodes: funny lump size");
		return -1;
	}
	mod->numnodes = sz / elsz;
	mod->nodes = out = Hunk_Alloc(mod->numnodes * sizeof(*out));

	for(i = 0; i < mod->numnodes; i++, out++){
		out->plane = mod->planes + le32u(in);

		for(j = 0; j < 2; j++){
			p = le32(in);
			if(p >= 0 && p < mod->numnodes)
				out->children[j] = mod->nodes + p;
			else{
				p = -1-p;
				if(p >= 0 && p < mod->numleafs)
					out->children[j] = (mnode_t*)(mod->leafs + p);
				else{
					Con_DPrintf("BSP2_LoadNodes: invalid node child\n");
					out->children[j] = (mnode_t*)mod->leafs;
				}
			}
		}

		for(j = 0; j < 6; j++)
			out->minmaxs[j] = f32(in);

		out->firstsurface = le32u(in);
		out->numsurfaces = le32u(in);
	}

	BSP_SetParent(mod->nodes, nil); // sets nodes and leafs
	return 0;
}

int
BSP2_LoadClipnodes(model_t *mod, byte *in, int sz)
{
	mclipnode_t	*out;
	int i;
	hull_t *hull;
	static const int elsz = 4+2*4;

	if(sz % elsz){
		werrstr("BSP2_LoadClipnodes: funny lump size");
		return -1;
	}
	mod->numclipnodes = sz / elsz;
	mod->clipnodes = out = Hunk_Alloc(mod->numclipnodes * sizeof(*out));

	hull = &mod->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = mod->numclipnodes-1;
	hull->planes = mod->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &mod->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = mod->numclipnodes-1;
	hull->planes = mod->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for(i = 0; i < mod->numclipnodes; i++, out++){
		out->planenum = le32u(in);
		out->children[0] = le32u(in);
		out->children[1] = le32u(in);
		if(out->children[0] >= mod->numclipnodes)
			out->children[0] -= 0x10000;
		if(out->children[1] >= mod->numclipnodes)
			out->children[1] -= 0x10000;
	}
	return 0;
}

int
BSP2_LoadMarksurfaces(model_t *mod, byte *in, int sz)
{
	int i, j;
	msurface_t **out;

	if(sz % 4){
		werrstr("BSP2_LoadMarksurfaces: funny lump size");
		return -1;
	}
	mod->nummarksurfaces = sz / 4;
	mod->marksurfaces = out = Hunk_Alloc(mod->nummarksurfaces * sizeof(*out));

	for(i = 0; i < mod->nummarksurfaces; i++){
		j = le32u(in);
		if(j < 0 || j >= mod->numsurfaces){
			werrstr("BSP2_LoadMarksurfaces: bad surface number: %d", j);
			return -1;
		}
		out[i] = mod->surfaces + j;
	}
	return 0;
}
