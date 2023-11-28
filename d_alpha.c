#include "quakedef.h"

float
alphafor(int flags)
{
	if(flags & (SURF_TELE|SURF_FENCE))
		return 1.0;
	if(flags & SURF_LAVA)
		return r_lavaalpha.value;
	if(flags & SURF_SLIME)
		return r_slimealpha.value;
	return r_wateralpha.value;
}
