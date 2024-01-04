typedef struct fog_t fog_t;

struct fog_t {
	pixel64_t v;
	pixel64_t d;
};

#define fogstep(f) do{ \
		(f).v += (f).d; \
	}while(0)

static inline byte
z2foga(uzint z)
{
	float d;

	if(z <= 65536)
		return 0;
	d = 65536ULL*65536ULL / (u64int)z;
	d = 1.0 - exp2(-fogvars.density * d*d);
	return 255*d;
}

static inline pixel64_t
pixto64(pixel_t p)
{
	return
		(pixel64_t)((p>>24)&0xff)<<56 |
		(pixel64_t)((p>>16)&0xff)<<40 |
		(pixel64_t)((p>> 8)&0xff)<<24 |
		(pixel64_t)((p>> 0)&0xff)<< 8;
}

static inline pixel_t
pixfrom64(pixel64_t p)
{
	return ((p>>56)&0xff)<<24 | ((p>>40)&0xff)<<16 | ((p>>24)&0xff)<<8 | ((p>>8)&0xff)<<0;
}

static inline pixel_t
blendfog(pixel_t pix, fog_t fog)
{
	pixel_t a = pixfrom64(fog.v);
	pixel_t b = mulalpha(pix, 0xff - (fog.v>>56));
	return a + b;
}

static inline bool
fogcalc(uzint zi0, uzint zi1, int cnt, fog_t *f)
{
	int v, e;
	pixel64_t x;

	if((v = z2foga(zi0)) == 0 || (e = z2foga(zi1)) == 0)
		return false;

	f->v = pixto64(mulalpha(fogvars.pix, v));
	x = pixto64(mulalpha(fogvars.pix, (e<v ? v-e : e-v)));
	f->d =
		(((x & (0xffffULL<<48))/cnt) & (0xffffULL<<48)) |
		(((x & (0xffffULL<<32))/cnt) & (0xffffULL<<32)) |
		(((x & (0xffffULL<<16))/cnt) & (0xffffULL<<16)) |
		(((x & (0xffffULL<< 0))/cnt) & (0xffffULL<< 0));
	if(e < v)
		f->d = -f->d;

	return true;
}
