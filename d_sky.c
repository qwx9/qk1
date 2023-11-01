#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

#define SKY_SPAN_SHIFT	5
#define SKY_SPAN_MAX	(1 << SKY_SPAN_SHIFT)

extern int skyw, skyh;

/*
=================
D_Sky_uv_To_st
=================
*/
void D_Sky_uv_To_st (int u, int v, fixed16_t *s, fixed16_t *t, float skydist)
{
	double	wu, wv;
	vec3_t	end;

	wu = (u - xcenter)/xscale;
	wv = (ycenter - v)/yscale;

	end[0] = vpn[0] + wu*vright[0] + wv*vup[0];
	end[1] = vpn[1] + wu*vright[1] + wv*vup[1];
	end[2] = vpn[2] + wu*vright[2] + wv*vup[2];
	end[2] *= 3;
	VectorNormalize(end);

	s[0] = (int)((skydist + 4*(skyw-1)*end[0]) * 0x10000);
	t[0] = (int)((skydist + 4*(skyh-1)*end[1]) * 0x10000);
	s[1] = (int)((skydist*2.0 + 4*(skyw-1)*end[0]) * 0x10000);
	t[1] = (int)((skydist*2.0 + 4*(skyh-1)*end[1]) * 0x10000);
}


/*
=================
D_DrawSkyScans8
=================
*/
void D_DrawSkyScans8 (espan_t *pspan)
{
	int				count, spancount, u, v;
	unsigned char	*pdest, m;
	fixed16_t		s[2], t[2], snext[2], tnext[2], sstep[2], tstep[2];
	int				spancountminus1;
	float			skydist;

	sstep[0] = sstep[1] = 0;	// keep compiler happy
	tstep[0] = tstep[1] = 0;	// ditto
	skydist = skytime*skyspeed;	// TODO: add D_SetupFrame & set this there

	do
	{
		pdest = (unsigned char *)((byte *)d_viewbuffer +
				(screenwidth * pspan->v) + pspan->u);

		count = pspan->count;

	// calculate the initial s & t
		u = pspan->u;
		v = pspan->v;
		D_Sky_uv_To_st (u, v, s, t, skydist);

		do
		{
			if (count >= SKY_SPAN_MAX)
				spancount = SKY_SPAN_MAX;
			else
				spancount = count;

			count -= spancount;

			if (count)
			{
				u += spancount;

			// calculate s and t at far end of span,
			// calculate s and t steps across span by shifting
				D_Sky_uv_To_st (u, v, snext, tnext, skydist);

				sstep[0] = (snext[0] - s[0]) >> SKY_SPAN_SHIFT;
				tstep[0] = (tnext[0] - t[0]) >> SKY_SPAN_SHIFT;
				sstep[1] = (snext[1] - s[1]) >> SKY_SPAN_SHIFT;
				tstep[1] = (tnext[1] - t[1]) >> SKY_SPAN_SHIFT;
			}
			else
			{
			// calculate s and t at last pixel in span,
			// calculate s and t steps across span by division
				spancountminus1 = (float)(spancount - 1);

				if (spancountminus1 > 0)
				{
					u += spancountminus1;
					D_Sky_uv_To_st (u, v, snext, tnext, skydist);

					sstep[0] = (snext[0] - s[0]) / spancountminus1;
					tstep[0] = (tnext[0] - t[0]) / spancountminus1;
					sstep[1] = (snext[1] - s[1]) / spancountminus1;
					tstep[1] = (tnext[1] - t[1]) / spancountminus1;
				}
			}

			do
			{
				m = r_skysource[1][((t[1] & R_SKY_TMASK) >> 9) +
						((s[1] & R_SKY_SMASK) >> 16)];
				if(m == 0)
					*pdest = r_skysource[0][((t[0] & R_SKY_TMASK) >> 9) +
							((s[0] & R_SKY_SMASK) >> 16)];
				else
					*pdest = m;
				pdest++;
				s[0] += sstep[0];
				t[0] += tstep[0];
				s[1] += sstep[1];
				t[1] += tstep[1];
			} while (--spancount > 0);

			s[0] = snext[0];
			t[0] = tnext[0];
			s[1] = snext[1];
			t[1] = tnext[1];
		} while (count > 0);

	} while ((pspan = pspan->pnext) != nil);
}

