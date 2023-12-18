#include "quakedef.h"

typedef struct Header Header;
typedef struct Footer Footer;
typedef struct Tga Tga;

struct Header {
	int idlen;
	int paltype;
	int imgtype;
	int palfirst;
	int pallen;
	int palbpp;
	int ox;
	int oy;
	int w;
	int h;
	int bpp;
	int flags;
};

struct Tga {
	Header;

	int alphasz;
	int pxsz;
	byte *pal;
	bool hasalpha;
};

struct Footer {
	u32int extoff;
	u32int ddoff;
	char sig[18];
};

enum {
	FlagOriginRight = 1<<4,
	FlagOriginTop = 1<<5,
	FlagAlphaSizeMask = 0x0f,

	ImgTypePaletted = 1,
	ImgTypeTrueColor,
	ImgTypeMonoChrome,
	ImgTypeFlagRLE = 1<<3,

	ExtAreaAttrTypeOffset = 0x1ee,

	AttrTypeAlpha = 3,
	AttrTypePremultipliedAlpha,

	HeaderSize = 18,
	FooterSize = 26,
};

static const char tga_sig[] = "TRUEVISION-XFILE.\0";

static void
w2rgb(int w, byte *r, byte *g, byte *b)
{
	*b = (w>>0) & 31;
	*b = (*b<<3) + (*b>>2);
	*g = (w>>5) & 31;
	*g = (*g<<3) + (*g>>2);
	*r = (w>>10) & 31;
	*r = (*r<<3) + (*r>>2);
}

static byte *
getpixel(Tga *h, pixel_t *out, byte *p, int *sz)
{
	byte r, g, b, a;
	int w;

	if(*sz < h->pxsz){
		werrstr("truncated?");
		return nil;
	}
	r = g = b = a = 0xff;
	switch(h->pxsz){
	case 4:
		if(h->hasalpha)
			a = p[3];
		/* fallthrough */
	case 3:
		b = p[0];
		g = p[1];
		r = p[2];
		break;
	case 2:
		if(h->imgtype == ImgTypeMonoChrome){
			r = g = b = p[0];
			if(h->hasalpha)
				a = p[1];
		}else{
			w = p[1]<<8 | p[0];
			w2rgb(w, &r, &g, &b);
			if(h->hasalpha && (w & (1<<15)) == 0)
				a = 0;
		}
		break;
	case 1:
		if(h->pal != nil){
			w = p[0];
			if(w >= h->pallen){
				werrstr("out of palette range: %d/%d", w, h->pallen);
				return nil;
			}
			if(h->palbpp == 24){
				w *= 3;
				b = h->pal[w+0];
				g = h->pal[w+1];
				r = h->pal[w+2];
			}else if(h->palbpp == 32){
				w *= 4;
				b = h->pal[w+0];
				g = h->pal[w+1];
				r = h->pal[w+2];
				if(h->hasalpha)
					a = h->pal[w+3];
			}else if(h->palbpp == 16){
				w *= 2;
				w = h->pal[w+1]<<8 | h->pal[w+0];
				w2rgb(w, &r, &g, &b);
			}
		}else{
			r = g = b = p[0];
		}
		break;
	}

	if(a == 0)
		*out = 0;
	else
		*out = a<<24 | r<<16 | g<<8 | b<<0;
	*sz -= h->pxsz;
	return p + h->pxsz;
}

