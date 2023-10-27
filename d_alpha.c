#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

/* FIXME(sigrid): this is super dumb.
 *
 * how it SHOULD be done:
 * 1) no point in 256 alphamaps, a'=255-a → (x->a->y) ≡ (y->a'->x)
 * 1.1) these are 8-bit colors, no way there are *different* variations for each alpha
 *      eg: x=3, y=9 → a≠a' → there is a very high chance (x->a->y) = (x->a'->y) if a is close to a'
 * 2) use k-d (3-d in this case) tree to look up colors faster
 * 3) if it's fast enough, can build all alpha maps just once on startup
 * 4) try different color models, this alpha blending looks like crap with liquids
 */
byte *alphamap[256];

void
buildalpha(int alpha)
{
	extern s32int fbpal[256];
	int a, b;
	byte *ca, *cb, *p;
	int rr, gg, bb;
	int i, dst, x, best;

	if(alphamap[alpha] != nil)
		return;
	alphamap[alpha] = malloc(256*256);
	p = (byte*)fbpal;
	ca = p;
	for(a = 0; a < 256; a++, ca += 4){
		cb = p;
		for(b = 0; b < 256; b++, cb++){
			rr = (alpha*ca[0] + (255 - alpha)*(*cb++))>>8;
			gg = (alpha*ca[1] + (255 - alpha)*(*cb++))>>8;
			bb = (alpha*ca[2] + (255 - alpha)*(*cb++))>>8;
			dst = 9999999;
			best = 255;
			for(i = 0; i < 768; i += 4){
				x = (rr-p[i+0])*(rr-p[i+0]) +
					(gg-p[i+1])*(gg-p[i+1]) +
					(bb-p[i+2])*(bb-p[i+2]);
				if(x < dst){
					dst = x;
					best = i;
				}
			}
			alphamap[alpha][a<<8 | b] = best/4;
		}
	}
}

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
