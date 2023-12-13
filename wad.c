#include "quakedef.h"

enum {
	WAD_VER2 = 'W'<<0|'A'<<8|'D'<<16|'2'<<24,
	WAD_VER3 = 'W'<<0|'A'<<8|'D'<<16|'3'<<24,

	TYP_QPIC = 0x42,
	TYP_MIPTEX = 0x43, /* it IS in Half-Life */
};

typedef struct Lmp Lmp;

struct Lmp {
	char name[16];
	int off;
	int sz;
	int type;
};

struct Wad {
	byte *in;
	int sz;
	int ver;
	int numlumps;
	Lmp lumps[];
};

Wad *
W_OpenWad(char *path)
{
	Wad *w;
	Lmp *lmp;
	byte *in, *p;
	int sz, ver, off, n, i;

	if((in = loadhunklmp(path, &sz)) == nil)
		goto err;
	if(sz < 4+4+4){
		werrstr("invalid size: %d", sz);
		goto err;
	}
	p = in;
	ver = le32(p);
	if(ver != WAD_VER2 && ver != WAD_VER3){
		werrstr("unsupported version: %c%c%c%c", (char)in[0], (char)in[1], (char)in[2], (char)in[3]);
		goto err;
	}
	n = le32(p);
	off = le32(p);
	if(off < 0 || n < 0 || off+n*32 > sz){
		werrstr("invalid wad: off=%d numlumps=%d sz=%d", off, n, sz);
		goto err;
	}
	w = Hunk_Alloc(sizeof(*w) + sizeof(*w->lumps)*n);
	w->in = in;
	w->sz = sz;
	w->ver = ver;
	w->numlumps = n;
	p = in + off;
	for(lmp = w->lumps; n-- > 0; lmp++){
		lmp->off = le32(p);
		p += 4; /* disksize */
		lmp->sz = le32(p);
		lmp->type = *p++;
		p += 1+2; /* compression + padding */
		memmove(lmp->name, p, sizeof(lmp->name));
		for(i = 0; i < nelem(lmp->name) && lmp->name[i]; i++)
			lmp->name[i] = tolower(lmp->name[i]);
		memset(lmp->name+i, 0, nelem(lmp->name)-i);
		p += nelem(lmp->name);
	}

	return w;
err:
	werrstr("W_OpenWad: %s: %s", path, lerr());
	return nil;
}

static Lmp *
W_FindName(Wad *w, char *name)
{
	int i;
	Lmp *lmp;
	char t[16];

	for(i = 0; i < nelem(lmp->name) && name[i]; i++)
		t[i] = tolower(name[i]);
	memset(t+i, 0, sizeof(t)-i);
	for(i = 0, lmp = w->lumps; i < w->numlumps; i++, lmp++){
		if(strncmp(lmp->name, t, nelem(lmp->name)) == 0)
			return lmp;
	}
	werrstr("%s: not found", name);
	return nil;
}

qpic_t *
W_ReadQpic(Wad *wad, char *name, mem_user_t *c)
{
	int i, n, w, h, palsz, j;
	Lmp *lmp;
	byte *p, *pal;
	qpic_t *q;
	mem_user_t dummy = {0};

	if(c == nil){
		memset(&dummy, 0, sizeof(dummy));
		c = &dummy;
	}
	if((q = Cache_Check(c)) != nil)
		return q;
	if((lmp = W_FindName(wad, name)) == nil || lmp->type != TYP_QPIC)
		return nil;
	p = wad->in + lmp->off;
	w = le32(p);
	h = le32(p);
	n = w*h;
	if(w < 0 || h < 0){
		werrstr("invalid size: %dx%d", w, h);
		goto err;
	}

	pal = nil;
	palsz = 0;
	if(wad->ver == WAD_VER2){
		if(lmp->sz < 4+4+n){
			werrstr("truncated: %d < %d", lmp->sz, 4+4+n);
			goto err;
		}
	}else if(wad->ver == WAD_VER3){
		pal = p + n;
		palsz = le16(pal);
		if(palsz < 0 || palsz > 256 || lmp->sz < 4+4+n+2+palsz*3){
			werrstr("invalid: palsz=%d, %d < %d", palsz, lmp->sz, 4+4+n+2+palsz*3);
			goto err;
		}
	}

	q = Cache_Alloc(c, sizeof(*q) + n*sizeof(pixel_t));
	q->width = w;
	q->height = h;

	if(wad->ver == WAD_VER2){
		for(i = 0; i < n; i++)
			q->pixels[i] = q1pal[p[i]];
	}else if(wad->ver == WAD_VER3 && palsz > 0){
		for(i = 0; i < n; i++){
			j = (*p++)*3;
			q->pixels[i] = j < palsz*3 ? (0xff<<24 | pal[j+0]<<16 | pal[j+1]<<8 | pal[j+2]) : 0;
		}
	}

	return q;
err:
	werrstr("%.*s: %s", nelem(lmp->name), lmp->name, lerr());
	return nil;
}

