#include "quakedef.h"

pixel_t
blendadditive(pixel_t ca, pixel_t cb, int alpha)
{
	int a, b, c;

	if(ca == 0 || (ca >> 24) != 0){
		alpha = alpha*(ca >> 24) + 0x80;
		alpha = ((alpha>>8) + alpha) >> 8;
	}

	ca = mulalpha(ca, alpha);
	a = ((ca>> 0)&0xff) + ((cb>> 0)&0xff);
	b = ((ca>> 8)&0xff) + ((cb>> 8)&0xff);
	c = ((ca>>16)&0xff) + ((cb>>16)&0xff);
	return (cb & 0xff000000) | min(a, 255) | min(b, 255)<<8 | min(c, 255)<<16;
}

pixel_t
blendalpha(pixel_t ca, pixel_t cb, int alpha)
{
	if(ca == 0 || (ca >> 24) != 0){
		alpha = alpha*(ca >> 24) + 0x80;
		alpha = ((alpha>>8) + alpha) >> 8;
	}

	return mulalpha(ca, alpha) + mulalpha(cb, 255-alpha);
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
