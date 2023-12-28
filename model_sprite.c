#include "quakedef.h"

static byte *
Mod_LoadSpriteFrame(model_t *mod, byte *in, byte *e, mspriteframe_t **ppframe)
{
	int i, w, h, size, origin[2];
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
	if(e-in < (size = w*h)*(mod->ver == SPRITE32_VERSION ? (int)sizeof(pixel_t) : 1))
		goto toosmall;

	*ppframe = pspriteframe = Hunk_Alloc(sizeof(*pspriteframe) + size*sizeof(pixel_t));
	pspriteframe->width = w;
	pspriteframe->height = h;
	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - h;
	pspriteframe->left = origin[0];
	pspriteframe->right = w + origin[0];
	if(mod->ver == SPRITE_VERSION){
		torgbx(in, pspriteframe->pixels, size);
		in += size;
	}else if(mod->ver == SPRITE32_VERSION){
		for(i = 0; i < size; i++, in += sizeof(pixel_t))
			pspriteframe->pixels[i] = in[0]<<24 | in[3]<<16 | in[1]<<8 | in[2];
	}

	return in;
}

static byte *
Mod_LoadSpriteGroup(model_t *mod, byte *in, byte *e, mspritegroup_t **ppgroup)
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
		in = Mod_LoadSpriteFrame(mod, in, e, &spgrp->frames[i]);
		if(in == nil)
			break;
	}

	return in;
}

void
Mod_LoadSpriteModel(model_t *mod, byte *in0, int total)
{
	msprite_t *psprite;
	int numframes, i;
	byte *in, *e;

	if(total < 9*4){
toosmall:
		werrstr("file too small");
		goto err;
	}

	in = in0 + 4;
	e = in0 + total;
	if((mod->ver = le32(in)) != SPRITE_VERSION && mod->ver != SPRITE32_VERSION){
		werrstr("invalid/unsupported version number: %d", mod->ver);
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
			in = Mod_LoadSpriteFrame(mod, in, e, &psprite->frames[i].frameptr);
		else
			in = Mod_LoadSpriteGroup(mod, in, e, &psprite->frames[i].framegrp);
		if(in == nil){
			werrstr("frame(group) %d: %s", i, lerr());
			goto err;
		}
	}
	return;

err:
	Host_Error("Mod_LoadSpriteModel: %s: %s", mod->name, lerr());
}
