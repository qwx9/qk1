#include "quakedef.h"

float dotadd(float *a, float *b);

void
BSP_SetParent(mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if(node->contents < 0)
		return;
	BSP_SetParent(node->children[0], node);
	BSP_SetParent(node->children[1], node);
}

int
BSP_CalcSurfaceExtents(model_t *mod, msurface_t *s)
{
	float mins[2], maxs[2], val;
	int i,j, e;
	mvertex_t *v;
	mtexinfo_t *tex;
	int bmins[2], bmaxs[2];

	mins[0] = mins[1] = Q_MAXFLOAT;
	maxs[0] = maxs[1] = -Q_MAXFLOAT;
	tex = s->texinfo;

	for(i = 0; i < s->numedges; i++){
		e = mod->surfedges[s->firstedge+i];
		if(e >= 0)
			v = &mod->vertexes[mod->edges[e].v[0]];
		else
			v = &mod->vertexes[mod->edges[-e].v[1]];

		for(j = 0; j < 2; j++){
			// this is... weird.
			// because everybody built maps long time ago, precision was
			// (most likely) 80 bits. we could just cast to double here,
			// but it's not 80 bits and stuff will still be broken.
			// instead we literally run 80-bit calculation emulated
			// using SoftFloat. enjoy. or not.
			val = dotadd(v->position, tex->vecs[j]);
			if(val < mins[j])
				mins[j] = val;
			if(val > maxs[j])
				maxs[j] = val;
		}
	}

	for(i = 0; i < 2; i++){
		bmins[i] = floor(mins[i]/16.0);
		bmaxs[i] = ceil(maxs[i]/16.0);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if((tex->flags & TEX_SPECIAL) == 0 && s->extents[i] > 2000){
			werrstr("BSP_CalcSurfaceExtents: bad surface: texture=%s flags=%ux extents[%d]=%d",
				tex->texture->name,
				tex->flags,
				i,
				s->extents[i]
			);
			return -1;
		}
	}

	return 0;
}