static int
W_ReadPixelsAt(Wad *wad, char *name, int off, int sz, pixel_t *out, int num)
{
	int n, palsz, x, fb;
	byte *t, *pal;

	num = min(num, sz);
	num = min(num, wad->sz-off);
	t = wad->in + off;
	if(wad->ver == WAD_VER2){
		for(n = 0; n < num; n++)
			*out++ = q1pal[*t++];
	}else if(wad->ver == WAD_VER3){
		if(off+num+2 >= wad->sz){
			werrstr("invalid lump: %d > %d", off+num+2, wad->sz);
			return -1;
		}
		pal = t + num;
		palsz = le16(pal);
		if(palsz <= 0 || palsz > 256 || off+num+2+palsz*3 > wad->sz){
			werrstr("invalid palette: palsz=%d pal_end=%d wad_sz=%d",
				palsz, off+num+2+palsz*3, wad->sz);
			goto err;
		}
		palsz *= 3;
		fb = strchr(name, '~') != nil;
		for(n = 0; n < num; n++, t++, out++){
			x = *t*3;
			if(x >= palsz || (name[0] == '{' && x == palsz-3))
				*out = 0;
			else
				*out = ((fb && x >= (256-32)*3) ? 0 : 0xff)<<24 | pal[x+0]<<16 | pal[x+1]<<8 | pal[x+2];
		}
	}
	return num;
err:
	return -1;
}

int
W_ReadMipTex(Wad *wad, char *name, texture_t *t)
{
	Lmp *lmp;
	byte *p;
	int i, w, h, n, off;

	if((lmp = W_FindName(wad, name)) == nil || lmp->type != TYP_MIPTEX)
		return -1;
	p = wad->in + lmp->off + 16;
	w = le32(p);
	h = le32(p);
	if(w != t->width || h != t->height){
		werrstr("%s: size mismatch: (%d->%d)x(%d->%d)\n", name, w, t->width, h, t->height);
		return -1;
	}
	n = w*h*85/64;
	for(i = 0; i < nelem(t->offsets); i++)
		t->offsets[i] = le32(p) - (16+2*4+4*4);
	off = p - wad->in;
	if((n = W_ReadPixelsAt(wad, name, off, lmp->off+lmp->sz-off, (pixel_t*)(t+1), n)) < 0)
		werrstr("%s: %s", name, lerr());
	return n;
}

int
W_ReadPixels(Wad *wad, char *name, pixel_t *out, int num)
{
	Lmp *lmp;
	int n;

	if((lmp = W_FindName(wad, name)) == nil)
		return -1;
	if((n = W_ReadPixelsAt(wad, name, lmp->off, lmp->sz, out, num)) < 0)
		werrstr("%s: %s", name, lerr());
	return n;
}

int
W_ReadRaw(Wad *wad, char *name, byte *out, int num)
{
	Lmp *lmp;

	if((lmp = W_FindName(wad, name)) == nil)
		return -1;
	num = min(num, lmp->sz);
	num = min(num, wad->sz-lmp->off);
	memmove(out, wad->in+lmp->off, num);
	return num;
}
