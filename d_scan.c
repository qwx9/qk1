#include "quakedef.h"

static pixel_t *r_turb_pbase, *r_turb_pdest;
static fixed16_t r_turb_s, r_turb_t, r_turb_sstep, r_turb_tstep;
static int *r_turb_turb;
static int r_turb_spancount;
static uzint *r_turb_z;

/*
=============
D_WarpScreen

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/
void D_WarpScreen (void)
{
	int		w, h, u, v, *turb, *col;
	pixel_t	*dest;
	pixel_t	**row;
	float	wratio, hratio;
	static pixel_t	*rowptr[MAXHEIGHT+(AMP2*2)];
	static int	column[MAXWIDTH+(AMP2*2)];

	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;

	wratio = w / (float)scr_vrect.width;
	hratio = h / (float)scr_vrect.height;

	for(v = 0; v < scr_vrect.height+AMP2*2; v++)
		rowptr[v] = d_viewbuffer + (r_refdef.vrect.y * screenwidth) + (screenwidth * (int)((float)v * hratio * h / (h + AMP2 * 2)));

	for(u = 0; u < scr_vrect.width+AMP2*2; u++)
		column[u] = r_refdef.vrect.x + (int)((float)u * wratio * w / (w + AMP2 * 2));

	turb = intsintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	dest = vid.buffer + scr_vrect.y * vid.rowbytes + scr_vrect.x;

	for(v = 0; v < scr_vrect.height; v++, dest += vid.rowbytes){
		col = &column[turb[v]];
		row = &rowptr[v];

		for(u = 0; u < scr_vrect.width; u += 4){
			dest[u+0] = row[turb[u+0]][col[u+0]];
			dest[u+1] = row[turb[u+1]][col[u+1]];
			dest[u+2] = row[turb[u+2]][col[u+2]];
			dest[u+3] = row[turb[u+3]][col[u+3]];
		}
	}
}

/*
=============
D_DrawTurbulent8Span
=============
*/
void D_DrawTurbulent8Span (int izi, byte alpha)
{
	int sturb, tturb;

	do{
		sturb = ((r_turb_s + r_turb_turb[(r_turb_t>>16)&(CYCLE-1)])>>16)&63;
		tturb = ((r_turb_t + r_turb_turb[(r_turb_s>>16)&(CYCLE-1)])>>16)&63;
		if(*r_turb_z <= izi || (r_drawflags & DRAW_BLEND) == 0)
			*r_turb_pdest = blendalpha(*(r_turb_pbase + (tturb<<6) + sturb), *r_turb_pdest, alpha);
		r_turb_s += r_turb_sstep;
		r_turb_t += r_turb_tstep;
		r_turb_pdest++;
		r_turb_z++;
	}while(--r_turb_spancount > 0);
}


void
Turbulent8(espan_t *pspan, byte alpha)
{
	int			count;
	fixed16_t	snext, tnext;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivz16stepu, tdivz16stepu, zi16stepu;

	r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));

	r_turb_sstep = 0;	// keep compiler happy
	r_turb_tstep = 0;	// ditto

	r_turb_pbase = cacheblock;

	sdivz16stepu = d_sdivzstepu * 16;
	tdivz16stepu = d_tdivzstepu * 16;
	zi16stepu = d_zistepu * 16;

	do{
		r_turb_pdest = d_viewbuffer + screenwidth*pspan->v + pspan->u;
		r_turb_z = d_pzbuffer + d_zwidth*pspan->v + pspan->u;

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = pspan->u;
		dv = pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		r_turb_s = (int)(sdivz * z) + sadjust;
		r_turb_s = clamp(r_turb_s, 0, bbextents);

		r_turb_t = (int)(tdivz * z) + tadjust;
		r_turb_t = clamp(r_turb_t, 0, bbextentt);

		do{
			// calculate s and t at the far end of the span
			r_turb_spancount = min(count, 16);
			count -= r_turb_spancount;

			if(count){
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz16stepu;
				tdivz += tdivz16stepu;
				zi += zi16stepu;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				// prevent round-off error on <0 steps from causing overstepping & running off the edge of the texture
				snext = clamp(snext, 16, bbextents);

				tnext = (int)(tdivz * z) + tadjust;
				tnext = clamp(tnext, 16, bbextentt); // guard against round-off error on <0 steps

				r_turb_sstep = (snext - r_turb_s) >> 4;
				r_turb_tstep = (tnext - r_turb_t) >> 4;
			}else{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = r_turb_spancount - 1;
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				zi += d_zistepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				// prevent round-off error on <0 steps from causing overstepping & running off the edge of the texture
				snext = clamp(snext, 16, bbextents);

				tnext = (int)(tdivz * z) + tadjust;
				tnext = clamp(tnext, 16, bbextentt); // guard against round-off error on <0 steps

				if(r_turb_spancount > 1){
					r_turb_sstep = (snext - r_turb_s) / (r_turb_spancount - 1);
					r_turb_tstep = (tnext - r_turb_t) / (r_turb_spancount - 1);
				}
			}

			r_turb_s = r_turb_s & ((CYCLE<<16)-1);
			r_turb_t = r_turb_t & ((CYCLE<<16)-1);

			D_DrawTurbulent8Span((int)(zi * 0x8000 * 0x10000), alpha);

			r_turb_s = snext;
			r_turb_t = tnext;

		}while(count > 0);

	}while((pspan = pspan->pnext) != nil);
}

