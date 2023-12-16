#include "quakedef.h"

static cvar_t r_skyfog = {"r_skyfog", "0.5"};

static struct {
	float density;
	pixel_t color;
}r_fog;

#ifdef __plan9__
static double ln2c;
#define exp2(x) (exp((x) * (ln2c ? ln2c : (ln2c = log(2.0)))))
#endif

static void
fog(void)
{
	int i, j, n;
	float x;

	i = 1;
	n = Cmd_Argc();
	switch(n){
	case 5:
	case 2:
		x = atof(Cmd_Argv(i++));
		r_fog.density = max(0.0, x) * 0.016;
		r_fog.density *= r_fog.density;
		if(n == 2)
			break;
	case 4:
		r_fog.color = 0;
		for(j = 0; j < 3; j++, i++){
			x = atof(Cmd_Argv(i));
			r_fog.color = r_fog.color << 8 | (int)(0xff * clamp(x, 0.0, 1.0));
		}
		break;
	}
}

void
R_DrawFog(void)
{
	byte skyfogalpha, a;
	pixel_t *pix;
	int i, x, y;
	uzint *z;
	float d;

	if(r_fog.density <= 0.0)
		return;

	skyfogalpha = 255 * clamp(r_skyfog.value, 0, 1.0);
	/* FIXME(sigrid): this is super slow */
	for(y = r_refdef.vrect.y; y < r_refdef.vrectbottom; y++){
		i = y * vid.width + r_refdef.vrect.x;
		pix = vid.buffer + i;
		z = d_pzbuffer + i;
		for(x = r_refdef.vrect.x; x < r_refdef.vrectright; x++, i++, pix++, z++){
			if(*z >= 65536){
				d = 65536.0 / (float)(d_pzbuffer[i] >> 16);
				d = 1.0 - exp2(-r_fog.density * d*d);
				a = 255 * clamp(d, 0.0, 1.0);
			}else if(*z < 0){
				a = skyfogalpha;
			}else
				continue;

			if(a > 0)
				*pix = blendalpha(r_fog.color, *pix, a);
		}
	}
}

void
R_InitFog(void)
{
	Cmd_AddCommand("fog", fog);
	Cvar_RegisterVariable(&r_skyfog);
}
