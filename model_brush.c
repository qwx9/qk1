#include "quakedef.h"
#include "model_bsp.h"

static float
RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;
	float fmin, fmax;

	for(i = 0; i < 3; i++) {
		fmin = fabs(mins[i]);
		fmax = fabs(maxs[i]);
		corner[i] = max(fmin, fmax);
	}

	return Length(corner);
}

static void
Mod_Origin(model_t *mod, submodel_t *sub, int id, char *e)
{
	enum {
		Sinit,
		Sinside,
		Satmodel,
		Smodelfound,
		Satorigin,
	};

	int state = Sinit, i;
	char *s;

	for(;;) {
		if((e = COM_Parse(e)) == nil)
			break;
		if(com_token[0] == '{') {
			if(state == Sinside)
				break;
			state = Sinside;
			continue;
		}
		if(com_token[0] == '}') {
			if(state != Sinside && state != Smodelfound)
				break;
			state = Sinit;
			continue;
		}
		if(state == Sinside && strcmp(com_token, "model") == 0)
			state = Satmodel;
		else if(state == Smodelfound && strcmp(com_token, "origin") == 0)
			state = Satorigin;
		if((e = COM_Parse(e)) == nil)
			break;
		if(state == Satmodel) {
			if(com_token[0] == '*' && atoi(com_token + 1) == id)
				state = Smodelfound;
			else
				state = Sinside;
		} else if(state == Satorigin) {
			mod->flags |= MOD_ORIGIN_SET;
			for(i = 0, s = com_token; i < 3; i++)
				sub->origin[i] = strtod(s, &s);
			break;
		}
	}
}

void
Mod_LoadBrushModel(model_t *mod, byte *in0, int total)
{
	int i, j, off, sz;
	model_t *submod;
	byte *in;
	submodel_t *bm;
	char name[16], *e;
	int (*loadf[HEADER_LUMPS])(model_t *, byte *, int, int) = {
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
	static const int extra[HEADER_LUMPS] = {
		[LUMP_VERTEXES] = 8,
		[LUMP_EDGES] = 12,
		[LUMP_SURFEDGES] = 24,
		[LUMP_FACES] = 6,
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
	if(mod->ver == BSPVERSION) {
		// all set
	} else if(mod->ver == BSP30VERSION) {
		loadf[LUMP_ENTITIES] = BSP30_LoadEntities;
		loadf[LUMP_FACES] = BSP30_LoadFaces;
		loadf[LUMP_LIGHTING] = BSP30_LoadLighting;
		loadf[LUMP_TEXTURES] = BSP30_LoadTextures;
	} else if(mod->ver == BSP2VERSION) {
		loadf[LUMP_EDGES] = BSP2_LoadEdges;
		loadf[LUMP_FACES] = BSP2_LoadFaces;
		loadf[LUMP_MARKSURFACES] = BSP2_LoadMarksurfaces;
		loadf[LUMP_LEAFS] = BSP2_LoadLeafs;
		loadf[LUMP_NODES] = BSP2_LoadNodes;
		loadf[LUMP_CLIPNODES] = BSP2_LoadClipnodes;
	} else {
		werrstr("unsupported version: %d", mod->ver);
		goto err;
	}

	if(total < 4 + nelem(loadf) * 2 * 4) {
		werrstr("truncated: total=%d", total);
		goto err;
	}

	for(i = 0; i < nelem(loadf); i++) {
		in = in0 + 4 + 2 * 4 * order[i];
		off = le32(in);
		sz = le32(in);
		if(off < 0 || sz < 0 || off + sz > total) {
			werrstr("invalid lump off=%d sz=%d total=%d", off, sz, total);
			goto err;
		}
		if(loadf[order[i]](mod, in0 + off, sz, extra[order[i]]) != 0)
			goto err;
	}

	BSP_MakeHull0(mod);

	mod->numframes = 2; // regular and alternate animation
	e = mod->entities;

	// set up the submodels (FIXME: this is confusing)
	for(i = 0; i < mod->numsubmodels; i++) {
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for(j = 1; j < MAX_MAP_HULLS; j++) {
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		mod->blend = false;
		mod->flags = MOD_NODRAW;
		for(j = bm->firstface; j < bm->firstface + bm->numfaces; j++) {
			if(mod->surfaces[j].flags & SURF_NODRAW)
				continue;
			mod->flags &= ~MOD_NODRAW;
			if(mod->surfaces[j].flags & SURF_TRANS) {
				mod->blend = true;
				break;
			}
		}

		VectorCopy(bm->maxs, mod->maxs);
		VectorCopy(bm->mins, mod->mins);
		mod->radius = RadiusFromBounds(mod->mins, mod->maxs);
		mod->numleafs = bm->visleafs;

		if(i > 0)
			Mod_Origin(mod, bm, i, e);
		if(i < mod->numsubmodels - 1) {
			snprint(name, sizeof(name), "*%d", i + 1);
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

const char *
Mod_FindInWorldspawn(model_t *mod, const char *key)
{
	enum {
		Sinit,
		Sinside,
		Sfound,
	};

	int state = Sinit;
	char *e;

	for(e = mod->entities;;) {
		if((e = COM_Parse(e)) == nil)
			break;
		if(com_token[0] == '{') {
			if(state == Sinside)
				break;
			state = Sinside;
			continue;
		}
		if(com_token[0] == '}')
			break;
		if(state == Sinside && strcmp(com_token, key) == 0)
			state = Sfound;
		if((e = COM_Parse(e)) == nil)
			break;
		if(state == Sfound)
			return com_token;
	}
	return nil;
}
