#include "quakedef.h"

static int		sprite_height;
static int		minindex, maxindex;
static sspan_t	*sprite_spans;

/*
=====================
D_SpriteDrawSpans
=====================
*/
void D_SpriteDrawSpans (sspan_t *pspan, byte alpha)
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

	pbase = dvars.cacheblock;

	sdivz8stepu = dvars.sdivzstepu * 8;
	tdivz8stepu = dvars.tdivzstepu * 8;
	zi8stepu = dvars.zistepu * 8;

	// we count on FP exceptions being turned off to avoid range problems
	izistep = (int)(dvars.zistepu * 0x8000 * 0x10000);

	do
	{
		pdest = dvars.viewbuffer + dvars.width * pspan->v + pspan->u;
		pz = dvars.zbuffer + dvars.width * pspan->v + pspan->u;

		count = pspan->count;

		if (count <= 0)
			goto NextSpan;

		// calculate the initial s/z, t/z, 1/z, s, and t and clamp
		du = (float)pspan->u;
		dv = (float)pspan->v;

		sdivz = dvars.sdivzorigin + dv*dvars.sdivzstepv + du*dvars.sdivzstepu;
		tdivz = dvars.tdivzorigin + dv*dvars.tdivzstepv + du*dvars.tdivzstepu;
		zi = dvars.ziorigin + dv*dvars.zistepv + du*dvars.zistepu;
		z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
		// we count on FP exceptions being turned off to avoid range problems
		izi = (int)(zi * 0x8000 * 0x10000);

		s = (int)(sdivz * z) + dvars.sadjust;
		s = clamp(s, 0, dvars.bbextents);

		t = (int)(tdivz * z) + dvars.tadjust;
		t = clamp(t, 0, dvars.bbextentt);

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

				snext = (int)(sdivz * z) + dvars.sadjust;
				snext = clamp(snext, 8, dvars.bbextents);
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + dvars.tadjust;
				tnext = clamp(tnext, 8, dvars.bbextentt); // guard against round-off error on <0 steps

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
				sdivz += dvars.sdivzstepu * spancountminus1;
				tdivz += dvars.tdivzstepu * spancountminus1;
				zi += dvars.zistepu * spancountminus1;
				z = (float)0x10000 / zi;	// prescale to 16.16 fixed-point
				snext = (int)(sdivz * z) + dvars.sadjust;
				snext = clamp(snext, 8, dvars.bbextents);
				// prevent round-off error on <0 steps from
				//  from causing overstepping & running off the
				//  edge of the texture

				tnext = (int)(tdivz * z) + dvars.tadjust;
				tnext = clamp(tnext, 8, dvars.bbextentt); // guard against round-off error on <0 steps

				if(spancount > 1){
					sstep = (snext - s) / (spancount - 1);
					tstep = (tnext - t) / (spancount - 1);
				}
			}

			do
			{
				btemp = *(pbase + (s >> 16) + (t >> 16) * dvars.cachewidth);
				if(opaque(btemp) && *pz <= izi){
					if(r_drawflags & DRAW_BLEND){
						*pdest = blendalpha(btemp, *pdest, alpha, izi);
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
void D_SpriteScanLeftEdge (void)
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
void D_SpriteScanRightEdge (void)
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
void D_SpriteCalculateGradients (void)
{
	vec3_t		p_normal, p_saxis, p_taxis, p_temp1;
	float		distinv;

	TransformVector (r_spritedesc.vpn, p_normal);
	TransformVector (r_spritedesc.vright, p_saxis);
	TransformVector (r_spritedesc.vup, p_taxis);
	VectorInverse (p_taxis);

	distinv = 1.0 / (-DotProduct (modelorg, r_spritedesc.vpn));

	dvars.sdivzstepu = p_saxis[0] * xscaleinv;
	dvars.tdivzstepu = p_taxis[0] * xscaleinv;

	dvars.sdivzstepv = -p_saxis[1] * yscaleinv;
	dvars.tdivzstepv = -p_taxis[1] * yscaleinv;

	dvars.zistepu = p_normal[0] * xscaleinv * distinv;
	dvars.zistepv = -p_normal[1] * yscaleinv * distinv;

	dvars.sdivzorigin = p_saxis[2] - xcenter*dvars.sdivzstepu - ycenter*dvars.sdivzstepv;
	dvars.tdivzorigin = p_taxis[2] - xcenter*dvars.tdivzstepu - ycenter*dvars.tdivzstepv;
	dvars.ziorigin = p_normal[2]*distinv - xcenter*dvars.zistepu - ycenter*dvars.zistepv;

	TransformVector (modelorg, p_temp1);

	dvars.sadjust = ((fixed16_t)(DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) - (-(dvars.cachewidth >> 1) << 16);
	dvars.tadjust = ((fixed16_t)(DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) - (-(sprite_height >> 1) << 16);

	// -1 (-epsilon) so we never wander off the edge of the texture
	dvars.bbextents = (dvars.cachewidth << 16) - 1;
	dvars.bbextentt = (sprite_height << 16) - 1;
}


/*
=====================
D_DrawSprite
=====================
*/
void D_DrawSprite (void)
{
	int			i, nump;
	float		ymin, ymax;
	emitpoint_t	*pverts;
	static sspan_t		spans[MAXHEIGHT+1];

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

	dvars.cachewidth = r_spritedesc.pspriteframe->width;
	sprite_height = r_spritedesc.pspriteframe->height;
	dvars.cacheblock = &r_spritedesc.pspriteframe->pixels[0];

	// copy the first vertex to the last vertex, so we don't have to deal with
	// wrapping
	nump = r_spritedesc.nump;
	pverts = r_spritedesc.pverts;
	pverts[nump] = pverts[0];

	D_SpriteCalculateGradients ();
	D_SpriteScanLeftEdge ();
	D_SpriteScanRightEdge ();
	D_SpriteDrawSpans (sprite_spans, currententity->alpha);
}
