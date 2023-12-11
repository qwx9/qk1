#include "quakedef.h"

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	pixel_t *pdest, *ptex;
	uzint *pz;
	int count, sfrac, tfrac, zi, light[3];
} spanpackage_t;

typedef struct {
	finalvert_t *pleftedgevert0;
	finalvert_t *pleftedgevert1;
	finalvert_t *pleftedgevert2;
	finalvert_t *prightedgevert0;
	finalvert_t *prightedgevert1;
	finalvert_t *prightedgevert2;
	int isflattop;
	int numleftedges;
	int numrightedges;
} edgetable;

// TODO: put in span spilling to shrink list size
// !!! if this is changed, it must be changed in d_polysa.s too !!!
static spanpackage_t a_spans[MAXHEIGHT+1];
									// 1 extra for spanpackage that marks end

static finalvert_t r_p0, r_p1, r_p2;
static pixel_t *d_pcolormap;
static int d_xdenom;
static edgetable *pedgetable;

static edgetable edgetables[12] = {
	{&r_p0, &r_p2, nil,   &r_p0, &r_p1, &r_p2, 0, 1, 2},
	{&r_p1, &r_p0, &r_p2, &r_p1, &r_p2, nil,   0, 2, 1},
	{&r_p0, &r_p2, nil,   &r_p1, &r_p2, nil,   1, 1, 1},
	{&r_p1, &r_p0, nil,   &r_p1, &r_p2, &r_p0, 0, 1, 2},
	{&r_p0, &r_p2, &r_p1, &r_p0, &r_p1, nil,   0, 2, 1},
	{&r_p2, &r_p1, nil,   &r_p2, &r_p0, nil,   0, 1, 1},
	{&r_p2, &r_p1, nil,   &r_p2, &r_p0, &r_p1, 0, 1, 2},
	{&r_p2, &r_p1, &r_p0, &r_p2, &r_p0, nil,   0, 2, 1},
	{&r_p1, &r_p0, nil,   &r_p1, &r_p2, nil,   0, 1, 1},
	{&r_p2, &r_p1, nil,   &r_p0, &r_p1, nil,   1, 1, 1},
	{&r_p1, &r_p0, nil,   &r_p2, &r_p0, nil,   1, 1, 1},
	{&r_p0, &r_p2, nil,   &r_p0, &r_p1, nil,   0, 1, 1},
};

// FIXME: some of these can become statics
static int a_sstepxfrac, a_tstepxfrac, r_lstepx[3], a_ststepxwhole;
static int r_sstepx, r_tstepx, r_lstepy[3], r_sstepy, r_tstepy;
static int r_zistepx, r_zistepy;
static int d_aspancount, d_countextrastep;

static spanpackage_t *d_pedgespanpackage;
static int ystart;
static pixel_t *d_pdest, *d_ptex;
static uzint *d_pz;
static int d_sfrac, d_tfrac, d_light[3], d_zi;
static int d_ptexextrastep, d_sfracextrastep;
static int d_lightextrastep[3], d_pdestextrastep, d_tfracextrastep;
static int d_lightbasestep[3], d_pdestbasestep, d_ptexbasestep;
static int d_sfracbasestep, d_tfracbasestep;
static int d_ziextrastep, d_zibasestep;
static int d_pzextrastep, d_pzbasestep;
static int ubasestep, errorterm, erroradjustup, erroradjustdown;

typedef struct {
	int		quotient;
	int		remainder;
} adivtab_t;

static const adivtab_t adivtab[32*32] = {
#include "adivtab.h"
};

static pixel_t *skintable[MAX_LBM_HEIGHT];
static pixel_t *skinstart;
int skinwidth;

void D_PolysetDrawSpans8 (spanpackage_t *pspanpackage, pixel_t *colormap, byte alpha);
void D_PolysetCalcGradients (int skinwidth);
void D_DrawSubdiv (pixel_t *colormap);
void D_DrawNonSubdiv (void);
void D_PolysetRecursiveTriangle (finalvert_t *p1, finalvert_t *p2, finalvert_t *p3, byte alpha, int l[3]);
void D_PolysetSetEdgeTable (void);
void D_RasterizeAliasPolySmooth (void);
void D_PolysetScanLeftEdge (int height);

