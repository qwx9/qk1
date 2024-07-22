#include "quakedef.h"

texture_t *
Load_ExternalTexture(char *map, char *name)
{
	char path[256], tmp[128], *s;
	int i, n, sz, w, h;
	texture_t *tx;
	bool premult;
	qpic_t *q;
	byte *in;

	if((s = strrchr(map, '/')) != nil)
		map = s+1;
	strcpy(tmp, map);
	if((s = strchr(tmp, '.')) != nil)
		*s = 0;
	n = snprint(path, sizeof(path), "textures/%s/", tmp);

	/* clean up the name, add extension */
	strcpy(tmp, name);
	for(s = tmp; (s = strchr(s, '*')) != nil; s++)
		*s = '#';
	snprint(path+n, sizeof(path)-n, "%s.tga", tmp);
	if((in = loadhunklmp(path, &sz)) == nil){
		snprint(path, sizeof(path), "textures/%s.tga", tmp);
		in = loadhunklmp(path, &sz);
	}
	if(in == nil)
		return nil;
	if((q = TGA_Decode(in, sz, &premult)) == nil){
		fprintf(stderr, "Load_ExternalTexture: %s: %s\n", path, lerr());
		return nil;
	}
	n = q->width * q->height;
	PixTransform(q->pixels, n);
	tx = Hunk_Alloc(sizeof(*tx) + n*85/64*sizeof(pixel_t));
	strncpy(tx->name, name, sizeof(tx->name)-1);
	tx->name[sizeof(tx->name)-1] = 0;
	w = tx->width = q->width;
	h = tx->height = q->height;
	memmove(tx->pixels, q->pixels, n*sizeof(pixel_t));

	tx->offsets[0] = 0;
	for(i = 1; i < MIPLEVELS; i++){
		tx->offsets[i] = tx->offsets[i-1] + n;
		w /= 2;
		h /= 2;
		n = w*h;
		pixels_resize(
			tx->pixels+tx->offsets[0],
			tx->pixels+tx->offsets[i],
			tx->width, tx->height,
			w, h,
			premult,
			name[0] == '{'
		);
	}

	return tx;
}
