#include "quakedef.h"

void
pal2xrgb(int n, s32int *pal, u8int *s, u32int *d)
{
	while(n-- > 0)
		*d++ = pal[*s++];
}
