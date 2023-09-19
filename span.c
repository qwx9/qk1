#include <u.h>
#include <libc.h>

void
dospan(uchar *pdest, uchar *pbase, int s, int t, int sstep, int tstep, int spancount, int cachewidth)
{
	switch(spancount)
	{
	case 16: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 15: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 14: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 13: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 12: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 11: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 10: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 9: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 8: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 7: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 6: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 5: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 4: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 3: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 2: *pdest++ = pbase[(s >> 16) + (t >> 16) * cachewidth]; s += sstep; t += tstep;
	case 1: *pdest = pbase[(s >> 16) + (t >> 16) * cachewidth];
	}
}
