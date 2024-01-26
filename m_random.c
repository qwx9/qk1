#include "quakedef.h"
#include "mt19937-64.h"

static mt19937_64 ctx;

void
m_random_init(u64int seed)
{
	init_genrand64(&ctx, seed);
}

u64int
m_random_64(void)
{
	return genrand64_int64(&ctx);
}

s64int
m_random_63(void)
{
	return genrand64_int63(&ctx);
}

double
m_random_i0i1(void)
{
	return genrand64_real1(&ctx);
}

double
m_random_i0e1(void)
{
	return genrand64_real2(&ctx);
}

double
m_random_e0e1(void)
{
	return genrand64_real3(&ctx);
}