void
D_DrawSpans16(espan_t *pspan, int forceblend, byte alpha) //qbism- up it from 8 to 16
{
	int			count, spancount, izistep;
	pixel_t		*pbase, *pdest;
	uzint		*pz;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivzstepu, tdivzstepu, zistepu;

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = cacheblock;

	sdivzstepu = d_sdivzstepu * 16;
	tdivzstepu = d_tdivzstepu * 16;
	zistepu = d_zistepu * 16;
	izistep = (int)(d_zistepu * 0x8000 * 0x10000);

	do{
		pdest = d_viewbuffer + screenwidth*pspan->v + pspan->u;
		pz = d_pzbuffer + d_zwidth*pspan->v + pspan->u;

		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = pspan->u;
		dv = pspan->v;

		sdivz = d_sdivzorigin + dv*d_sdivzstepv + du*d_sdivzstepu;
		tdivz = d_tdivzorigin + dv*d_tdivzstepv + du*d_tdivzstepu;
		zi = d_ziorigin + dv*d_zistepv + du*d_zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + sadjust;
		s = clamp(s, 0, bbextents);

		t = (int)(tdivz * z) + tadjust;
		t = clamp(t, 0, bbextentt);

		do{
			// calculate s and t at the far end of the span
			spancount = min(count, 16);
			spancountminus1 = spancount - 1;
			count -= spancount;

			if(count){
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivzstepu;
				tdivz += tdivzstepu;
				z = (float)0x10000 / (zi + zistepu);	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + sadjust;
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture
				snext = clamp(snext, 16, bbextents);

				tnext = (int)(tdivz * z) + tadjust;
				// guard against round-off error on <0 steps
				tnext = clamp(tnext, 16, bbextentt);

				sstep = (snext - s) >> 4;
				tstep = (tnext - t) >> 4;
			}else{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				sdivz += d_sdivzstepu * spancountminus1;
				tdivz += d_tdivzstepu * spancountminus1;
				z = (float)0x10000 / (zi + d_zistepu * spancountminus1);	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + sadjust;
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture
				snext = clamp(snext, 16, bbextents);

				tnext = (int)(tdivz * z) + tadjust;
				// guard against round-off error on <0 steps
				tnext = clamp(tnext, 16, bbextentt);

				if(spancount > 1){
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			if(spancount > 0){
				void dospan(pixel_t *, pixel_t *, int, int, int, int, int, int);
				void dospan_alpha(pixel_t *, pixel_t *, int, int, int, int, int, int, byte, uzint *, int, int);
				if((r_drawflags & DRAW_BLEND) != 0 || forceblend)
					dospan_alpha(pdest, pbase, s, t, sstep, tstep, spancount, cachewidth, alpha, pz, (int)(zi * 0x8000 * 0x10000), izistep);
				else
					dospan(pdest, pbase, s, t, sstep, tstep, spancount, cachewidth);
				pdest += spancount;
				pz += spancount;
			}

			zi += count ? zistepu : d_zistepu*spancountminus1;
			s = snext;
			t = tnext;
		}while(count > 0);
	}while((pspan = pspan->pnext) != nil);
}

void
D_DrawZSpans(espan_t *pspan)
{
	int			count, spancount, spancountminus1, izi, izistep;
	uzint		*pz;
	float		zi;
	float		zistepu;

	if((r_drawflags & DRAW_BLEND) != 0)
		return;

	zistepu = d_zistepu * 16;
	izistep = d_zistepu * 0x8000 * 0x10000;

	do{
		pz = d_pzbuffer + d_zwidth*pspan->v + pspan->u;
		zi = d_ziorigin + pspan->v*d_zistepv + pspan->u*d_zistepu;
		count = pspan->count;

		do{
			spancount = min(count, 16);
			spancountminus1 = spancount - 1;
			count -= spancount;

			izi = (int)(zi * 0x8000 * 0x10000);
			while(spancount-- > 0){
				*pz++ = izi;
				izi += izistep;
			}
			zi += count ? zistepu : d_zistepu*spancountminus1;
		}while(count > 0);
	}while((pspan = pspan->pnext) != nil);
}
