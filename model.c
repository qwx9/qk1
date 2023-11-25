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


