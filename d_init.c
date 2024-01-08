#include "quakedef.h"

static cvar_t d_mipcap = {"d_mipcap", "0"};
static cvar_t d_mipscale = {"d_mipscale", "1"};

surfcache_t *d_initial_rover;
bool d_roverwrapped;
int d_minmip;
float d_scalemip[MIPLEVELS-1];

extern int *r_turb_turb;

static float basemip[MIPLEVELS-1] = {1.0, 0.5*0.8, 0.25*0.8};

void
D_Init(void)
{
	Cvar_RegisterVariable(&d_mipcap);
	Cvar_RegisterVariable(&d_mipscale);

	r_recursiveaffinetriangles = true;
	r_aliasuvscale = 1.0;
}

void
D_SetupFrame(void)
{
	int i;

	r_turb_turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));

	dvars.fb = r_dowarp ? r_warpbuffer : vid.buffer;
	dvars.w = vid.width;

	d_roverwrapped = false;
	d_initial_rover = sc_rover;

	d_minmip = clamp(d_mipcap.value, 0, MIPLEVELS-1);

	for(i = 0; i < MIPLEVELS-1; i++)
		d_scalemip[i] = basemip[i] * d_mipscale.value;
}
