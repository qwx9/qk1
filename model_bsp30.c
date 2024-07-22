#include "quakedef.h"
#include "model_bsp.h"

int
BSP30_LoadEntities(model_t *mod, byte *in, int sz)
{
	char *p, *s, *e, path[32];
	Wad *w;
	int maxw;

	mod->numwads = 0;
	if(sz == 0){
		mod->entities = nil;
		return 0;
	}

	memcpy(mod->entities = Hunk_Alloc(sz), in, sz);
	if((s = strstr((char*)mod->entities, "\"wad\"")) == nil ||
	   (s = strchr(s+5, '"')) == nil ||
	   (e = strchr(s+1, '"')) == nil)
		return 0;

	maxw = 4;
	mod->wads = Hunk_Alloc(maxw * sizeof(*mod->wads));
	for(s = s+1, p = s; s <= e; s++){
		if(p != s && (*s == ';' || s == e)){
			snprintf(path, sizeof(path), "%.*s", (int)(s-p), p);
			if((w = W_OpenWad(path)) != nil){
				mod->wads = Arr_AllocExtra(mod->wads, &maxw, mod->numwads-maxw);
				mod->wads[mod->numwads++] = w;
			}else{
				Con_Printf("BSP_LoadEntities: %s\n", lerr());
				continue;
			}
			p = s+1;
		}else if(*s == '\\')
			p = s+1;
	}
	return 0;
}

int
BSP30_LoadLighting(model_t *mod, byte *in, int sz)
{
	if(sz == 0){
		mod->lightdata = nil;
		return 0;
	}

	memcpy(mod->lightdata = Hunk_Alloc(sz), in, sz);
	LightTransform(mod->lightdata, sz);
	return 0;
}

int
BSP30_LoadFaces(model_t *mod, byte *in, int sz)
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
		if(i >= 0){
			if(i % 3)
				Con_Printf("misaligned light samples: %d\n", i);
			else{
				out->samples = mod->lightdata + i;
			}
		}

		// set the drawing flags flag
		if(strncmp(out->texinfo->texture->name, "sky", 3) == 0)
			out->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
		else if(out->texinfo->texture->name[0] == '!'){	// turbulent
			out->flags |= SURF_DRAWTURB | SURF_DRAWTILED | SURF_TRANS;
			for(i = 0; i < 2; i++){
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
		}else if(out->texinfo->texture->name[0] == '{')
			out->flags |= SURF_TRANS | SURF_FENCE;
	}
	return 0;
}

int
BSP30_LoadTextures(model_t *mod, byte *in, int sz)
{
	int off, i, j, pixels, num, max, altmax, w, h, palsz;
	byte *p, *in0, *x;
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
			tx->offsets[j] = le32(p) - (16+2*4+4*4);
		mod->textures[i] = tx;
		tx->width = w;
		tx->height = h;
		if(tx->offsets[0] >= 0){
			// the pixels immediately follow the structures
			x = p + pixels;
			if((palsz = le16(x)) > 0){
				pal3torgbx(p, tx->pixels, pixels, x, palsz);
				if(tx->name[0] == '{'){
					pixels = w*h;
					for(j = 0; j < pixels; j++){
						if(p[j] >= palsz-1) /* last color is transparent */
							tx->pixels[j] = 0;
					}
					for(j = 1; j < MIPLEVELS; j++){
						w /= 2;
						h /= 2;
						pixels_resize(
							tx->pixels+tx->offsets[0],
							tx->pixels+tx->offsets[j],
							tx->width, tx->height,
							w, h,
							false,
							true
						);
					}
				}
				if(strchr(tx->name, '~') != nil){
					tx->drawsurf |= DRAWSURF_FULLBRIGHT;
					/* last 32 colors are fullbright */
					for(j = 0; j < pixels; j++){
						if(p[j] >= palsz-32)
							tx->pixels[j] &= 0xffffff;
					}
				}
			}
		}else{
			// alternative: outside, in a wad
			for(j = 0; j < mod->numwads; j++){
				if(W_ReadMipTex(mod->wads[j], tx->name, tx) >= 0)
					break;
			}
			if(j >= mod->numwads)
				Con_Printf("missing texture: %s\n", tx->name);
		}
		if(strncmp(tx->name, "sky", 3) == 0)
			R_InitSky(nil); /* FIXME(sigrid): skybox */
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
