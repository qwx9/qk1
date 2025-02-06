#include <u.h>
#include <libc.h>
#include <tos.h>

enum {
	Nsec = 1000000000ULL,
};

/*
 * nsec() is wallclock and can be adjusted by timesync
 * so need to use cycles() instead, but fall back to
 * nsec() in case we can't
 */
u64int
nanosec(void)
{
	static u64int fasthz, xstart;
	u64int x;

	if(fasthz == ~0ULL)
		return nsec() - xstart;

	if(fasthz == 0){
		if(_tos->cyclefreq){
			fasthz = _tos->cyclefreq;
			cycles(&xstart);
		} else {
			fasthz = ~0ULL;
			xstart = nsec();
		}
		return 0;
	}
	cycles(&x);
	x -= xstart;

	u64int q = x / fasthz;
	u64int r = x % fasthz;

	return q*Nsec + r*Nsec/fasthz;
}