int
BSP_LoadTextures(model_t *mod, byte *in, int sz)
{
	int off, i, j, pixels, num, max, altmax, w, h;
	byte *p, *in0;
	texture_t *tx, *tx2;
	texture_t *anims[10];
	texture_t *altanims[10];
	static const int elsz = 16+2*4+4*4;

	if(sz < 1){
		mod->textures = nil;
		return 0;
	}
	if(sz < 4 || (sz % 4) != 0){
		werrstr("funny lump size");
		goto err;
	}

	in0 = in;
	mod->numtextures = le32(in);
	if(mod->numtextures*4 > sz-4){
		werrstr("overflow? %d > %d", mod->numtextures*4, sz-4);
		goto err;
	}
	mod->textures = Hunk_Alloc(mod->numtextures * sizeof(*mod->textures));

	for(i = 0; i < mod->numtextures; i++){
		off = le32(in);
		if(off == -1)
			continue;
		if(off < 0 || off > sz-elsz){
			werrstr("bad offset %d (sz %d)", off, sz);
			goto err;
		}
		p = in0+off+16;
		w = le32(p);
		h = le32(p);
		pixels = w*h*85/64;
		tx = Hunk_Alloc(sizeof(*tx) + pixels*sizeof(pixel_t));
		strncpy(tx->name, (char*)in0+off, sizeof(tx->name)-1);
		tx->name[sizeof(tx->name)-1] = 0;
		for(j = 0; j < MIPLEVELS; j++)
			tx->offsets[j] = sizeof(texture_t) + (le32(p) - (16+2*4+4*4))*sizeof(pixel_t);
		mod->textures[i] = tx;
		tx->width = w;
		tx->height = h;
		// the pixels immediately follow the structures
		torgbx(p, (pixel_t*)(tx+1), pixels);
		if(strncmp(tx->name, "sky", 3) == 0)
			R_InitSky(tx);
	}

	// sequence the animations
	for(i = 0; i < mod->numtextures; i++){
		tx = mod->textures[i];
		if(!tx || tx->name[0] != '+')
			continue;
		if(tx->anim_next)
			continue;	// already sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof(anims));
		memset(altanims, 0, sizeof(altanims));

		max = tx->name[1];
		if(max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if(max >= '0' && max <= '9'){
			max -= '0';
			altmax = 0;
			anims[max++] = tx;
		}else if(max >= 'A' && max <= 'J'){
			altmax = max - 'A';
			max = 0;
			altanims[altmax++] = tx;
		}else{
badanim:
			werrstr("bad animating texture: %s", tx->name);
			goto err;
		}

		for(j = i+1; j < mod->numtextures; j++){
			tx2 = mod->textures[j];
			if(!tx2 || tx2->name[0] != '+')
				continue;
			if(strcmp(tx2->name+2, tx->name+2) != 0)
				continue;

			num = tx2->name[1];
			if(num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if(num >= '0' && num <= '9'){
				num -= '0';
				anims[num] = tx2;
				if(num+1 > max)
					max = num + 1;
			}else if(num >= 'A' && num <= 'J'){
				num = num - 'A';
				altanims[num] = tx2;
				if(num+1 > altmax)
					altmax = num+1;
			}else{
				goto badanim;
			}
		}

#define	ANIM_CYCLE	2
		// link them all together
		for(j = 0; j < max; j++){
			tx2 = anims[j];
			if(!tx2){
badframe:
				werrstr("missing frame %d of %s", j, tx->name);
				goto err;
			}
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[(j+1) % max];
			if(altmax)
				tx2->alternate_anims = altanims[0];
		}
		for(j = 0; j < altmax; j++){
			tx2 = altanims[j];
			if(!tx2)
				goto badframe;
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[(j+1) % altmax];
			if(max)
				tx2->alternate_anims = anims[0];
		}
	}

	return 0;
err:
	werrstr("BSP_LoadTextures: %s", lerr());
	return -1;
}

int
BSP_LoadLighting(model_t *mod, byte *in, int sz)
{
	if(sz == 0)
		mod->lightdata = nil;
	else
		memcpy(mod->lightdata = Hunk_Alloc(sz), in, sz);
	return 0;
}

int
BSP_LoadVisibility(model_t *mod, byte *in, int sz)
{
	if(sz == 0)
		mod->visdata = nil;
	else
		memcpy(mod->visdata = Hunk_Alloc(sz), in, sz);
	return 0;
}

int
BSP_LoadEntities(model_t *mod, byte *in, int sz)
{
	if(sz == 0)
		mod->entities = nil;
	else
		memcpy(mod->entities = Hunk_Alloc(sz), in, sz);
	return 0;
}

int
BSP_LoadVertexes(model_t *mod, byte *in, int sz)
{
	mvertex_t *out;
	int i;
	static const int elsz = 3*4;

	if(sz % elsz){
		werrstr("BSP_LoadVertexes: funny lump size");
		return -1;
	}
	mod->numvertexes = sz / elsz;
	mod->vertexes = out = Hunk_Alloc(mod->numvertexes * sizeof(*out));
	for(i = 0; i < mod->numvertexes; i++, out++){
		out->position[0] = f32(in);
		out->position[1] = f32(in);
		out->position[2] = f32(in);
	}
	return 0;
}

int
BSP_LoadSubmodels(model_t *mod, byte *in, int sz)
{
	submodel_t *out;
	int i, j;
	static const int elsz = 3*4+3*4+3*4+MAX_MAP_HULLS*4+4+4+4;

	if(sz % elsz){
		werrstr("BSP_LoadSubmodels: funny lump size");
		return -1;
	}
	mod->numsubmodels = sz / elsz;
	mod->submodels = out = Hunk_Alloc(mod->numsubmodels * sizeof(*out));

	for(i = 0 ; i < mod->numsubmodels; i++, out++){
		for(j = 0; j < 3; j++)
			out->mins[j] = f32(in) - 1.0;
		for(j = 0; j < 3; j++)
			out->maxs[j] = f32(in) + 1.0;
		for(j = 0; j < 3; j++)
			out->origin[j] = f32(in);
		for(j = 0; j < MAX_MAP_HULLS; j++)
			out->headnode[j] = le32(in);
		out->visleafs = le32(in);
		out->firstface = le32(in);
		out->numfaces = le32(in);
	}
	return 0;
}

int
BSP_LoadEdges(model_t *mod, byte *in, int sz)
{
	medge_t *out;
	int i;
	static const int elsz = 2*2;

	if(sz % elsz){
		werrstr("BSP_LoadEdges: funny lump size");
		return -1;
	}
	mod->numedges = sz / elsz;
	mod->edges = out = Hunk_Alloc(mod->numedges * sizeof(*out));

	for(i = 0; i < mod->numedges; i++, out++){
		out->v[0] = le16u(in);
		out->v[1] = le16u(in);
	}
	return 0;
}

int
BSP_LoadTexinfo(model_t *mod, byte *in, int sz)
{
	mtexinfo_t *out;
	int i, j, k, miptex;
	float len1, len2;
	static const int elsz = 2*4*4+4+4;

	if(sz % elsz){
		werrstr("BSP_LoadTexinfo: funny lump size");
		return -1;
	}
	mod->numtexinfo = sz / elsz;
	mod->texinfo = out = Hunk_Alloc(mod->numtexinfo * sizeof(*out));

	for(i = 0; i < mod->numtexinfo; i++, out++){
		for(j = 0; j < 2; j++){
			for(k = 0; k < 4; k++)
				out->vecs[j][k] = f32(in);
		}
		len1 = Length(out->vecs[0]);
		len2 = Length(out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = le32(in);
		out->flags = le32(in);
		out->texture = nil;
		if(mod->textures != nil){
			if(miptex >= mod->numtextures){
				werrstr("BSP_LoadTexinfo: miptex >= mod->numtextures");
				return -1;
			}
			out->texture = mod->textures[miptex];
		}
		if(out->texture == nil){
			out->texture = r_notexture_mip; // texture not found
			out->flags = 0;
		}
	}
	return 0;
}

int
BSP_LoadFaces(model_t *mod, byte *in, int sz)
{
	msurface_t *out;
	int i, surfnum;
	static const int elsz = 2+2+4+2+2+MAXLIGHTMAPS+4;

	if(sz % elsz){
		werrstr("BSP_LoadFaces: funny lump size");
		return -1;
	}
	mod->numsurfaces = sz / elsz;
	mod->surfaces = out = Hunk_Alloc(mod->numsurfaces * sizeof(*out));

	for(surfnum = 0; surfnum < mod->numsurfaces; surfnum++, out++){
		out->plane = mod->planes + le16u(in);
		out->flags = le16u(in) ? SURF_PLANEBACK : 0;
		out->firstedge = le32u(in);
		out->numedges = le16u(in);
		out->texinfo = mod->texinfo + le16u(in);

		if(BSP_CalcSurfaceExtents(mod, out) != 0)
			return -1;

		// lighting info

		memmove(out->styles, in, MAXLIGHTMAPS);
		in += MAXLIGHTMAPS;
		i = le32(in);
		out->samples = i < 0 ? nil : mod->lightdata + i;

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
BSP_LoadNodes(model_t *mod, byte *in, int sz)
{
	int i, j, p;
	mnode_t *out;
	static const int elsz = 4+2*2+3*2+3*2+2+2;

	if(sz % elsz){
		werrstr("BSP_LoadNodes: funny lump size");
		return -1;
	}
	mod->numnodes = sz / elsz;
	mod->nodes = out = Hunk_Alloc(mod->numnodes * sizeof(*out));

	for(i = 0; i < mod->numnodes; i++, out++){
		out->plane = mod->planes + le32u(in);

		for(j = 0; j < 2; j++){
			p = le16u(in);
			if(p >= 0 && p < mod->numnodes)
				out->children[j] = mod->nodes + p;
			else{
				p = 0xffff-p;
				assert(mod->leafs != nil);
				if(p >= 0 && p < mod->numleafs)
					out->children[j] = (mnode_t*)(mod->leafs + p);
				else{
					Con_DPrintf("BSP_LoadNodes: invalid node child\n");
					out->children[j] = (mnode_t*)mod->leafs;
				}
			}
		}

		for(j = 0; j < 6; j++)
			out->minmaxs[j] = le16(in);

		out->firstsurface = le16u(in);
		out->numsurfaces = le16u(in);
	}

	BSP_SetParent(mod->nodes, nil); // sets nodes and leafs
	return 0;
}

int
BSP_LoadLeafs(model_t *mod, byte *in, int sz)
{
	mleaf_t *out;
	int i, j, p;
	static const int elsz = 4+4+3*2+3*2+2+2+Namb;

	if(sz % elsz){
		werrstr("BSP_LoadLeafs: funny lump size");
		return -1;
	}
	mod->numleafs = sz / elsz;
	mod->leafs = out = Hunk_Alloc(mod->numleafs * sizeof(*out));

	for(i = 0; i < mod->numleafs; i++, out++){
		out->contents = le32(in);
		out->compressed_vis = (p = le32(in)) < 0 ? nil : mod->visdata + p;

		for(j = 0; j < 3; j++)
			out->minmaxs[0+j] = le16(in);
		for(j = 0; j < 3; j++)
			out->minmaxs[3+j] = le16(in);

		out->firstmarksurface = mod->marksurfaces + le16u(in);
		out->nummarksurfaces = le16u(in);

		memmove(out->ambient_sound_level, in, Namb);
		in += Namb;
	}
	return 0;
}

int
BSP_LoadClipnodes(model_t *mod, byte *in, int sz)
{
	mclipnode_t *out;
	int i;
	hull_t *hull;
	static const int elsz = 4+2*2;

	if(sz % elsz){
		werrstr("BSP_LoadClipnodes: funny lump size");
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
		out->children[0] = le16u(in);
		out->children[1] = le16u(in);
		if(out->children[0] >= mod->numclipnodes)
			out->children[0] -= 0x10000;
		if(out->children[1] >= mod->numclipnodes)
			out->children[1] -= 0x10000;
	}
	return 0;
}

void
BSP_MakeHull0(model_t *mod)
{
	mnode_t *in, *child;
	mclipnode_t *out;
	int i, j;
	hull_t *hull;

	hull = &mod->hulls[0];

	in = mod->nodes;
	out = Hunk_Alloc(mod->numnodes * sizeof(*out));

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = mod->numnodes-1;
	hull->planes = mod->planes;

	for(i = 0; i < mod->numnodes; i++, out++, in++){
		out->planenum = in->plane - mod->planes;
		for(j = 0; j < 2; j++){
			child = in->children[j];
			if(child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - mod->nodes;
		}
	}
}

int
BSP_LoadMarksurfaces(model_t *mod, byte *in, int sz)
{
	int i, j;
	msurface_t **out;
	static const int elsz = 2;

	if(sz % elsz){
		werrstr("BSP_LoadMarksurfaces: funny lump size");
		return -1;
	}
	mod->nummarksurfaces = sz / elsz;
	mod->marksurfaces = out = Hunk_Alloc(mod->nummarksurfaces * sizeof(*out));

	for(i = 0; i < mod->nummarksurfaces; i++){
		j = le16u(in);
		if(j < 0 || j >= mod->numsurfaces){
			werrstr("BSP_LoadMarksurfaces: bad surface number: %d", j);
			return -1;
		}
		out[i] = mod->surfaces + j;
	}
	return 0;
}

int
BSP_LoadSurfedges(model_t *mod, byte *in, int sz)
{
	int i, *out;
	static const int elsz = 4;

	if(sz % elsz){
		werrstr("BSP_LoadSurfedges: funny lump size");
		return -1;
	}
	mod->numsurfedges = sz / elsz;
	mod->surfedges = out = Hunk_Alloc(mod->numsurfedges * sizeof(*out));

	for(i = 0; i < mod->numsurfedges; i++)
		out[i] = le32(in);
	return 0;
}

int
BSP_LoadPlanes(model_t *mod, byte *in, int sz)
{
	int i, j, bits;
	mplane_t *out;
	static const int elsz = 3*4+4+4;

	if(sz % elsz){
		werrstr("BSP_LoadPlanes: funny lump size");
		return -1;
	}
	mod->numplanes = sz / elsz;
	mod->planes = out = Hunk_Alloc(mod->numplanes * sizeof(*out));

	for(i = 0; i < mod->numplanes; i++, out++){
		bits = 0;
		for(j = 0; j < 3; j++){
			if((out->normal[j] = f32(in)) < 0)
				bits |= 1<<j;
		}

		out->dist = f32(in);
		out->type = le32(in);
		out->signbits = bits;
	}
	return 0;
}
