#include "quakedef.h"

double d_pix_scale;
int d_vrectx, d_vrecty, d_vrectright_particle, d_vrectbottom_particle;
int d_pix_min, d_pix_max;
int d_scantable[MAXHEIGHT];
uzint *zspantable[MAXHEIGHT];

void
D_ViewChanged (void)
{
	int rowbytes, i;

	d_zwidth = vid.width;
	rowbytes = vid.rowbytes;
	scale_for_mip = max(xscale, yscale);

	d_pix_scale = 90.0 / max(r_refdef.fov_x, r_refdef.fov_y);
	d_pix_min = (r_refdef.vrect.width / 320.0) * d_pix_scale;
	d_pix_max = 0.5 + d_pix_min*4.0;
	d_pix_max = max(d_pix_max, d_pix_min+1);

	d_vrectx = r_refdef.vrect.x;
	d_vrecty = r_refdef.vrect.y;
	d_vrectright_particle = r_refdef.vrectright - d_pix_max;
	d_vrectbottom_particle = r_refdef.vrectbottom - d_pix_max;

	for(i = 0; i < vid.height; i++){
		d_scantable[i] = i*rowbytes;
		zspantable[i] = d_pzbuffer + i*d_zwidth;
	}
}