/*
================
D_PolysetDraw
================
*/
void D_PolysetDraw (pixel_t *colormap)
{
	if (r_affinetridesc.drawtype)
	{
		D_DrawSubdiv (colormap);
	}
	else
	{
		D_DrawNonSubdiv ();
	}
}


/*
================
D_PolysetDrawFinalVerts
================
*/
void D_PolysetDrawFinalVerts (finalvert_t *fv, int numverts, pixel_t *colormap, byte alpha)
{
	int		i, z;
	uzint	*zbuf;

	for(i = 0 ; i < numverts; i++, fv++){
		// valid triangle coordinates for filling can include the bottom and
		// right clip edges, due to the fill rule; these shouldn't be drawn
		if (fv->u < r_refdef.vrectright && fv->v < r_refdef.vrectbottom){
			z = fv->zi >> 16;
			zbuf = zspantable[fv->v] + fv->u;
			if(z >= *zbuf){
				pixel_t p = addlight(skintable[fv->t >> 16][fv->s >> 16], fv->l[0], fv->l[1], fv->l[2]);
				int n = d_scantable[fv->v] + fv->u;
				if(r_drawflags & DRAW_BLEND){
					d_viewbuffer[n] = blendalpha(p, d_viewbuffer[n], alpha);
				}else{
					d_viewbuffer[n] = p;
					*zbuf = z;
				}
			}
		}
	}
}


/*
================
D_DrawSubdiv
================
*/
void D_DrawSubdiv (pixel_t *colormap)
{
	mtriangle_t		*ptri;
	finalvert_t		*pfv, *index0, *index1, *index2;
	int				i;
	int				lnumtriangles;

	pfv = r_affinetridesc.pfinalverts;
	ptri = r_affinetridesc.ptriangles;
	lnumtriangles = r_affinetridesc.numtriangles;

	for (i=0 ; i<lnumtriangles ; i++)
	{
		index0 = pfv + ptri[i].vertindex[0];
		index1 = pfv + ptri[i].vertindex[1];
		index2 = pfv + ptri[i].vertindex[2];

		if (((index0->v - index1->v) *
			 (index0->u - index2->u) -
			 (index0->u - index1->u) *
			 (index0->v - index2->v)) >= 0)
		{
			continue;
		}

		d_pcolormap = colormap + (index0->l[0] & 0xFF00);

		if (ptri[i].facesfront)
		{
			D_PolysetRecursiveTriangle(index0, index1, index2, currententity->alpha, index0->l);
		}
		else
		{
			int		s0, s1, s2;

			s0 = index0->s;
			s1 = index1->s;
			s2 = index2->s;

			if (index0->flags & ALIAS_ONSEAM)
				index0->s += r_affinetridesc.seamfixupX16;
			if (index1->flags & ALIAS_ONSEAM)
				index1->s += r_affinetridesc.seamfixupX16;
			if (index2->flags & ALIAS_ONSEAM)
				index2->s += r_affinetridesc.seamfixupX16;

			D_PolysetRecursiveTriangle(index0, index1, index2, currententity->alpha, index0->l);

			index0->s = s0;
			index1->s = s1;
			index2->s = s2;
		}
	}
}


/*
================
D_DrawNonSubdiv
================
*/
void D_DrawNonSubdiv (void)
{
	mtriangle_t		*ptri;
	finalvert_t		*pfv, *index0, *index1, *index2;
	int				i;
	int				lnumtriangles;

	pfv = r_affinetridesc.pfinalverts;
	ptri = r_affinetridesc.ptriangles;
	lnumtriangles = r_affinetridesc.numtriangles;

	for (i=0 ; i<lnumtriangles ; i++, ptri++)
	{
		index0 = pfv + ptri->vertindex[0];
		index1 = pfv + ptri->vertindex[1];
		index2 = pfv + ptri->vertindex[2];

		d_xdenom = (index0->v - index1->v) * (index0->u - index2->u) -
				(index0->u - index1->u) * (index0->v - index2->v);

		if(d_xdenom >= 0)
			continue;

		memmove(r_p0.x, index0->x, sizeof(r_p0.x));
		memmove(r_p1.x, index1->x, sizeof(r_p1.x));
		memmove(r_p2.x, index2->x, sizeof(r_p2.x));

		if (!ptri->facesfront)
		{
			if (index0->flags & ALIAS_ONSEAM)
				r_p0.s += r_affinetridesc.seamfixupX16;
			if (index1->flags & ALIAS_ONSEAM)
				r_p1.s += r_affinetridesc.seamfixupX16;
			if (index2->flags & ALIAS_ONSEAM)
				r_p2.s += r_affinetridesc.seamfixupX16;
		}

		D_PolysetSetEdgeTable();
		D_RasterizeAliasPolySmooth();
	}
}


