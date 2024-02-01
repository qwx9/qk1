#include "quakedef.h"

long
sndqueued(void)
{
	return 0;
}

void
sndstop(void)
{
}

void
sndwrite(byte *buf, long sz)
{
	USED(buf); USED(sz);
}

void
sndclose(void)
{
}

int
sndopen(void)
{
	return -1;
}
