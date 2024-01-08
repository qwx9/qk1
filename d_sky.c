#include "quakedef.h"

#define SKY_SPAN_SHIFT	5
#define SKY_SPAN_MAX	(1 << SKY_SPAN_SHIFT)

skyvars_t skyvars;

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

	end[0] = r_refdef.view.pn[0] + wu*r_refdef.view.right[0] + wv*r_refdef.view.up[0];
	end[1] = r_refdef.view.pn[1] + wu*r_refdef.view.right[1] + wv*r_refdef.view.up[1];
	end[2] = r_refdef.view.pn[2] + wu*r_refdef.view.right[2] + wv*r_refdef.view.up[2];
	end[2] *= 3;
	VectorNormalize(end);

	s[0] = (int)((skydist + 4*(skyvars.w-1)*end[0]) * 0x10000);
	t[0] = (int)((skydist + 4*(skyvars.h-1)*end[1]) * 0x10000);
	s[1] = (int)((skydist*2.0 + 4*(skyvars.w-1)*end[0]) * 0x10000);
	t[1] = (int)((skydist*2.0 + 4*(skyvars.h-1)*end[1]) * 0x10000);
}


/*
=================
D_DrawSkyScans8
=================
*/
void D_DrawSkyScans8 (espan_t *pspan)
{
	int count, spancount, u, v, spancountminus1;
	pixel_t *pdest, pix;
	uzint *pz;
	fixed16_t s[2], t[2], snext[2], tnext[2], sstep[2], tstep[2];
	float skydist;
	pixel_t fog;
	int inva;

	if(skyvars.source[0] == nil || skyvars.source[1] == nil)
		return;

	sstep[0] = sstep[1] = 0;	// keep compiler happy
	tstep[0] = tstep[1] = 0;	// ditto
	skydist = skyvars.time*skyvars.speed;	// TODO: add D_SetupFrame & set this there
	fog = fogvars.skypix;
	inva = 255 - (fog>>24);

	do
	{
		pdest = dvars.fb + pspan->v*dvars.w + pspan->u;
		count = pspan->count;
		pz = dvars.zb + pspan->v*dvars.w + pspan->u;
		memset(pz, 0xff, count*sizeof(*pz));

		// calculate the initial s & t
		u = pspan->u;
		v = pspan->v;
		D_Sky_uv_To_st (u, v, s, t, skydist);

		do
		{
			spancount = min(count, SKY_SPAN_MAX);
			count -= spancount;

			if(count){
				u += spancount;

				// calculate s and t at far end of span,
				// calculate s and t steps across span by shifting
				D_Sky_uv_To_st (u, v, snext, tnext, skydist);

				sstep[0] = (snext[0] - s[0]) >> SKY_SPAN_SHIFT;
				tstep[0] = (tnext[0] - t[0]) >> SKY_SPAN_SHIFT;
				sstep[1] = (snext[1] - s[1]) >> SKY_SPAN_SHIFT;
				tstep[1] = (tnext[1] - t[1]) >> SKY_SPAN_SHIFT;
			}else{
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

			do{
				pix = skyvars.source[1][((t[1] & skyvars.tmask) >> skyvars.tshift) + ((s[1] & skyvars.smask) >> 16)];
				if(!opaque(pix))
					pix = skyvars.source[0][((t[0] & skyvars.tmask) >> skyvars.tshift) + ((s[0] & skyvars.smask) >> 16)];
				if(fog != 0)
					pix = fog + mulalpha(pix, inva);
				*pdest++ = pix;
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

