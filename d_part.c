#include "quakedef.h"

extern double d_pix_scale;

void
D_DrawParticle (particle_t *pparticle)
{
	vec3_t local, transformed;
	double zi;
	pixel_t *pdest, color;
	uzint *pz;
	int i, izi, pix, count, u, v;

	// transform point
	VectorSubtract(pparticle->org, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);

	if(transformed[2] < PARTICLE_Z_CLIP)
		return;

	// project the point
	// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = xcenter + zi * transformed[0] + 0.5;
	v = ycenter - zi * transformed[1] + 0.5;

	if ((v > d_vrectbottom_particle) ||
		(u > d_vrectright_particle) ||
		(v < d_vrecty) ||
		(u < d_vrectx))
	{
		return;
	}

	pz = d_pzbuffer + d_zwidth*v + u;
	pdest = d_viewbuffer + d_scantable[v] + u;
	izi = zi * 0x8000 * 0x10000;

	zi = 1024.0 * d_pix_scale / Length(local);
	pix = clamp(zi, d_pix_min, d_pix_max) * r_part_scale.value;
	if(pix < 1)
		pix = 1;
	color = pparticle->color;

	for(count = pix; count; count--, pz += d_zwidth, pdest += screenwidth){
		for(i = 0; i < pix; i++){
			if(pz[i] <= izi){
				pz[i] = izi;
				pdest[i] = color;
			}
		}
	}
}