/*
================
D_PolysetRecursiveTriangle
================
*/
void D_PolysetRecursiveTriangle (finalvert_t *lp1, finalvert_t *lp2, finalvert_t *lp3, byte alpha, int l[3])
{
	finalvert_t new, *temp;
	uzint *zbuf;
	int d, z;

	d = lp2->u - lp1->u;
	if (d < -1 || d > 1)
		goto split;
	d = lp2->v - lp1->v;
	if (d < -1 || d > 1)
		goto split;

	d = lp3->u - lp2->u;
	if (d < -1 || d > 1)
		goto split2;
	d = lp3->v - lp2->v;
	if (d < -1 || d > 1)
		goto split2;

	d = lp1->u - lp3->u;
	if (d < -1 || d > 1)
		goto split3;
	d = lp1->v - lp3->v;
	if (d < -1 || d > 1)
	{
split3:
		temp = lp1;
		lp1 = lp3;
		lp3 = lp2;
		lp2 = temp;

		goto split;
	}

	return;			// entire tri is filled

split2:
	temp = lp1;
	lp1 = lp2;
	lp2 = lp3;
	lp3 = temp;

split:
	// split this edge
	new.u = (lp1->u + lp2->u) >> 1;
	new.v = (lp1->v + lp2->v) >> 1;
	new.s = (lp1->s + lp2->s) >> 1;
	new.t = (lp1->t + lp2->t) >> 1;
	new.zi = (lp1->zi + lp2->zi) >> 1;

	// draw the point if splitting a leading edge
	if (lp2->v > lp1->v)
		goto nodraw;
	if ((lp2->v == lp1->v) && (lp2->u < lp1->u))
		goto nodraw;

	z = new.zi >> 16;
	zbuf = zspantable[new.v] + new.u;
	if (z >= *zbuf){
		pixel_t p = addlight(skintable[new.t >> 16][new.s >> 16], l[0], l[1], l[2]);
		int n = d_scantable[new.v] + new.u;
		if(r_drawflags & DRAW_BLEND){
			d_viewbuffer[n] = blendalpha(p, d_viewbuffer[n], alpha);
		}else{
			d_viewbuffer[n] = p;
			*zbuf = z;
		}
	}

nodraw:
	// recursively continue
	D_PolysetRecursiveTriangle(lp3, lp1, &new, alpha, l);
	D_PolysetRecursiveTriangle(lp3, &new, lp2, alpha, l);
}


/*
================
D_PolysetUpdateTables
================
*/
void D_PolysetUpdateTables (void)
{
	int		i;
	pixel_t	*s;

	if (r_affinetridesc.skinwidth != skinwidth ||
		r_affinetridesc.pskin != skinstart)
	{
		skinwidth = r_affinetridesc.skinwidth;
		skinstart = r_affinetridesc.pskin;
		s = skinstart;
		for (i=0 ; i<MAX_LBM_HEIGHT ; i++, s+=skinwidth)
			skintable[i] = s;
	}
}

/*
===================
D_PolysetScanLeftEdge
====================
*/
void D_PolysetScanLeftEdge (int height)
{
	do
	{
		d_pedgespanpackage->pdest = d_pdest;
		d_pedgespanpackage->pz = d_pz;
		d_pedgespanpackage->count = d_aspancount;
		d_pedgespanpackage->ptex = d_ptex;

		d_pedgespanpackage->sfrac = d_sfrac;
		d_pedgespanpackage->tfrac = d_tfrac;

		// FIXME: need to clamp l, s, t, at both ends?
		d_pedgespanpackage->light[0] = d_light[0];
		d_pedgespanpackage->light[1] = d_light[1];
		d_pedgespanpackage->light[2] = d_light[2];
		d_pedgespanpackage->zi = d_zi;

		d_pedgespanpackage++;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_pdest += d_pdestextrastep;
			d_pz += d_pzextrastep;
			d_aspancount += d_countextrastep;
			d_ptex += d_ptexextrastep;
			d_sfrac += d_sfracextrastep;
			d_ptex += d_sfrac >> 16;

			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracextrastep;
			if (d_tfrac & 0x10000)
			{
				d_ptex += r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}
			d_light[0] += d_lightextrastep[0];
			d_light[1] += d_lightextrastep[1];
			d_light[2] += d_lightextrastep[2];
			d_zi += d_ziextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_pdest += d_pdestbasestep;
			d_pz += d_pzbasestep;
			d_aspancount += ubasestep;
			d_ptex += d_ptexbasestep;
			d_sfrac += d_sfracbasestep;
			d_ptex += d_sfrac >> 16;
			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracbasestep;
			if (d_tfrac & 0x10000)
			{
				d_ptex += r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}
			d_light[0] += d_lightbasestep[0];
			d_light[1] += d_lightbasestep[1];
			d_light[2] += d_lightbasestep[2];
			d_zi += d_zibasestep;
		}
	} while (--height);
}


