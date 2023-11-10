#include "quakedef.h"

model_t	*loadmodel;
char	loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, void *buffer);
void Mod_LoadBrushModel (model_t *mod, void *buffer);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, bool crash);

#define	MAX_MOD_KNOWN	4096
model_t	*mod_known;
int		mod_numknown;

// values for model_t's needload
#define NL_PRESENT		0
#define NL_NEEDS_LOADED	1
#define NL_UNREFERENCED	2

void
Mod_Init(void)
{
	mod_known = Hunk_Alloc(MAX_MOD_KNOWN * sizeof(*mod_known));
}

/*
===============
Mod_Extradata

Caches the data if needed
===============
*/
void *Mod_Extradata (model_t *mod)
{
	void	*r;

	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);

	if (!mod->cache.data)
		Host_Error("Mod_Extradata: caching failed: %s", mod->name);
	return mod->cache.data;
}

/*
===============
Mod_PointInLeaf
===============
*/
mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;

	if(!model || !model->nodes)
		Host_Error("Mod_PointInLeaf: bad model");

	node = model->nodes;
	while(1){
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct(p, plane->normal) - plane->dist;
		node = node->children[d <= 0];
	}
}


/*
===================
Mod_DecompressVis
===================
*/
static byte *Mod_DecompressVis (byte *in, model_t *model, int *outsz)
{
	static byte	*decompressed;
	static int decompressed_size;
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)/8;
	if(decompressed == nil || row > decompressed_size){
		decompressed_size = row;
		decompressed = realloc(decompressed, decompressed_size);
	}
	out = decompressed;
	*outsz = row;

	if(!in){ // no vis info, so make all visible
		memset(out, 0xff, row);
		return decompressed;
	}

	do{
		if (*in){
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while(c){
			*out++ = 0;
			c--;
		}
	}while(out - decompressed < row);

	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model, int *outsz)
{
	static byte *mod_novis;
	static int mod_novis_size;
	int sz;

	sz = ((model->numleafs+7)/8 + 3) & ~3;
	*outsz = sz;
	if (leaf == model->leafs) {
		if(mod_novis == nil || mod_novis_size < sz){
			mod_novis = realloc(mod_novis, sz);
			mod_novis_size = sz;
		}
		memset(mod_novis, 0xff, mod_novis_size);
		return mod_novis;
	}
	return Mod_DecompressVis (leaf->compressed_vis, model, outsz);
}

/*
===================
Mod_ClearAll
===================
*/
void Mod_ClearAll (void)
{
	int		i;
	model_t	*mod;


	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++) {
		mod->needload = NL_UNREFERENCED;
		//FIX FOR CACHE_ALLOC ERRORS:
		if (mod->type == mod_sprite)
			mod->cache.data = nil;
	}
}

/*
==================
Mod_FindName

==================
*/
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	model_t	*avail = nil;

	if (!name[0])
		Host_Error("Mod_FindName: nil name");

	// search the currently loaded models
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if(strcmp(mod->name, name) == 0)
			break;
		if(mod->needload == NL_UNREFERENCED)
			if (!avail || mod->type != mod_alias)
				avail = mod;
	}

	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
		{
			if (avail)
			{
				mod = avail;
				if (mod->type == mod_alias)
					if (Cache_Check (&mod->cache))
						Cache_Free (&mod->cache);
			}
			else
				Host_Error("mod_numknown == MAX_MOD_KNOWN");
		}
		else
			mod_numknown++;
		strcpy (mod->name, name);
		mod->needload = NL_NEEDS_LOADED;
	}

	return mod;
}

/*
==================
Mod_TouchModel

==================
*/
void Mod_TouchModel (char *name)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	if (mod->needload == NL_PRESENT)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}

