#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

/*
=====================
D_DrawZPoint
=====================
*/
void D_DrawZPoint (void)
{
	byte	*pdest;
	uzint	*pz;
	int		izi;
	
	pz = d_pzbuffer + (d_zwidth * r_zpointdesc.v) + r_zpointdesc.u;
	pdest = d_viewbuffer + d_scantable[r_zpointdesc.v] + r_zpointdesc.u;
	izi = (int)(r_zpointdesc.zi * 0x8000);

	if (*pz <= izi)
	{
		*pz = izi;
		*pdest = r_zpointdesc.color;
	}
}