qpic_t *
TGA_Decode(byte *in, int sz, bool *premult)
{
	int i, n, cnt, x, y;
	pixel_t pix;
	byte *p, b;
	qpic_t *q;
	bool rle;
	Footer f;
	Tga h;

	if(sz < HeaderSize){
		werrstr("invalid size: %d", sz);
		goto err;
	}
	h.idlen = *in++;
	h.paltype = *in++;
	h.imgtype = *in++;
	h.palfirst = le16u(in);
	h.pallen = le16u(in);
	h.palbpp = *in++;
	h.ox = le16u(in);
	h.oy = le16u(in);
	h.w = le16u(in);
	h.h = le16u(in);
	h.bpp = *in++;
	h.flags = *in++;
	sz -= HeaderSize;
	if(h.idlen >= sz){
		werrstr("invalid idlen: %d", h.idlen);
		goto err;
	}
	in += h.idlen;
	sz -= h.idlen;
	if(h.w < 1 || h.h < 1){
		werrstr("invalid size: %dx%d", h.w, h.h);
		goto err;
	}

	rle = h.imgtype & ImgTypeFlagRLE;
	h.imgtype &= 3;

	h.alphasz = h.flags & FlagAlphaSizeMask;
	if(h.alphasz != 0 && h.alphasz != 1 && h.alphasz != 8){
		werrstr("invalid alpha size: %d", h.alphasz);
		goto err;
	}
	*premult = false;
	h.hasalpha =
		(h.alphasz > 0 || h.bpp == 32) ||
		(h.imgtype == ImgTypeMonoChrome && h.bpp == 16) ||
		(h.imgtype == ImgTypePaletted && h.palbpp == 32);
	h.pxsz = h.bpp >> 3;

	if(sz > h.idlen + FooterSize){
		p = in + sz - FooterSize;
		f.extoff = le32u(p);
		f.ddoff = le32u(p);
		if(f.extoff != 0 && memcmp(p, tga_sig, sizeof(f.sig)) == 0){
			f.extoff += ExtAreaAttrTypeOffset - HeaderSize;
			if(f.extoff >= (u32int)sz){
				werrstr("extoff out of range: %d", f.extoff);
				goto err;
			}
			switch(in[f.extoff]){
			case AttrTypeAlpha:
				h.hasalpha = true;
				break;
			case AttrTypePremultipliedAlpha:
				h.hasalpha = true;
				*premult = true;
			default:
				h.hasalpha = false;
				break;
			}
		}
	}

	if(
		(h.imgtype == ImgTypePaletted && (
			h.paltype != 1 ||
			h.bpp != 8 ||
			(h.palbpp != 15 && h.palbpp != 16 && h.palbpp != 24 && h.palbpp != 32))) ||
		(h.imgtype == ImgTypeTrueColor && (
			h.bpp != 32 && h.bpp != 16 && (h.bpp != 24 || h.hasalpha))) ||
		(h.imgtype == ImgTypeMonoChrome && (
			(h.bpp != 16 && h.hasalpha) ||
			(h.bpp != 8 && !h.hasalpha)))){
		werrstr("invalid/unsupported: imgtype=%d bpp=%d pal=[type=%d bpp=%d] hasalpha=%d",
			h.imgtype, h.bpp, h.paltype, h.palbpp, h.hasalpha
		);
		goto err;
	}

	if(h.imgtype == ImgTypePaletted){
		if(h.pallen <= h.palfirst){
			werrstr("invalid palfirst/pallen: %d/%d", h.palfirst, h.pallen);
			goto err;
		}
		h.pallen -= h.palfirst;
		n = (h.palbpp+1) >> 3;
		i = n*((int)h.palfirst + (int)h.pallen);
		if(i >= sz){
			werrstr("truncated?");
			goto err;
		}
		h.pal = in + n*h.palfirst;
		in += i;
		sz -= i;
	}else{
		h.pal = nil;
	}

	q = Hunk_Alloc(sizeof(*q) + h.w*h.h*sizeof(pixel_t));
	q->width = h.w;
	q->height = h.h;
	n = q->width * q->height;
	if(rle){
		for(i = 0; i < n && sz > 0;){
			b = *in++;
			sz--;
			cnt = b;
			if(cnt & (1<<7)){
				cnt &= ~(1<<7);
				if((in = getpixel(&h, &q->pixels[i++], in, &sz)) == nil)
					goto err;
				for(; cnt > 0 && i < n; cnt--, i++)
					q->pixels[i] = q->pixels[i-1];
			}else{
				for(cnt++; cnt > 0 && i < n; cnt--, i++){
					if((in = getpixel(&h, &q->pixels[i], in, &sz)) == nil)
						goto err;
				}
			}
		}
	}else{
		for(i = 0; i < n; i++){
			if((in = getpixel(&h, &q->pixels[i], in, &sz)) == nil)
				goto err;
		}
	}

	if((h.flags & FlagOriginRight) != 0){
		for(y = 0; y < q->height; y++){
			for(x = 0; x < q->width/2; x++){
				pix = q->pixels[x];
				q->pixels[x] = q->pixels[q->width-x-1];
				q->pixels[q->width-x-1] = pix;
			}
		}
	}
	if((h.flags & FlagOriginTop) == 0){
		for(y = 0; y < q->height/2; y++){
			for(x = 0; x < q->width; x++){
				pix = q->pixels[y*q->width+x];
				q->pixels[y*q->width+x] = q->pixels[(q->height-y-1)*q->width+x];
				q->pixels[(q->height-y-1)*q->width+x] = pix;
			}
		}
	}

	return q;
err:
	werrstr("TGA_Decode: %s", lerr());
	return nil;
}
