#include "quakedef.h"
#include "softfloat.h"

float
dotadd(float *a, float *b)
{
	extFloat80_t x, y, m, z;
	union {
		float32_t v;
		float f;
	}f;
	int i;

	f.f = b[3];
	f32_to_extF80M(f.v, &z);
	for(i = 0; i < 3; i++){
		f.f = a[i]; f32_to_extF80M(f.v, &x);
		f.f = b[i]; f32_to_extF80M(f.v, &y);
		extF80M_mul(&x, &y, &m);
		extF80M_add(&z, &m, &z);
		f.v = extF80M_to_f32(&z);
	}

	return f.f;
}
