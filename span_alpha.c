#include "quakedef.h"

void
dospan_alpha(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int cachewidth, u8int alpha, uzint *pz, int izi, int izistep)
{
	pixel_t pix;

	if(alpha != 255){
		do{
			pix = pbase[(s >> 16) + (t >> 16) * cachewidth];
			if(opaque(pix) && *pz <= (izi >> 16))
				*pdest = blendalpha(pix, *pdest, alpha);
			pdest++;
			pz++;
			izi += izistep;
			s += sstep;
			t += tstep;
		}while(--spancount);
	}else{
		do{
			pix = pbase[(s >> 16) + (t >> 16) * cachewidth];
			if(opaque(pix) && *pz <= (izi >> 16)){
				*pdest = pix;
				*pz = izi >> 16;
			}
			pdest++;
			pz++;
			izi += izistep;
			s += sstep;
			t += tstep;
		}while(--spancount);
	}
}
