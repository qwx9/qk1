#include "quakedef.h"

pixel_t
blendalpha(pixel_t ca, pixel_t cb, int alpha, uzint izi)
{
	int a, b, c;

	if(ca == 0 || (ca >> 24) != 0)
		alpha = (ca >> 24)*alpha >> 8;

	if(currententity != nil && currententity->effects & EF_ADDITIVE){
		ca = R_BlendFog(ca, izi);
		a = (alpha*((ca>> 0)&0xff) + 255*((cb>> 0)&0xff))>> 8;
		b = (alpha*((ca>> 8)&0xff) + 255*((cb>> 8)&0xff))>> 8;
		c = (alpha*((ca>>16)&0xff) + 255*((cb>>16)&0xff))>> 8;
		return (cb & 0xff000000) | min(a, 255) | min(b, 255)<<8 | min(c, 255)<<16;
	}

	return R_BlendFog(
		(cb & 0xff000000) |
		((alpha*((ca>> 0)&0xff) + (255-alpha)*((cb>> 0)&0xff))>> 8) << 0 |
		((alpha*((ca>> 8)&0xff) + (255-alpha)*((cb>> 8)&0xff))>> 8) << 8 |
		((alpha*((ca>>16)&0xff) + (255-alpha)*((cb>>16)&0xff))>> 8) << 16,
		izi
	);
}

float
alphafor(int flags)
{
	if(flags & (SURF_TELE|SURF_FENCE))
		return 1.0;
	if(flags & SURF_LAVA)
		return r_lavaalpha.value;
	if(flags & SURF_SLIME)
		return r_slimealpha.value;
	return r_wateralpha.value;
}
