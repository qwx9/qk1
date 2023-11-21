#include "quakedef.h"

void
dospan_alpha(byte *pdest, byte *pbase, int s, int t, int sstep, int tstep, int spancount, int cachewidth, u8int alpha, uzint *pz, int izi, int izistep)
{
	uchar pix;

	if(alpha != 255){
		do{
			pix = pbase[(s >> 16) + (t >> 16) * cachewidth];
			if(pix != 255 && *pz <= (izi >> 16))
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
			if(pix != 255 && *pz <= (izi >> 16)){
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
