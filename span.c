#include "quakedef.h"

void
dospan(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int cachewidth)
{
	do{
		*pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth];
		s += sstep;
		t += tstep;
	}while(--spancount);
}