/*
===================
D_PolysetSetUpForLineScan
====================
*/
void D_PolysetSetUpForLineScan(fixed8_t startvertu, fixed8_t startvertv,
		fixed8_t endvertu, fixed8_t endvertv)
{
	double		dm, dn;
	int			tm, tn, n;

	// TODO: implement x86 version

	errorterm = -1;

	tm = endvertu - startvertu;
	tn = endvertv - startvertv;

	if (((tm <= 16) && (tm >= -15)) &&
		((tn <= 16) && (tn >= -15)))
	{
		n = ((tm+15) << 5) + (tn+15);
		ubasestep = adivtab[n].quotient;
		erroradjustup = adivtab[n].remainder;
		erroradjustdown = tn;
	}
	else
	{
		dm = (double)tm;
		dn = (double)tn;

		FloorDivMod (dm, dn, &ubasestep, &erroradjustup);

		erroradjustdown = dn;
	}
}


/*
================
D_PolysetCalcGradients
================
*/
void D_PolysetCalcGradients (int skinwidth)
{
	float	xstepdenominv, ystepdenominv, t0, t1;
	float	p0v_minus_p2v, p1v_minus_p2v, p0u_minus_p2u, p1u_minus_p2u;
	int i;

	p0u_minus_p2u = r_p0.u - r_p2.u;
	p0v_minus_p2v = r_p0.v - r_p2.v;
	p1u_minus_p2u = r_p1.u - r_p2.u;
	p1v_minus_p2v = r_p1.v - r_p2.v;

	xstepdenominv = 1.0 / (float)d_xdenom;

	ystepdenominv = -xstepdenominv;

	// ceil () for light so positive steps are exaggerated, negative steps
	// diminished,  pushing us away from underflow toward overflow. Underflow is
	// very visible, overflow is very unlikely, because of ambient lighting
	for(i = 0; i < 3; i++){
		t0 = r_p0.l[i] - r_p2.l[i];
		t1 = r_p1.l[i] - r_p2.l[i];
		r_lstepx[i] = (int)ceil((t1 * p0v_minus_p2v - t0 * p1v_minus_p2v) * xstepdenominv);
		r_lstepy[i] = (int)ceil((t1 * p0u_minus_p2u - t0 * p1u_minus_p2u) * ystepdenominv);
	}

	t0 = r_p0.s - r_p2.s;
	t1 = r_p1.s - r_p2.s;
	r_sstepx = (int)((t1 * p0v_minus_p2v - t0 * p1v_minus_p2v) * xstepdenominv);
	r_sstepy = (int)((t1 * p0u_minus_p2u - t0 * p1u_minus_p2u) * ystepdenominv);

	t0 = r_p0.t - r_p2.t;
	t1 = r_p1.t - r_p2.t;
	r_tstepx = (int)((t1 * p0v_minus_p2v - t0 * p1v_minus_p2v) * xstepdenominv);
	r_tstepy = (int)((t1 * p0u_minus_p2u - t0 * p1u_minus_p2u) * ystepdenominv);

	t0 = r_p0.zi - r_p2.zi;
	t1 = r_p1.zi - r_p2.zi;
	r_zistepx = (int)((t1 * p0v_minus_p2v - t0 * p1v_minus_p2v) * xstepdenominv);
	r_zistepy = (int)((t1 * p0u_minus_p2u - t0 * p1u_minus_p2u) * ystepdenominv);

	a_sstepxfrac = r_sstepx & 0xFFFF;
	a_tstepxfrac = r_tstepx & 0xFFFF;
	a_ststepxwhole = skinwidth * (r_tstepx >> 16) + (r_sstepx >> 16);
}

