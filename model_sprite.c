#include "quakedef.h"

static byte *
Mod_LoadSpriteFrame(byte *in, byte *e, mspriteframe_t **ppframe)
{
	int w, h, size, origin[2];
	mspriteframe_t *pspriteframe;

	if(e-in < 4*4){
toosmall:
		werrstr("truncated?");
		return nil;
	}

	origin[0] = le32(in);
	origin[1] = le32(in);
	w = le32(in);
	h = le32(in);
	if(w < 0 || h < 0){
		werrstr("invalid dimensions: %dx%d", w, h);
		return nil;
	}
	if(e-in < (size = w*h))
		goto toosmall;

	*ppframe = pspriteframe = Hunk_Alloc(sizeof(*pspriteframe) + size*sizeof(pixel_t));
	pspriteframe->width = w;
	pspriteframe->height = h;
	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - h;
	pspriteframe->left = origin[0];
	pspriteframe->right = w + origin[0];
	torgbx(in, pspriteframe->pixels, size);
	in += size;

	return in;
}

static byte *
Mod_LoadSpriteGroup(byte *in, byte *e, mspritegroup_t **ppgroup)
{
	mspritegroup_t *spgrp;
	float *poutintervals;
	int i, numframes;

	if(e-in < 4){
toosmall:
		werrstr("truncated?");
		return nil;
	}
	if((numframes = le32(in)) < 0){
		werrstr("invalid number of frames: %d", numframes);
		return nil;
	}
	*ppgroup = spgrp = Hunk_Alloc(
		sizeof(*spgrp) +
		numframes*(sizeof(*spgrp->frames) + sizeof(*spgrp->intervals))
	);
	spgrp->numframes = numframes;

	poutintervals = (void*)(spgrp->frames + numframes);
	for(i = 0; i < numframes; i++){
		if(e-in < 4)
			goto toosmall;
		if((*poutintervals++ = f32(in)) <= 0.0){
			werrstr("interval <= 0");
			return nil;
		}
	}

	for(i = 0; i < numframes; i++){
		in = Mod_LoadSpriteFrame(in, e, &spgrp->frames[i]);
		if(in == nil)
			break;
	}

	return in;
}

void
Mod_LoadSpriteModel(model_t *mod, byte *in0, int total)
{
	int version, numframes, i;
	msprite_t *psprite;
	byte *in, *e;

	if(total < 9*4){
toosmall:
		werrstr("file too small");
		goto err;
	}

	in = in0 + 4;
	e = in0 + total;
	if((version = le32(in)) != SPRITE_VERSION){
		werrstr("wrong version number (%d should be %d)", version, SPRITE_VERSION);
		goto err;
	}

	in = in0 + 24;
	if((numframes = le32(in)) < 1){
		werrstr("invalid number of frames: %d", numframes);
		goto err;
	}

	psprite = Hunk_Alloc(sizeof(*psprite) + numframes*sizeof(*psprite->frames));
	mod->cache.data = psprite;

	in = in0 + 8;
	psprite->type = le32(in);
	psprite->boundingradius = f32(in);
	psprite->maxwidth = le32(in);
	psprite->maxheight = le32(in);
	psprite->numframes = le32(in);
	psprite->beamlength = f32(in);
	mod->synctype = le32(in);

	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->maxs[2] = psprite->maxheight/2;
	mod->mins[0] = mod->mins[1] = -mod->maxs[0];
	mod->mins[2] = -mod->maxs[2];

	mod->type = mod_sprite;
	mod->numframes = numframes;
	mod->flags = 0;

	for(i = 0; i < numframes; i++){
		if(e-in < 4)
			goto toosmall;
		if((psprite->frames[i].type = le32(in)) == SPR_SINGLE)
			in = Mod_LoadSpriteFrame(in, e, &psprite->frames[i].frameptr);
		else
			in = Mod_LoadSpriteGroup(in, e, &psprite->frames[i].framegrp);
		if(in == nil){
			werrstr("frame(group) %d: %s", i, lerr());
			goto err;
		}
	}
	return;

err:
	Host_Error("Mod_LoadSpriteModel: %s: %s", mod->name, lerr());
}
