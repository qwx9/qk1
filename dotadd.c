#include "quakedef.h"
#include "softfloat.h"

float
dotadd(float *a, float *b)
{
	extFloat80_t x, y, m, z;
	int i;

	f32_to_extF80M(b[3], &z);
	for(i = 0; i < 3; i++){
		f32_to_extF80M(a[i], &x);
		f32_to_extF80M(b[i], &y);
		extF80M_mul(&x, &y, &m);
		extF80M_add(&z, &m, &z);
	}

	return extF80M_to_f32(&z);;
}
