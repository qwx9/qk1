#include "quakedef.h"
#include "r_fog.h"

int *r_turb_turb;

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
		rowptr[v] = dvars.viewbuffer + (r_refdef.vrect.y * dvars.width) + (dvars.width * (int)((float)v * hratio * h / (h + AMP2 * 2)));

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

static inline void
dospan_solid(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, uzint *pz, uzint izi, int izistep)
{
	pixel_t pix;
	do{
		*pz++ = izi;
		pix = pbase[(s >> 16) + (t >> 16) * width];
		s += sstep;
		t += tstep;
		izi += izistep;
		*pdest++ = pix;
	}while(--spancount);
}

static inline void
dospan_solid_f1(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, uzint *pz, uzint izi, int izistep, fog_t *fog)
{
	pixel_t pix;
	do{
		*pz++ = izi;
		izi += izistep;
		pix = pbase[(s >> 16) + (t >> 16) * width];
		s += sstep;
		t += tstep;
		*pdest++ = blendfog(pix, *fog);
		fogstep(*fog);
	}while(--spancount);
}

static inline void
dospan_blend(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, byte alpha, uzint *pz, uzint izi, int izistep)
{
	pixel_t pix;

	do{
		pix = pbase[(s >> 16) + (t >> 16) * width];
		s += sstep;
		t += tstep;
		if(opaque(pix) && *pz <= izi)
			*pdest = blendalpha(pix, *pdest, alpha);
		izi += izistep;
		pdest++;
		pz++;
	}while(--spancount);
}

static inline void
dospan_blend_f1(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, byte alpha, uzint *pz, uzint izi, int izistep, fog_t *fog)
{
	pixel_t pix;

	do{
		pix = pbase[(s >> 16) + (t >> 16) * width];
		s += sstep;
		t += tstep;
		if(opaque(pix) && *pz <= izi)
			*pdest = blendalpha(0xff<<24 | blendfog(pix, *fog), *pdest, alpha);
		izi += izistep;
		pdest++;
		pz++;
		fogstep(*fog);
	}while(--spancount);
}

static inline void
dospan_fence(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, uzint *pz, uzint izi, int izistep)
{
	pixel_t pix;

	do{
		pix = pbase[(s >> 16) + (t >> 16) * width];
		s += sstep;
		t += tstep;
		if(opaque(pix) && *pz <= izi){
			*pdest = pix;
			*pz = izi;
		}
		izi += izistep;
		pdest++;
		pz++;
	}while(--spancount);
}

static inline void
dospan_fence_f1(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, int width, uzint *pz, uzint izi, int izistep, fog_t *fog)
{
	pixel_t pix;

	do{
		pix = pbase[(s >> 16) + (t >> 16) * width];
		s += sstep;
		t += tstep;
		if(opaque(pix) && *pz <= izi){
			*pdest = blendfog(pix, *fog);
			*pz = izi;
		}
		izi += izistep;
		pdest++;
		pz++;
		fogstep(*fog);
	}while(--spancount);
}

static void
dospan_turb(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, byte alpha, uzint *pz, uzint izi, int izistep)
{
	int sturb, tturb;
	bool noblend;

	noblend = (r_drawflags & DRAW_BLEND) == 0;
	s &= (CYCLE<<16)-1;
	t &= (CYCLE<<16)-1;

	do{
		if(noblend || *pz <= izi){
			sturb = ((s + r_turb_turb[(t>>16)&(CYCLE-1)])>>16)&63;
			tturb = ((t + r_turb_turb[(s>>16)&(CYCLE-1)])>>16)&63;
			*pdest = blendalpha(*(pbase + (tturb<<6) + sturb), *pdest, alpha);
			if(noblend)
				*pz = izi;
		}
		s += sstep;
		t += tstep;
		izi += izistep;
		pdest++;
		pz++;
	}while(--spancount > 0);

}

static void
dospan_turb_f1(pixel_t *pdest, pixel_t *pbase, int s, int t, int sstep, int tstep, int spancount, byte alpha, uzint *pz, uzint izi, int izistep, fog_t *fog)
{
	int sturb, tturb;
	bool noblend;

	noblend = (r_drawflags & DRAW_BLEND) == 0;
	s &= (CYCLE<<16)-1;
	t &= (CYCLE<<16)-1;

	do{
		if(noblend || *pz <= izi){
			sturb = ((s + r_turb_turb[(t>>16)&(CYCLE-1)])>>16)&63;
			tturb = ((t + r_turb_turb[(s>>16)&(CYCLE-1)])>>16)&63;
			*pdest = blendalpha(0xff<<24 | blendfog(*(pbase + (tturb<<6) + sturb), *fog), *pdest, alpha);
			if(noblend)
				*pz = izi;
		}
		s += sstep;
		t += tstep;
		izi += izistep;
		pdest++;
		pz++;
		fogstep(*fog);
	}while(--spancount > 0);

}

