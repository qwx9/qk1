#include "quakedef.h"

static char loadname[32];	// for hunk tags

void Mod_LoadSpriteModel (model_t *mod, byte *buffer, int total);
void Mod_LoadBrushModel (model_t *mod, byte *buffer, int total);
void Mod_LoadAliasModel (model_t *mod, void *buffer);
model_t *Mod_LoadModel (model_t *mod, bool crash);

#define	MAX_MOD_KNOWN	4096
static model_t *mod_known;
static int mod_numknown;

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
	byte *buf;
	int len;

	if(mod->type == mod_alias){
		if(Cache_Check(&mod->cache)){
			mod->needload = NL_PRESENT;
			return mod;
		}
	}else if(mod->needload == NL_PRESENT)
		return mod;

	// because the world is so huge, load it one piece at a time
	if((buf = loadstklmp(mod->name, nil, 0, &len)) == nil){
		if(crash)
			Host_Error("Mod_LoadModel: %s", lerr());
		return nil;
	}

	// allocate a new model
	radix(mod->name, loadname);

	// fill it in

	// call the apropriate loader
	mod->needload = NL_PRESENT;

	switch(LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel(mod, buf);
		break;

	case IDSPRITEHEADER:
		Mod_LoadSpriteModel(mod, buf, len);
		break;

	default:
		Mod_LoadBrushModel(mod, buf, len);
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
model_t *
Mod_ForName(char *name, bool crash)
{
	return Mod_LoadModel(Mod_FindName(name), crash);
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


