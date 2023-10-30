#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

typedef struct Col Col;

struct Col {
	Col *s[2];
	u8int c[3];
	u8int x;
};

static Col *lookup;

static void
ins(u8int c[3], u8int x)
{
	Col **p, *e, *l;
	int i;

	for(l = lookup, p = &lookup, e = lookup, i = 0; e != nil; i = (i+1)%3){
		l = *p;
		p = &e->s[c[i] >= e->c[i]];
		e = *p;
	}
	if(l != nil && memcmp(l->c, c, 3) == 0)
		return;
	e = calloc(1, sizeof(*e));
	memmove(e->c, c, 3);
	e->x = x;
	*p = e;
}

static void
closest_(u8int c[3], Col *t, int i, Col **best, int *min)
{
	int d, d2, nd;

	if(t == nil)
		return;

	i %= 3;
	nd = (int)t->c[i] - (int)c[i];
	i = (i+1)%3;
	d = (int)t->c[i] - (int)c[i];
	i = (i+1)%3;
	d2 = (int)t->c[i] - (int)c[i];
	d = nd*nd + d*d + d2*d2;

	if(*min > d){
		*min = d;
		*best = t;
	}

	i += 2;
	closest_(c, t->s[nd <= 0], i, best, min);
	if(nd*nd < *min)
		closest_(c, t->s[nd > 0], i, best, min);
}

static Col *
closest(u8int c[3])
{
	Col *best;
	int min;

	best = nil;
	min = 99999;
	closest_(c, lookup, 0, &best, &min);
	return best;
}

byte *alphamap[256>>AlphaStep];

void
initalpha(void)
{
	extern s32int fbpal[256];
	int a, b, ai, alpha, i;
	byte *ca, *cb, *p;
	u8int c[3];
	Col *best;

	if(lookup != nil)
		return;

	p = (byte*)fbpal;
	for(i = 0; i < 64; i++){
		ins(p+(64+i)*4, 64+i);
		ins(p+(63-i)*4, 63-i);
		ins(p+(128+i)*4, 128+i);
		ins(p+(255-i)*4, 255-i);
	}

	for(alpha = 1; alpha <= 128; alpha += 1<<AlphaStep){
		ai = alpha >> AlphaStep;
		alphamap[ai] = malloc(256*256);

		ca = p;
		for(a = 0; a < 256; a++, ca += 4){
			cb = p;
			for(b = 0; b < 256; b++, cb++){
				c[0] = (alpha*ca[0] + (255 - alpha)*(*cb++))>>8;
				c[1] = (alpha*ca[1] + (255 - alpha)*(*cb++))>>8;
				c[2] = (alpha*ca[2] + (255 - alpha)*(*cb++))>>8;
				best = closest(c);
				alphamap[ai][a<<8 | b] = best->x;
			}
		}
	}
	for(alpha = 128+AlphaStep; alpha < 256; alpha += 1<<AlphaStep){
		ai = alpha >> AlphaStep;
		alphamap[ai] = malloc(256*256);
		i = (256-alpha)>>AlphaStep;
		for(a = 0; a < 256; a++){
			for(b = 0; b < 256; b++)
				alphamap[ai][b<<8 | a] = alphamap[i][a<<8 | b];
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