/*
================
D_PolysetDrawSpans8
================
*/
void D_PolysetDrawSpans8 (spanpackage_t *pspanpackage, pixel_t *colormap, byte alpha)
{
	int		lcount;
	pixel_t	*lpdest, *lptex;
	int		lsfrac, ltfrac;
	int		llight[3];
	int		lzi;
	uzint	*lpz;

	do
	{
		lcount = d_aspancount - pspanpackage->count;

		errorterm += erroradjustup;
		if (errorterm >= 0)
		{
			d_aspancount += d_countextrastep;
			errorterm -= erroradjustdown;
		}
		else
		{
			d_aspancount += ubasestep;
		}

		if (lcount)
		{
			lpdest = pspanpackage->pdest;
			lptex = pspanpackage->ptex;
			lpz = pspanpackage->pz;
			lsfrac = pspanpackage->sfrac;
			ltfrac = pspanpackage->tfrac;
			llight[0] = pspanpackage->light[0];
			llight[1] = pspanpackage->light[1];
			llight[2] = pspanpackage->light[2];
			lzi = pspanpackage->zi;

			do
			{
				if ((lzi >> 16) >= *lpz){
					pixel_t p = addlight(*lptex, llight[0], llight[1], llight[2]);
					if(r_drawflags & DRAW_BLEND){
						*lpdest = blendalpha(
							p,
							*lpdest,
							alpha
						);
					}else{
						*lpdest = p;
						// gel mapping	*lpdest = gelmap[*lpdest];
						*lpz = lzi >> 16;
					}
				}
				lpdest++;
				lzi += r_zistepx;
				lpz++;
				llight[0] += r_lstepx[0];
				llight[1] += r_lstepx[1];
				llight[2] += r_lstepx[2];
				lptex += a_ststepxwhole;
				lsfrac += a_sstepxfrac;
				lptex += lsfrac >> 16;
				lsfrac &= 0xFFFF;
				ltfrac += a_tstepxfrac;
				if (ltfrac & 0x10000)
				{
					lptex += r_affinetridesc.skinwidth;
					ltfrac &= 0xFFFF;
				}
			} while (--lcount);
		}

		pspanpackage++;
	} while (pspanpackage->count != Q_MININT);
}

