#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"

#define P \
	do{ \
		if(*z <= (izi >> 16)) \
			*pdest = blendalpha(pbase[(s >> 16) + (t >> 16) * cachewidth], *pdest, alpha); \
		pdest++; \
		z++; \
		s += sstep; \
		t += tstep; \
	}while(0)

void
dospan_alpha(uchar *pdest, uchar *pbase, int s, int t, int sstep, int tstep, int spancount, int cachewidth, u8int alpha, uzint *z, int izi)
{
	switch(spancount)
	{
	case 16: P; case 15: P; case 14: P; case 13: P; case 12: P; case 11: P; case 10: P; case 9: P;
	case  8: P; case  7: P; case  6: P; case  5: P; case  4: P; case  3: P; case  2: P; case 1: P;
	}
	USED(pdest, s, t, z);
}