void
D_DrawSpans16(espan_t *pspan, pixel_t *pbase, int width, byte alpha, int spanfunc)
{
	int			count, spancount, izistep, spancountminus1;
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

	sdivzstepu = dvars.sdivzstepu * 16;
	tdivzstepu = dvars.tdivzstepu * 16;
	zistepu = dvars.zistepu * 16;
	izistep = (int)(dvars.zistepu * 0x8000 * 0x10000);

	fogenabled = isfogged();

	do{
		pdest = dvars.viewbuffer + pspan->v*dvars.width + pspan->u;
		pz = dvars.zbuffer + pspan->v*dvars.width + pspan->u;
		zi = dvars.ziorigin + pspan->v*dvars.zistepv + pspan->u*dvars.zistepu;
		izi = zi * 0x8000 * 0x10000;
		count = pspan->count;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = pspan->u;
		dv = pspan->v;

		sdivz = dvars.sdivzorigin + dv*dvars.sdivzstepv + du*dvars.sdivzstepu;
		tdivz = dvars.tdivzorigin + dv*dvars.tdivzstepv + du*dvars.tdivzstepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

		s = (int)(sdivz * z) + dvars.sadjust;
		s = clamp(s, 0, dvars.bbextents);

		t = (int)(tdivz * z) + dvars.tadjust;
		t = clamp(t, 0, dvars.bbextentt);

		do{
			// calculate s and t at the far end of the span
			spancount = min(count, 16);
			count -= spancount;
			fogged = fogenabled && fogcalc(izi, izi + izistep*spancount, spancount, &fog);

			if(count){
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivzstepu;
				tdivz += tdivzstepu;
				// prescale to 16.16 fixed-point
				z = (float)0x10000 / (zi + zistepu);

				snext = (int)(sdivz * z) + dvars.sadjust;
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture
				snext = clamp(snext, 16, dvars.bbextents);

				tnext = (int)(tdivz * z) + dvars.tadjust;
				// guard against round-off error on <0 steps
				tnext = clamp(tnext, 16, dvars.bbextentt);

				sstep = (snext - s) >> 4;
				tstep = (tnext - t) >> 4;
			}else{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = spancount - 1;
				sdivz += dvars.sdivzstepu * spancountminus1;
				tdivz += dvars.tdivzstepu * spancountminus1;
				// prescale to 16.16 fixed-point
				z = (float)0x10000 / (zi + dvars.zistepu * spancountminus1);
				snext = (int)(sdivz * z) + dvars.sadjust;
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture
				snext = clamp(snext, 16, dvars.bbextents);

				tnext = (int)(tdivz * z) + dvars.tadjust;
				// guard against round-off error on <0 steps
				tnext = clamp(tnext, 16, dvars.bbextentt);

				if(spancount > 1){
					sstep = (snext - s) / spancountminus1;
					tstep = (tnext - t) / spancountminus1;
				}
			}

			if(fogged){
				switch(spanfunc){
				case SPAN_SOLID:
					dospan_solid_f1(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep, &fog);
					break;
				case SPAN_TURB:
					dospan_turb_f1(pdest, pbase, s, t, sstep, tstep, spancount, alpha, pz, izi, izistep, &fog);
					break;
				case SPAN_BLEND:
					dospan_blend_f1(pdest, pbase, s, t, sstep, tstep, spancount, width, alpha, pz, izi, izistep, &fog);
					break;
				case SPAN_FENCE:
					dospan_fence_f1(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep, &fog);
					break;
				}
			}else{
				switch(spanfunc){
				case SPAN_SOLID:
					dospan_solid(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep);
					break;
				case SPAN_TURB:
					dospan_turb(pdest, pbase, s, t, sstep, tstep, spancount, alpha, pz, izi, izistep);
					break;
				case SPAN_BLEND:
					dospan_blend(pdest, pbase, s, t, sstep, tstep, spancount, width, alpha, pz, izi, izistep);
					break;
				case SPAN_FENCE:
					dospan_fence(pdest, pbase, s, t, sstep, tstep, spancount, width, pz, izi, izistep);
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
