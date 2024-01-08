#include "quakedef.h"

static int		sprite_height;
static int		minindex, maxindex;
static sspan_t	*sprite_spans;

/*
=====================
D_SpriteDrawSpans
=====================
*/
static void
D_SpriteDrawSpans(sspan_t *pspan, texvars_t *tv, byte alpha)
{
	int			count, spancount, izistep;
	int			izi;
	pixel_t		*pbase, *pdest, btemp;
	fixed16_t	s, t, snext, tnext, sstep, tstep;
	float		sdivz, tdivz, zi, z, du, dv, spancountminus1;
	float		sdivz8stepu, tdivz8stepu, zi8stepu;
	uzint		*pz;

	sstep = 0;	// keep compiler happy
	tstep = 0;	// ditto

	pbase = tv->p;

	sdivz8stepu = tv->s.divz.stepu * 8;
	tdivz8stepu = tv->t.divz.stepu * 8;
	zi8stepu = tv->z.stepu * 8;

	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(tv->z.stepu * 0x8000 * 0x10000);

	assert(pspan->v >= 0);

	do
	{
		pdest = dvars.fb + dvars.w * pspan->v + pspan->u;
		pz = dvars.zb + dvars.w * pspan->v + pspan->u;

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = tv->s.divz.origin + dv*tv->s.divz.stepv + du*tv->s.divz.stepu;
		tdivz = tv->t.divz.origin + dv*tv->t.divz.stepv + du*tv->t.divz.stepu;
		zi = tv->z.origin + dv*tv->z.stepv + du*tv->z.stepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
		// we count on FP exceptions being turned off to avoid range problems
		izi = (int)(zi * 0x8000 * 0x10000);

		s = (int)(sdivz * z) + tv->s.adjust;
		s = clamp(s, 0, tv->s.bbextent);

		t = (int)(tdivz * z) + tv->t.adjust;
		t = clamp(t, 0, tv->t.bbextent);

		do
		{
			// calculate s and t at the far end of the span
			spancount = min(count, 8);
			count -= spancount;

			if(count){
				// calculate s/z, t/z, zi->fixed s and t at far end of span,
				// calculate s and t steps across span by shifting
				sdivz += sdivz8stepu;
				tdivz += tdivz8stepu;
				zi += zi8stepu;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point

				snext = (int)(sdivz * z) + tv->s.adjust;
				snext = clamp(snext, 8, tv->s.bbextent);
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + tv->t.adjust;
				tnext = clamp(tnext, 8, tv->t.bbextent); // guard against round-off error on <0 steps

				sstep = (snext - s) >> 3;
				tstep = (tnext - t) >> 3;
			}
			else
			{
				// calculate s/z, t/z, zi->fixed s and t at last pixel in span (so
				// can't step off polygon), clamp, calculate s and t steps across
				// span by division, biasing steps low so we don't run off the
				// texture
				spancountminus1 = (float)(spancount - 1);
				sdivz += tv->s.divz.stepu * spancountminus1;
				tdivz += tv->t.divz.stepu * spancountminus1;
				zi += tv->z.stepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + tv->s.adjust;
				snext = clamp(snext, 8, tv->s.bbextent);
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + tv->t.adjust;
				tnext = clamp(tnext, 8, tv->t.bbextent); // guard against round-off error on <0 steps

				if(spancount > 1){
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				btemp = *(pbase + (s >> 16) + (t >> 16) * tv->w);
				if(opaque(btemp) && *pz <= izi){
					if(r_drawflags & DRAW_BLEND){
						*pdest = blendalpha(btemp, *pdest, alpha);
					}else{
						*pz = izi;
						*pdest = btemp;
					}
				}

				izi += izistep;
				pdest++;
				pz++;
				s += sstep;
				t += tstep;
			} while (--spancount > 0);

			s = snext;
			t = tnext;

		} while (count > 0);

NextSpan:
		pspan++;

	} while (pspan->count != DS_SPAN_LIST_END);
}


/*
=====================
D_SpriteScanLeftEdge
=====================
*/
static void
D_SpriteScanLeftEdge(void)
{
	int			i, v, itop, ibottom, lmaxindex;
	emitpoint_t	*pvert, *pnext;
	sspan_t		*pspan;
	float		du, dv, vtop, vbottom, slope;
	fixed16_t	u, u_step;

	pspan = sprite_spans;
	i = minindex;
	if (i == 0)
		i = r_spritedesc.nump;

	lmaxindex = maxindex;
	if (lmaxindex == 0)
		lmaxindex = r_spritedesc.nump;

	vtop = ceil (r_spritedesc.pverts[i].v);

	do
	{
		pvert = &r_spritedesc.pverts[i];
		pnext = pvert - 1;

		vbottom = ceil (pnext->v);

		if (vtop < vbottom)
		{
			du = pnext->u - pvert->u;
			dv = pnext->v - pvert->v;
			slope = du / dv;
			u_step = (int)(slope * 0x10000);
			// adjust u to ceil the integer portion
			u = (int)((pvert->u + (slope * (vtop - pvert->v))) * 0x10000) + (0x10000 - 1);
			itop = (int)vtop;
			ibottom = (int)vbottom;

			for (v=itop ; v<ibottom ; v++)
			{
				pspan->u = u >> 16;
				pspan->v = v;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;

		i--;
		if (i == 0)
			i = r_spritedesc.nump;

	} while (i != lmaxindex);
}


/*
=====================
D_SpriteScanRightEdge
=====================
*/
static void
D_SpriteScanRightEdge(void)
{
	int			i, v, itop, ibottom;
	emitpoint_t	*pvert, *pnext;
	sspan_t		*pspan;
	float		du, dv, vtop, vbottom, slope, uvert, unext, vvert, vnext;
	fixed16_t	u, u_step;

	pspan = sprite_spans;
	i = minindex;

	vvert = r_spritedesc.pverts[i].v;
	if (vvert < r_refdef.fvrecty_adj)
		vvert = r_refdef.fvrecty_adj;
	if (vvert > r_refdef.fvrectbottom_adj)
		vvert = r_refdef.fvrectbottom_adj;

	vtop = ceil (vvert);

	do
	{
		pvert = &r_spritedesc.pverts[i];
		pnext = pvert + 1;

		vnext = pnext->v;
		if (vnext < r_refdef.fvrecty_adj)
			vnext = r_refdef.fvrecty_adj;
		if (vnext > r_refdef.fvrectbottom_adj)
			vnext = r_refdef.fvrectbottom_adj;

		vbottom = ceil (vnext);

		if (vtop < vbottom)
		{
			uvert = pvert->u;
			if (uvert < r_refdef.fvrectx_adj)
				uvert = r_refdef.fvrectx_adj;
			if (uvert > r_refdef.fvrectright_adj)
				uvert = r_refdef.fvrectright_adj;

			unext = pnext->u;
			if (unext < r_refdef.fvrectx_adj)
				unext = r_refdef.fvrectx_adj;
			if (unext > r_refdef.fvrectright_adj)
				unext = r_refdef.fvrectright_adj;

			du = unext - uvert;
			dv = vnext - vvert;
			slope = du / dv;
			u_step = (int)(slope * 0x10000);
			// adjust u to ceil the integer portion
			u = (int)((uvert + (slope * (vtop - vvert))) * 0x10000) + (0x10000 - 1);
			itop = (int)vtop;
			ibottom = (int)vbottom;

			for (v=itop ; v<ibottom ; v++)
			{
				pspan->count = (u >> 16) - pspan->u;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;
		vvert = vnext;

		i++;
		if (i == r_spritedesc.nump)
			i = 0;

	} while (i != maxindex);

	pspan->count = DS_SPAN_LIST_END;	// mark the end of the span list
}


/*
=====================
D_SpriteCalculateGradients
=====================
*/
static void
D_SpriteCalculateGradients(view_t *v, texvars_t *tv)
{
	vec3_t		p_normal, p_saxis, p_taxis, p_temp1;
	float		distinv;

	TransformVector (r_spritedesc.vpn, p_normal, v);
	TransformVector (r_spritedesc.vright, p_saxis, v);
	TransformVector (r_spritedesc.vup, p_taxis, v);
	VectorInverse (p_taxis);

	distinv = 1.0 / (-DotProduct (v->modelorg, r_spritedesc.vpn));

	tv->s.divz.stepu = p_saxis[0] * xscaleinv;
	tv->t.divz.stepu = p_taxis[0] * xscaleinv;

	tv->s.divz.stepv = -p_saxis[1] * yscaleinv;
	tv->t.divz.stepv = -p_taxis[1] * yscaleinv;

	tv->z.stepu = p_normal[0] * xscaleinv * distinv;
	tv->z.stepv = -p_normal[1] * yscaleinv * distinv;

	tv->s.divz.origin = p_saxis[2] - xcenter*tv->s.divz.stepu - ycenter*tv->s.divz.stepv;
	tv->t.divz.origin = p_taxis[2] - xcenter*tv->t.divz.stepu - ycenter*tv->t.divz.stepv;
	tv->z.origin = p_normal[2]*distinv - xcenter*tv->z.stepu - ycenter*tv->z.stepv;

	TransformVector (v->modelorg, p_temp1, v);

	tv->s.adjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) - (-(tv->w >> 1) << 16);
	tv->t.adjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) - (-(sprite_height >> 1) << 16);

	// -1 (-epsilon) so we never wander off the edge of the texture
	tv->s.bbextent = (tv->w << 16) - 1;
	tv->t.bbextent = (sprite_height << 16) - 1;
}


/*
=====================
D_DrawSprite
=====================
*/
void
D_DrawSprite(view_t *v)
{
	int			i, nump;
	float		ymin, ymax;
	emitpoint_t	*pverts;
	static sspan_t		spans[MAXHEIGHT+1];
	texvars_t tv;

	sprite_spans = spans;

	// find the top and bottom vertices, and make sure there's at least one scan to
	// draw
	ymin = Q_MAXFLOAT;
	ymax = -Q_MAXFLOAT;
	pverts = r_spritedesc.pverts;

	for (i=0 ; i<r_spritedesc.nump ; i++)
	{
		if (pverts->v < ymin)
		{
			ymin = pverts->v;
			minindex = i;
		}

		if (pverts->v > ymax)
		{
			ymax = pverts->v;
			maxindex = i;
		}

		pverts++;
	}

	ymin = ceil (ymin);
	ymax = ceil (ymax);

	if (ymin >= ymax)
		return;		// doesn't cross any scans at all

	tv.w = r_spritedesc.pspriteframe->width;
	tv.p = r_spritedesc.pspriteframe->pixels;
	sprite_height = r_spritedesc.pspriteframe->height;

	// copy the first vertex to the last vertex, so we don't have to deal with
	// wrapping
	nump = r_spritedesc.nump;
	pverts = r_spritedesc.pverts;
	pverts[nump] = pverts[0];

	D_SpriteCalculateGradients(v, &tv);
	D_SpriteScanLeftEdge();
	D_SpriteScanRightEdge();
	D_SpriteDrawSpans(sprite_spans, &tv, currententity->alpha);
}