/*
==================
Mod_LoadModel

Loads a model into the cache
==================
*/
model_t *Mod_LoadModel (model_t *mod, bool crash)
{
	unsigned *buf;
	byte stackbuf[1024];		// avoid dirtying the cache heap

	if (mod->type == mod_alias)
	{
		if (Cache_Check (&mod->cache))
		{
			mod->needload = NL_PRESENT;
			return mod;
		}
	}
	else
	{
		if (mod->needload == NL_PRESENT){
			return mod;
		}
	}

	// because the world is so huge, load it one piece at a time
	buf = loadstklmp(mod->name, stackbuf, sizeof stackbuf, nil);
	if(buf == nil){
		if(crash)
			Host_Error("Mod_LoadModel: %s", lerr());
		return nil;
	}

	// allocate a new model
	radix(mod->name, loadname);
	loadmodel = mod;

	// fill it in

	// call the apropriate loader
	mod->needload = NL_PRESENT;

	switch (LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;

	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}

	return mod;
}

/*
==================
Mod_ForName

Loads in a model for the given name
==================
*/
model_t *Mod_ForName (char *name, bool crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}


/*
===============================================================================

					BRUSHMODEL LOADING

===============================================================================
*/

byte	*mod_base;


/*
=================
Mod_LoadTextures
=================
*/
void Mod_LoadTextures (lump_t *l)
{
	int		i, j, pixels, num, max, altmax;
	miptex_t	*mt;
	texture_t	*tx, *tx2;
	texture_t	*anims[10];
	texture_t	*altanims[10];
	dmiptexlump_t *m;

	if (!l->filelen)
	{
		loadmodel->textures = nil;
		return;
	}
	m = (dmiptexlump_t *)(mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);

	loadmodel->numtextures = m->nummiptex;
	loadmodel->textures = Hunk_Alloc(m->nummiptex * sizeof *loadmodel->textures);

	for (i=0 ; i<m->nummiptex ; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;
		mt = (miptex_t *)((byte *)m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);
		for (j=0 ; j<MIPLEVELS ; j++)
			mt->offsets[j] = LittleLong (mt->offsets[j]);

		if ( (mt->width & 15) || (mt->height & 15) )
			Con_DPrintf("Texture %s is not 16 aligned", mt->name);
		pixels = mt->width*mt->height/64*85;
		tx = Hunk_Alloc(pixels + sizeof *tx);
		loadmodel->textures[i] = tx;

		memcpy(tx->name, mt->name, sizeof tx->name);
		tx->width = mt->width;
		tx->height = mt->height;
		for (j=0 ; j<MIPLEVELS ; j++)
			tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
		// the pixels immediately follow the structures
		memcpy(tx+1, mt+1, pixels);

		if(strncmp(mt->name, "sky", 3) == 0)
			R_InitSky (tx);
	}

	// sequence the animations
	for (i=0 ; i<m->nummiptex ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

		// find the number of frames in the animation
		memset(anims, 0, sizeof anims);
		memset(altanims, 0, sizeof altanims);

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Host_Error("Bad animating texture %s", tx->name);

		for (j=i+1 ; j<m->nummiptex ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if(strcmp(tx2->name+2, tx->name+2) != 0)
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Host_Error("Bad animating texture %s", tx->name);
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Host_Error("Missing frame %d of %s", j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Host_Error("Missing frame %d of %s", j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

/*
=================
Mod_LoadLighting
=================
*/
void Mod_LoadLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = nil;
		return;
	}
	loadmodel->lightdata = Hunk_Alloc(l->filelen);
	memcpy(loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVisibility
=================
*/
void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = nil;
		return;
	}
	loadmodel->visdata = Hunk_Alloc(l->filelen);
	memcpy(loadmodel->visdata, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadEntities
=================
*/
void Mod_LoadEntities (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->entities = nil;
		return;
	}
	loadmodel->entities = Hunk_Alloc(l->filelen);
	memcpy(loadmodel->entities, mod_base + l->fileofs, l->filelen);
}


/*
=================
Mod_LoadVertexes
=================
*/
void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadVertexes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

/*
=================
Mod_LoadSubmodels
=================
*/
void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadSubmodels: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

/*
=================
Mod_LoadEdges
=================
*/
void Mod_LoadEdges (lump_t *l, int ver)
{
	byte *in;
	medge_t *out;
	int 	i, count, sz;

	in = (void *)(mod_base + l->fileofs);
	sz = ver == BSP2VERSION ? sizeof(bsp2_dedge_t) : sizeof(dedge_t);
	if (l->filelen % sz)
		Host_Error("Mod_LoadEdges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sz;
	out = Hunk_Alloc((count+1) * sizeof *out);

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, out++)
	{
		out->v[0] = ver == BSP2VERSION ? le32u(in) : le16u(in);
		out->v[1] = ver == BSP2VERSION ? le32u(in) : le16u(in);
	}
}

/*
=================
Mod_LoadTexinfo
=================
*/
void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadTexInfo: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<4 ; j++)
			out->vecs[0][j] = LittleFloat (in->vecs[0][j]);
		for (j=0 ; j<4 ; j++)
			out->vecs[1][j] = LittleFloat (in->vecs[1][j]);
		len1 = Length (out->vecs[0]);
		len2 = Length (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;
		/*
		if (len1 + len2 < 0.001)
			out->mipadjust = 1;		// don't crash
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 + 0.1 );
		*/

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Host_Error("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

/*
================
CalcSurfaceExtents

Fills in s->texturemins[] and s->extents[]
================
*/
void CalcSurfaceExtents (msurface_t *s)
{
	double	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = Q_MAXFLOAT;
	maxs[0] = maxs[1] = -Q_MAXFLOAT;
	tex = s->texinfo;

	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];

		for (j=0 ; j<2 ; j++)
		{
			val = (double)v->position[0] * (double)tex->vecs[j][0] +
				(double)v->position[1] * (double)tex->vecs[j][1] +
				(double)v->position[2] * (double)tex->vecs[j][2] +
				(double)tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{
		bmins[i] = floor(mins[i]/16.0);
		bmaxs[i] = ceil(maxs[i]/16.0);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 2000)
			Host_Error("Bad surface: texture=%s flags=%ux extents[%d]=%d",
				tex->texture->name,
				tex->flags,
				i,
				s->extents[i]
			);
	}
}


/*
=================
Mod_LoadFaces
=================
*/
void Mod_LoadFaces (lump_t *l, int ver)
{
	byte		*in;
	msurface_t 	*out;
	int			i, count, surfnum, sz;

	in = (void *)(mod_base + l->fileofs);
	sz = ver == BSP2VERSION ? sizeof(bsp2_dface_t) : sizeof(dface_t);
	if (l->filelen % sz)
		Host_Error("Mod_LoadFaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sz;
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, out++)
	{
		out->plane = loadmodel->planes + (ver == BSP2VERSION ? le32u(in) : le16u(in));
		out->flags = (ver == BSP2VERSION ? le32u(in) : le16u(in)) ? SURF_PLANEBACK : 0;
		out->firstedge = le32u(in);
		out->numedges = ver == BSP2VERSION ? le32u(in) : le16u(in);
		out->texinfo = loadmodel->texinfo + (ver == BSP2VERSION ? le32u(in) : le16u(in));

		CalcSurfaceExtents (out);

		// lighting info

		memmove(out->styles, in, MAXLIGHTMAPS);
		in += MAXLIGHTMAPS;
		out->samples = (i = le32(in)) < 0 ? nil : loadmodel->lightdata + i;

		// set the drawing flags flag

		if(strncmp(out->texinfo->texture->name, "sky", 3) == 0)
			out->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
		else if(out->texinfo->texture->name[0] == '*'){	// turbulent
			out->flags |= SURF_DRAWTURB | SURF_DRAWTILED | SURF_TRANS;
			for (i=0 ; i<2 ; i++){
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
}


/*
=================
Mod_SetParent
=================
*/
void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

/*
=================
Mod_LoadNodes
=================
*/
void Mod_LoadNodes (lump_t *l, int ver)
{
	int			i, j, count, p, sz;
	byte		*in;
	mnode_t 	*out;

	in = (void *)(mod_base + l->fileofs);
	sz = ver == BSP2VERSION ? sizeof(bsp2_dnode_t) : sizeof(dnode_t);
	if (l->filelen % sz)
		Host_Error("Mod_LoadNodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sz;
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, out++)
	{
		out->plane = loadmodel->planes + le32u(in);

		for (j=0 ; j<2 ; j++){
			p = ver == BSP2VERSION ? le32(in) : le16u(in);
			if(p >= 0 && p < count)
				out->children[j] = loadmodel->nodes + p;
			else{
				p = ver == BSP2VERSION ? -1-p : 0xffff-p;
				if(p >= 0 && p < loadmodel->numleafs)
					out->children[j] = (mnode_t *)(loadmodel->leafs + p);
				else{
					Con_Printf("Mod_LoadNodes: invalid node child\n");
					out->children[j] = (mnode_t *)loadmodel->leafs;
				}
			}
		}

		for(j=0 ; j<3 ; j++)
			out->minmaxs[0+j] = ver == BSP2VERSION ? f32(in) : le16(in);
		for(j=0 ; j<3 ; j++)
			out->minmaxs[3+j] = ver == BSP2VERSION ? f32(in) : le16(in);

		out->firstsurface = ver == BSP2VERSION ? le32u(in) : le16u(in);
		out->numsurfaces = ver == BSP2VERSION ? le32u(in) : le16u(in);
	}

	Mod_SetParent (loadmodel->nodes, nil);	// sets nodes and leafs
}

/*
=================
Mod_LoadLeafs
=================
*/
void Mod_LoadLeafs (lump_t *l, int ver)
{
	byte	 	*in;
	mleaf_t 	*out;
	int			i, j, count, p, sz;

	in = (void *)(mod_base + l->fileofs);
	sz = ver == BSP2VERSION ? sizeof(bsp2_dleaf_t) : sizeof(dleaf_t);
	if (l->filelen % sz)
		Host_Error("Mod_LoadLeafs: funny lump size in %s", loadmodel->name);
	count = l->filelen / sz;
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, out++)
	{
		out->contents = le32(in);
		out->compressed_vis = (p = le32(in)) < 0 ? nil : loadmodel->visdata + p;

		for(j=0 ; j<3 ; j++)
			out->minmaxs[0+j] = ver == BSP2VERSION ? f32(in) : le16(in);
		for(j=0 ; j<3 ; j++)
			out->minmaxs[3+j] = ver == BSP2VERSION ? f32(in) : le16(in);

		out->firstmarksurface = loadmodel->marksurfaces + (ver == BSP2VERSION ? le32u(in) : le16u(in));
		out->nummarksurfaces = ver == BSP2VERSION ? le32u(in) : le16u(in);

		memmove(out->ambient_sound_level, in, 4);
		in += 4;
	}
}

/*
=================
Mod_LoadClipnodes
=================
*/
void Mod_LoadClipnodes (lump_t *l, int ver)
{
	byte *in;
	mclipnode_t	*out;
	int			i, count, sz;
	hull_t		*hull;

	in = (void *)(mod_base + l->fileofs);
	sz = ver == BSP2VERSION ? sizeof(bsp2_dclipnode_t) : sizeof(dclipnode_t);
	if (l->filelen % sz)
		Host_Error("Mod_LoadClipnodes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sz;
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	hull = &loadmodel->hulls[1];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -16;
	hull->clip_mins[1] = -16;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 16;
	hull->clip_maxs[1] = 16;
	hull->clip_maxs[2] = 32;

	hull = &loadmodel->hulls[2];
	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;
	hull->clip_mins[0] = -32;
	hull->clip_mins[1] = -32;
	hull->clip_mins[2] = -24;
	hull->clip_maxs[0] = 32;
	hull->clip_maxs[1] = 32;
	hull->clip_maxs[2] = 64;

	for (i=0 ; i<count ; i++, out++)
	{
		out->planenum = le32u(in);
		out->children[0] = ver == BSP2VERSION ? le32u(in) : le16u(in);
		out->children[1] = ver == BSP2VERSION ? le32u(in) : le16u(in);
		if(out->children[0] >= count)
			out->children[0] -= 0x10000;
		if(out->children[1] >= count)
			out->children[1] -= 0x10000;
	}
}

/*
=================
Mod_MakeHull0

Deplicate the drawing hull structure as a clipping hull
=================
*/
void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	mclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;

	hull = &loadmodel->hulls[0];

	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_Alloc(count * sizeof *out);

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

/*
=================
Mod_LoadMarksurfaces
=================
*/
void Mod_LoadMarksurfaces (lump_t *l, int ver)
{
	int		i, j, count, sz;
	byte		*in;
	msurface_t **out;

	in = (void *)(mod_base + l->fileofs);
	sz = ver == BSP2VERSION ? sizeof(u32int) : sizeof(u16int);
	if (l->filelen % sz)
		Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", loadmodel->name);
	count = l->filelen / sz;
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = ver == BSP2VERSION ? le32u(in) : le16u(in);
		if(j < 0 || j >= loadmodel->numsurfaces)
			Host_Error("Mod_ParseMarksurfaces: bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Mod_LoadSurfedges (lump_t *l)
{
	int		i, count;
	int		*in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadSurfedges: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * sizeof *out);

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}

/*
=================
Mod_LoadPlanes
=================
*/
void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadPlanes: funny lump size in %s", loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_Alloc(count * 2 * sizeof *out);

	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

/*
=================
RadiusFromBounds
=================
*/
float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);
	}

	return Length (corner);
}

/*
=================
Mod_LoadBrushModel
=================
*/
void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j, ver;
	dheader_t	*header;
	dmodel_t 	*bm;

	loadmodel->type = mod_brush;

	header = (dheader_t *)buffer;

	ver = LittleLong (header->version);
	if (ver != BSPVERSION && ver != BSP2VERSION)
		Host_Error("Mod_LoadBrushModel: %s has wrong version number (%ux should be %ux or %ux)", mod->name, ver, BSPVERSION, BSP2VERSION);

	// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<(int)sizeof(dheader_t)/4 ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	// load into heap

	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);
	Mod_LoadEdges (&header->lumps[LUMP_EDGES], ver);
	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);
	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadLighting (&header->lumps[LUMP_LIGHTING]);
	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);
	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);
	Mod_LoadFaces (&header->lumps[LUMP_FACES], ver);
	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES], ver);
	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS], ver);
	Mod_LoadNodes (&header->lumps[LUMP_NODES], ver);
	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES], ver);
	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);
	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

	Mod_MakeHull0 ();

	mod->numframes = 2;		// regular and alternate animation
	mod->flags = 0;

	// set up the submodels (FIXME: this is confusing)
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}

		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		mod->blend = false;
		for(j = bm->firstface; j < bm->firstface+bm->numfaces; j++){
			if(loadmodel->surfaces[j].flags & SURF_TRANS){
				mod->blend = true;
				break;
			}
		}

		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);
		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[16];

			snprint(name, sizeof(name), "*%d", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

/*
==============================================================================

ALIAS MODELS

==============================================================================
*/

/*
=================
Mod_LoadAliasFrame
=================
*/
void * Mod_LoadAliasFrame (void * pin, int *pframeindex, int numv,
	trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	trivertx_t		*pframe, *pinframe;
	int				i, j;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (name, pdaliasframe->name);

	for (i=0 ; i<3 ; i++)
	{
		// these are byte values, so we don't have to worry about
		// endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);
	*pframeindex = Hunk_From(pheader);
	pframe = Hunk_Alloc(numv * sizeof *pframe);

	for (j=0 ; j<numv ; j++)
	{
		int		k;

		// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k=0 ; k<3 ; k++)
		{
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	pinframe += numv;

	return (void *)pinframe;
}


/*
=================
Mod_LoadAliasGroup
=================
*/
void * Mod_LoadAliasGroup (void * pin, int *pframeindex, int numv,
	trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	daliasgroup_t		*pingroup;
	maliasgroup_t		*paliasgroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	*pframeindex = Hunk_From(pheader);
	paliasgroup = Hunk_Alloc(sizeof(*paliasgroup) +
			(numframes - 1) * sizeof paliasgroup->frames[0]);

	paliasgroup->numframes = numframes;

	for (i=0 ; i<3 ; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pingroup->bboxmin.v[i];
		pbboxmax->v[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	paliasgroup->intervals = Hunk_From(pheader);
	poutintervals = Hunk_Alloc(numframes * sizeof *poutintervals);

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error("Mod_LoadAliasGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadAliasFrame (ptemp,
									&paliasgroup->frames[i].frame,
									numv,
									&paliasgroup->frames[i].bboxmin,
									&paliasgroup->frames[i].bboxmax,
									pheader, name);
	}

	return ptemp;
}

void *
Mod_LoadAliasSkin(void * pin, int *pskinindex, int skinsize, aliashdr_t *pheader)
{
	uchar *pskin, *pinskin;

	*pskinindex = Hunk_From(pheader);
	pskin = Hunk_Alloc(skinsize);
	pinskin = (uchar *)pin;
	memcpy(pskin, pinskin, skinsize);
	pinskin += skinsize;
	return (void *)pinskin;
}


/*
=================
Mod_LoadAliasSkinGroup
=================
*/
void * Mod_LoadAliasSkinGroup (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	daliasskingroup_t		*pinskingroup;
	maliasskingroup_t		*paliasskingroup;
	int						i, numskins;
	daliasskininterval_t	*pinskinintervals;
	float					*poutskinintervals;
	void					*ptemp;

	pinskingroup = (daliasskingroup_t *)pin;

	numskins = LittleLong (pinskingroup->numskins);

	*pskinindex = Hunk_From(pheader);
	paliasskingroup = Hunk_Alloc(sizeof(*paliasskingroup) +
		(numskins - 1) * sizeof paliasskingroup->skindescs[0]);

	paliasskingroup->numskins = numskins;

	pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

	paliasskingroup->intervals = Hunk_From(pheader);
	poutskinintervals = Hunk_Alloc(numskins * sizeof *poutskinintervals);

	for (i=0 ; i<numskins ; i++)
	{
		*poutskinintervals = LittleFloat (pinskinintervals->interval);
		if (*poutskinintervals <= 0)
			Host_Error("Mod_LoadAliasSkinGroup: interval<=0");

		poutskinintervals++;
		pinskinintervals++;
	}

	ptemp = (void *)pinskinintervals;

	for (i=0 ; i<numskins ; i++)
	{
		ptemp = Mod_LoadAliasSkin (ptemp,
				&paliasskingroup->skindescs[i].skin, skinsize, pheader);
	}

	return ptemp;
}


/*
=================
Mod_LoadAliasModel
=================
*/
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i;
	mdl_t				*pmodel, *pinmodel;
	stvert_t			*pstverts, *pinstverts;
	aliashdr_t			*pheader;
	mtriangle_t			*ptri;
	dtriangle_t			*pintriangles;
	int					version, numframes, numskins;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	maliasskindesc_t	*pskindesc;
	int					skinsize;

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error("%s has wrong version number (%d should be %d)",
				 mod->name, version, ALIAS_VERSION);

	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mdl_t) +
			LittleLong (pinmodel->numverts) * sizeof (stvert_t) +
			LittleLong (pinmodel->numtris) * sizeof (mtriangle_t);

	pheader = Hunk_Alloc(size);
	pmodel = (mdl_t *) ((byte *)&pheader[1] +
			(LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]));
	pheader->model = (byte *)pmodel - (byte *)pheader;

	//mod->cache.data = pheader;
	mod->flags = LittleLong (pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Host_Error("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Host_Error("model %s has no vertices", mod->name);

	if (pmodel->numverts > MAXALIASVERTS)
		Host_Error("model %s has too many vertices", mod->name);

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Host_Error("model %s has no triangles", mod->name);

	pmodel->numframes = LittleLong (pinmodel->numframes);
	pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

	if (pmodel->skinwidth & 0x03)
		Host_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");

	// load the skins
	skinsize = pmodel->skinheight * pmodel->skinwidth;

	if (numskins < 1)
		Host_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	pskintype = (daliasskintype_t *)&pinmodel[1];

	pheader->skindesc = Hunk_From(pheader);
	pskindesc = Hunk_Alloc(numskins * sizeof *pskindesc);

	for (i=0 ; i<numskins ; i++)
	{
		aliasskintype_t	skintype;

		skintype = LittleLong (pskintype->type);
		pskindesc[i].type = skintype;

		if (skintype == ALIAS_SKIN_SINGLE)
		{
			pskintype = (daliasskintype_t *)
					Mod_LoadAliasSkin (pskintype + 1,
									   &pskindesc[i].skin,
									   skinsize, pheader);
		}
		else
		{
			pskintype = (daliasskintype_t *)
					Mod_LoadAliasSkinGroup (pskintype + 1,
											&pskindesc[i].skin,
											skinsize, pheader);
		}
	}

	// set base s and t vertices
	pstverts = (stvert_t *)&pmodel[1];
	pinstverts = (stvert_t *)pskintype;

	pheader->stverts = (byte *)pstverts - (byte *)pheader;

	for (i=0 ; i<pmodel->numverts ; i++)
	{
		pstverts[i].onseam = LittleLong (pinstverts[i].onseam);
	// put s and t in 16.16 format
		pstverts[i].s = LittleLong (pinstverts[i].s) << 16;
		pstverts[i].t = LittleLong (pinstverts[i].t) << 16;
	}

	// set up the triangles
	ptri = (mtriangle_t *)&pstverts[pmodel->numverts];
	pintriangles = (dtriangle_t *)&pinstverts[pmodel->numverts];

	pheader->triangles = (byte *)ptri - (byte *)pheader;

	for (i=0 ; i<pmodel->numtris ; i++)
	{
		int		j;

		ptri[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			ptri[i].vertindex[j] =
					LittleLong (pintriangles[i].vertindex[j]);
		}
	}

	// load the frames
	if (numframes < 1)
		Host_Error("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pframetype = (daliasframetype_t *)&pintriangles[pmodel->numtris];

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;

		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasFrame (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasGroup (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
	}

	mod->type = mod_alias;

	// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;

	// move the complete, relocatable alias model to the cache

	Hunk_CacheFrom(&mod->cache, pheader);
}

void *
Mod_LoadSpriteFrame(void * pin, mspriteframe_t **ppframe)
{
	int width, height, size, origin[2];
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong(pinframe->width);
	height = LittleLong(pinframe->height);
	size = width * height;

	pspriteframe = Hunk_Alloc(size + sizeof *pspriteframe);

	memset(pspriteframe, 0, size + sizeof *pspriteframe);
	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	memcpy(&pspriteframe->pixels[0], (uchar *)(pinframe + 1), size);
	return (void *)((byte *)pinframe + size + sizeof *pinframe);
}


/*
=================
Mod_LoadSpriteGroup
=================
*/
void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe)
{
	dspritegroup_t		*pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_Alloc(sizeof(*pspritegroup) +
		(numframes - 1) * sizeof pspritegroup->frames[0]);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Hunk_Alloc(numframes * sizeof *poutintervals);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i]);
	}

	return ptemp;
}


/*
=================
Mod_LoadSpriteModel
=================
*/
void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;

	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Host_Error("%s has wrong version number "
				 "(%d should be %d)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_Alloc(size);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

	// load the frames
	if (numframes < 1)
		Host_Error("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;
	mod->flags = 0;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteFrame (pframetype + 1,
										 &psprite->frames[i].frameptr);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (pframetype + 1,
										 &psprite->frames[i].frameptr);
		}
	}

	mod->type = mod_sprite;
}

//=============================================================================

/*
================
Mod_Print
================
*/
void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s",mod->cache.data, mod->name);
		if (mod->needload & NL_UNREFERENCED)
			Con_Printf (" (!R)");
		if (mod->needload & NL_NEEDS_LOADED)
			Con_Printf (" (!P)");
		Con_Printf ("\n");
	}
}


