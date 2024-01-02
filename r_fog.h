#define fogstep(f) \
	do{ \
		(f).v[0] += (f).d[0]; \
		(f).v[1] += (f).d[1]; \
		(f).v[2] += (f).d[2]; \
		(f).v[3] += (f).d[3]; \
	}while(0)

#define fogshift 8

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

static inline pixel_t
blendfog(pixel_t pix, fog_t fog)
{
	byte inva = 0xff - (fog.v[3]>>fogshift);
	return
		((fog.v[0] + ((inva*((pix>> 0)&0xff))<<fogshift)) >> (8 + fogshift)) << 0 |
		((fog.v[1] + ((inva*((pix>> 8)&0xff))<<fogshift)) >> (8 + fogshift)) << 8 |
		((fog.v[2] + ((inva*((pix>>16)&0xff))<<fogshift)) >> (8 + fogshift)) << 16;
}

static inline bool
fogcalc(uzint zi0, uzint zi1, int cnt, fog_t *f)
{
	int end[3], v, e;

	if((v = z2foga(zi0)) == 0 || (e = z2foga(zi1)) == 0)
		return false;

	v <<= fogshift;
	e <<= fogshift;

	end[0] = e * fogvars.c0;
	end[1] = e * fogvars.c1;
	end[2] = e * fogvars.c2;
	f->v[0] = v * fogvars.c0;
	f->v[1] = v * fogvars.c1;
	f->v[2] = v * fogvars.c2;
	f->v[3] = v;
	f->d[0] = (end[0] - f->v[0])/cnt;
	f->d[1] = (end[1] - f->v[1])/cnt;
	f->d[2] = (end[2] - f->v[2])/cnt;
	f->d[3] = (e - v)/cnt;

	return true;
}