/*
================
D_RasterizeAliasPolySmooth
================
*/
void D_RasterizeAliasPolySmooth (void)
{
	int initialleftheight, initialrightheight;
	finalvert_t *plefttop, *prighttop, *pleftbottom, *prightbottom;
	int working_lstepx[3], originalcount;

	plefttop = pedgetable->pleftedgevert0;
	prighttop = pedgetable->prightedgevert0;

	pleftbottom = pedgetable->pleftedgevert1;
	prightbottom = pedgetable->prightedgevert1;

	initialleftheight = pleftbottom->v - plefttop->v;
	initialrightheight = prightbottom->v - prighttop->v;
	assert(initialleftheight <= MAXHEIGHT);

	// set the s, t, and light gradients, which are consistent across the triangle
	// because being a triangle, things are affine
	D_PolysetCalcGradients (r_affinetridesc.skinwidth);

	// rasterize the polygon
	// scan out the top (and possibly only) part of the left edge
	d_pedgespanpackage = a_spans;

	ystart = plefttop->v;
	d_aspancount = plefttop->u - prighttop->u;

	d_ptex = r_affinetridesc.pskin + (plefttop->s >> 16) +
			(plefttop->t >> 16) * r_affinetridesc.skinwidth;
	d_sfrac = plefttop->s & 0xFFFF;
	d_tfrac = plefttop->t & 0xFFFF;
	d_zi = plefttop->zi;
	d_light[0] = plefttop->l[0];
	d_light[1] = plefttop->l[1];
	d_light[2] = plefttop->l[2];

	d_pdest = d_viewbuffer + ystart * screenwidth + plefttop->u;
	d_pz = d_pzbuffer + ystart * d_zwidth + plefttop->u;

	if (initialleftheight == 1)
	{
		d_pedgespanpackage->pdest = d_pdest;
		d_pedgespanpackage->pz = d_pz;
		d_pedgespanpackage->count = d_aspancount;
		d_pedgespanpackage->ptex = d_ptex;

		d_pedgespanpackage->sfrac = d_sfrac;
		d_pedgespanpackage->tfrac = d_tfrac;

		// FIXME: need to clamp l, s, t, at both ends?
		d_pedgespanpackage->light[0] = d_light[0];
		d_pedgespanpackage->light[1] = d_light[1];
		d_pedgespanpackage->light[2] = d_light[2];
		d_pedgespanpackage->zi = d_zi;

		d_pedgespanpackage++;
	}
	else
	{
		D_PolysetSetUpForLineScan(plefttop->u, plefttop->v,
							  pleftbottom->u, pleftbottom->v);

		d_pzbasestep = d_zwidth + ubasestep;
		d_pzextrastep = d_pzbasestep + 1;
		d_pdestbasestep = screenwidth + ubasestep;
		d_pdestextrastep = d_pdestbasestep + 1;

		// TODO: can reuse partial expressions here

		// for negative steps in x along left edge, bias toward overflow rather than
		// underflow (sort of turning the floor () we did in the gradient calcs into
		// ceil (), but plus a little bit)
		working_lstepx[0] = r_lstepx[0];
		working_lstepx[1] = r_lstepx[1];
		working_lstepx[2] = r_lstepx[2];
		if(ubasestep < 0){
			working_lstepx[0] -= 1;
			working_lstepx[1] -= 1;
			working_lstepx[2] -= 1;
		}

		d_countextrastep = ubasestep + 1;
		d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
				((r_tstepy + r_tstepx * ubasestep) >> 16) *
				r_affinetridesc.skinwidth;
		d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
		d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;
		d_lightbasestep[0] = r_lstepy[0] + working_lstepx[0] * ubasestep;
		d_lightbasestep[1] = r_lstepy[1] + working_lstepx[1] * ubasestep;
		d_lightbasestep[2] = r_lstepy[2] + working_lstepx[2] * ubasestep;
		d_zibasestep = r_zistepy + r_zistepx * ubasestep;

		d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
				((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
				r_affinetridesc.skinwidth;
		d_sfracextrastep = (r_sstepy + r_sstepx*d_countextrastep) & 0xFFFF;
		d_tfracextrastep = (r_tstepy + r_tstepx*d_countextrastep) & 0xFFFF;
		d_lightextrastep[0] = d_lightbasestep[0] + working_lstepx[0];
		d_lightextrastep[1] = d_lightbasestep[1] + working_lstepx[1];
		d_lightextrastep[2] = d_lightbasestep[2] + working_lstepx[2];
		d_ziextrastep = d_zibasestep + r_zistepx;

		D_PolysetScanLeftEdge (initialleftheight);
	}

	// scan out the bottom part of the left edge, if it exists
	if (pedgetable->numleftedges == 2)
	{
		int		height;

		plefttop = pleftbottom;
		pleftbottom = pedgetable->pleftedgevert2;

		height = pleftbottom->v - plefttop->v;

		// TODO: make this a function; modularize this function in general

		ystart = plefttop->v;
		d_aspancount = plefttop->u - prighttop->u;
		d_ptex = r_affinetridesc.pskin + (plefttop->s >> 16) +
				(plefttop->t >> 16) * r_affinetridesc.skinwidth;
		d_sfrac = 0;
		d_tfrac = 0;
		d_zi = plefttop->zi;
		d_light[0] = plefttop->l[0];
		d_light[1] = plefttop->l[1];
		d_light[2] = plefttop->l[2];

		d_pdest = d_viewbuffer + ystart * screenwidth + plefttop->u;
		d_pz = d_pzbuffer + ystart * d_zwidth + plefttop->u;

		if (height == 1)
		{
			d_pedgespanpackage->pdest = d_pdest;
			d_pedgespanpackage->pz = d_pz;
			d_pedgespanpackage->count = d_aspancount;
			d_pedgespanpackage->ptex = d_ptex;

			d_pedgespanpackage->sfrac = d_sfrac;
			d_pedgespanpackage->tfrac = d_tfrac;

			// FIXME: need to clamp l, s, t, at both ends?
			d_pedgespanpackage->light[0] = d_light[0];
			d_pedgespanpackage->light[1] = d_light[1];
			d_pedgespanpackage->light[2] = d_light[2];
			d_pedgespanpackage->zi = d_zi;
		}
		else
		{
			D_PolysetSetUpForLineScan(plefttop->u, plefttop->v,
								  pleftbottom->u, pleftbottom->v);

			d_pdestbasestep = screenwidth + ubasestep;
			d_pdestextrastep = d_pdestbasestep + 1;
			d_pzbasestep = d_zwidth + ubasestep;
			d_pzextrastep = d_pzbasestep + 1;

			if (ubasestep < 0){
				working_lstepx[0] = r_lstepx[0] - 1;
				working_lstepx[1] = r_lstepx[1] - 1;
				working_lstepx[2] = r_lstepx[2] - 1;
			}else{
				working_lstepx[0] = r_lstepx[0];
				working_lstepx[1] = r_lstepx[1];
				working_lstepx[2] = r_lstepx[2];
			}

			d_countextrastep = ubasestep + 1;
			d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
					((r_tstepy + r_tstepx * ubasestep) >> 16) *
					r_affinetridesc.skinwidth;
			d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
			d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;
			d_lightbasestep[0] = r_lstepy[0] + working_lstepx[0] * ubasestep;
			d_lightbasestep[1] = r_lstepy[1] + working_lstepx[1] * ubasestep;
			d_lightbasestep[2] = r_lstepy[2] + working_lstepx[2] * ubasestep;
			d_zibasestep = r_zistepy + r_zistepx * ubasestep;

			d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
					((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
					r_affinetridesc.skinwidth;
			d_sfracextrastep = (r_sstepy+r_sstepx*d_countextrastep) & 0xFFFF;
			d_tfracextrastep = (r_tstepy+r_tstepx*d_countextrastep) & 0xFFFF;
			d_lightextrastep[0] = d_lightbasestep[0] + working_lstepx[0];
			d_lightextrastep[1] = d_lightbasestep[1] + working_lstepx[1];
			d_lightextrastep[2] = d_lightbasestep[2] + working_lstepx[2];
			d_ziextrastep = d_zibasestep + r_zistepx;

			D_PolysetScanLeftEdge (height);
		}
	}

	// scan out the top (and possibly only) part of the right edge, updating the
	// count field
	d_pedgespanpackage = a_spans;

	D_PolysetSetUpForLineScan(prighttop->u, prighttop->v,
						  prightbottom->u, prightbottom->v);
	d_aspancount = 0;
	d_countextrastep = ubasestep + 1;
	originalcount = a_spans[initialrightheight].count;
	a_spans[initialrightheight].count = Q_MININT; // mark end of the spanpackages
	D_PolysetDrawSpans8 (a_spans, currententity->colormap, currententity->alpha);

	// scan out the bottom part of the right edge, if it exists
	if (pedgetable->numrightedges == 2)
	{
		int				height;
		spanpackage_t	*pstart;

		pstart = a_spans + initialrightheight;
		pstart->count = originalcount;

		d_aspancount = prightbottom->u - prighttop->u;

		prighttop = prightbottom;
		prightbottom = pedgetable->prightedgevert2;

		height = prightbottom->v - prighttop->v;

		D_PolysetSetUpForLineScan(prighttop->u, prighttop->v,
							  prightbottom->u, prightbottom->v);

		d_countextrastep = ubasestep + 1;
		a_spans[initialrightheight + height].count = Q_MININT;
											// mark end of the spanpackages
		D_PolysetDrawSpans8 (pstart, currententity->colormap, currententity->alpha);
	}
}


/*
================
D_PolysetSetEdgeTable
================
*/
void D_PolysetSetEdgeTable (void)
{
	int edgetableindex;

	edgetableindex = 0;	// assume the vertices are already in
						//  top to bottom order

	// determine which edges are right & left, and the order in which
	// to rasterize them
	if(r_p0.v >= r_p1.v){
		if(r_p0.v == r_p1.v){
			pedgetable = &edgetables[r_p0.v < r_p2.v ? 2 : 5];
			return;
		}
		edgetableindex = 1;
	}

	if(r_p0.v == r_p2.v){
		pedgetable = &edgetables[edgetableindex ? 8 : 9];
		return;
	}
	if(r_p1.v == r_p2.v){
		pedgetable = &edgetables[edgetableindex ? 10 : 11];
		return;
	}

	if(r_p0.v > r_p2.v)
		edgetableindex += 2;
	if(r_p1.v > r_p2.v)
		edgetableindex += 4;

	pedgetable = &edgetables[edgetableindex];
}
