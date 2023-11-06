#include <u.h>

int
isnanf(float f)
{
	union {
		float f;
		u32int u;
	}x;

	x.f = f;
	return (x.u & (0xff<<23)) == (0xff<<23);
}
