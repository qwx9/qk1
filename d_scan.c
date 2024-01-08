#include "quakedef.h"
#include "r_fog.h"

int *r_turb_turb;

#include "d_scan.h"

/*
=============
D_WarpScreen

// this performs a slight compression of the screen at the same time as
// the sine warp, to keep the edges from wrapping
=============
*/
void D_WarpScreen (void)
{
	static pixel_t *rowptr[MAXHEIGHT+(AMP2*2)];
	static int column[MAXWIDTH+(AMP2*2)];
	int w, h, u, v, *turb, *col;
	pixel_t	*dest, **row;
	float wratio, hratio;

	w = r_refdef.vrect.width;
	h = r_refdef.vrect.height;

	wratio = w / (float)scr_vrect.width;
	hratio = h / (float)scr_vrect.height;

	for(v = 0; v < scr_vrect.height+AMP2*2; v++)
		rowptr[v] = dvars.fb + (r_refdef.vrect.y * dvars.w) + (dvars.w * (int)((float)v * hratio * h / (h + AMP2 * 2)));

	for(u = 0; u < scr_vrect.width+AMP2*2; u++)
		column[u] = r_refdef.vrect.x + (int)((float)u * wratio * w / (w + AMP2 * 2));

	turb = intsintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	dest = vid.buffer + scr_vrect.y * vid.width + scr_vrect.x;

	for(v = 0; v < scr_vrect.height; v++, dest += vid.width){
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

static int
D_DrawSpanGetMax(float u, float v)
{
	int s, m;
	float us, vs;

	s = 4;
	us = u * (1<<16);
	vs = v * (1<<16);
	do{
		m = 1 << (s+1);
		if((int)(us*m) != 0 || (int)(vs*m) != 0)
			return s;
		s++;
	}while(m < (int)dvars.w);

	return s;
}

void
D_DrawSpans(espan_t *pspan, texvars_t *tv, byte alpha, int spanfunc)
{
	int			count, spancount, izistep, spancountminus1, spanshift, spanmax;
	pixel_t		*pdest;
	uzint		*pz, izi;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, zi, z, du, dv;
	float		sdivzstepu, tdivzstepu, zistepu;
	fog_t fog;
	bool fogged, fogenabled;

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto
	memset(&fog, 0, sizeof(fog));

	spanshift = D_DrawSpanGetMax(tv->z.stepu, tv->z.stepv);
	spanmax = 1 << spanshift;

	sdivzstepu = tv->s.divz.stepu * spanmax;
	tdivzstepu = tv->t.divz.stepu * spanmax;
	zistepu = tv->z.stepu * spanmax;
	izistep = (int)(tv->z.stepu * 0x8000 * 0x10000);

	fogenabled = isfogged();

	do{
		pdest = dvars.fb + pspan->v*dvars.w + pspan->u;
		pz = dvars.zb + pspan->v*dvars.w + pspan->u;
		zi = tv->z.origin + pspan->v*tv->z.stepv + pspan->u*tv->z.stepu;
		izi = zi * 0x8000 * 0x10000;
		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = pspan->u;
		dv = pspan->v;

		sdivz = tv->s.divz.origin + dv*tv->s.divz.stepv + du*tv->s.divz.stepu;
		tdivz = tv->t.divz.origin + dv*tv->t.divz.stepv + du*tv->t.divz.stepu;
		z = (float)(1<<16) / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + tv->s.adjust;
		s = clamp(s, 0, tv->s.bbextent);

		t = (int)(tdivz * z) + tv->t.adjust;
		t = clamp(t, 0, tv->t.bbextent);

		do{
			// calculate s and t at the far end of the span
			spancount = min(count, spanmax);
			count -= spancount;
			fogged = fogenabled && fogcalc(izi, izi + izistep*spancount, spancount, &fog);

			if(count){
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivzstepu;
				tdivz += tdivzstepu;
				// prescale to 16.16 fixed-point
				z = (float)(1<<16) / (zi + zistepu);

				snext = (int)(sdivz * z) + tv->s.adjust;
				// prevent round-off error on <0 steps from
				//  causing overstepping & running off the
				//  edge of the texture
				snext = clamp(snext, spanmax, tv->s.bbextent);

				tnext = (int)(tdivz * z) + tv->t.adjust;
				// guard against round-off error on <0 steps
				tnext = clamp(tnext, spanmax, tv->t.bbextent);

				sstep = (snext - s) >> spanshift;
				tstep = (tnext - t) >> spanshift;
			}else{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = spancount - 1;
				sdivz += tv->s.divz.stepu * spancountminus1;
				tdivz += tv->t.divz.stepu * spancountminus1;
				// prescale to 16.16 fixed-point
				z = (float)(1<<16) / (zi + tv->z.stepu * spancountminus1);
				snext = (int)(sdivz * z) + tv->s.adjust;
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture
				snext = clamp(snext, spanmax, tv->s.bbextent);

				tnext = (int)(tdivz * z) + tv->t.adjust;
				// guard against round-off error on <0 steps
				tnext = clamp(tnext, spanmax, tv->t.bbextent);

				if(spancount > 1){
					sstep = (snext - s) / spancountminus1;
					tstep = (tnext - t) / spancountminus1;
				}
			}

			pixel_t *pdest_ = pdest, *pbase = tv->p;
			uzint *pz_ = pz;
			fog_t *fog_ = &fog;
			int spancount_ = spancount, izi_ = izi;
			if(fogged){
				switch(spanfunc){
				case SPAN_SOLID:
					dospan_solid_f1(pdest_, pbase, s, t, sstep, tstep, spancount_, tv->w, pz_, izi_, izistep, fog_);
					break;
				case SPAN_TURB:
					dospan_turb_f1(pdest_, pbase, s, t, sstep, tstep, spancount_, alpha, pz_, izi_, izistep, fog_);
					break;
				case SPAN_BLEND:
					dospan_blend_f1(pdest_, pbase, s, t, sstep, tstep, spancount_, tv->w, alpha, pz_, izi_, izistep, fog_);
					break;
				case SPAN_FENCE:
					dospan_fence_f1(pdest_, pbase, s, t, sstep, tstep, spancount_, tv->w, pz_, izi_, izistep, fog_);
					break;
				}
			}else{
				switch(spanfunc){
				case SPAN_SOLID:
					dospan_solid(pdest_, pbase, s, t, sstep, tstep, spancount_, tv->w, pz_, izi_, izistep);
					break;
				case SPAN_TURB:
					dospan_turb(pdest_, pbase, s, t, sstep, tstep, spancount_, alpha, pz_, izi_, izistep);
					break;
				case SPAN_BLEND:
					dospan_blend(pdest_, pbase, s, t, sstep, tstep, spancount_, tv->w, alpha, pz_, izi_, izistep);
					break;
				case SPAN_FENCE:
					dospan_fence(pdest_, pbase, s, t, sstep, tstep, spancount_, tv->w, pz_, izi_, izistep);
					break;
				}
			}

			pdest += spancount;
			pz += spancount;
			izi += izistep*spancount;
			zi += zistepu;
			s = snext;
			t = tnext;
		}while(count > 0);
	}while((pspan = pspan->pnext) != nil);
}
