#include "quakedef.h"

pixel_t q1pal[256];

void
stopfb(void)
{
}

void
flipfb(void)
{
}

void
setpal(byte *p0)
{
	int x;
	byte *p;

	for(p = p0, x = 0; x < 256; x++, p += 3)
		q1pal[x] = (x < 256-32 ? 0xff : 0)<<24 | p[0]<<16 | p[1]<<8 | p[2];
	q1pal[255] &= 0;

	scr_fullupdate = 0;
}

char *
sys_wrpath(void)
{
	return "";
}

void
initfb(void)
{
}
