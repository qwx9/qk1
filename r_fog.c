#include "quakedef.h"

static cvar_t r_fog = {"r_fog", "1", true};
static cvar_t r_skyfog = {"r_skyfog", "0.5"};

static struct {
	float density;
	pixel_t color;
}r_fog_data;

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
		r_fog_data.density = max(0.0, x) * 0.016;
		r_fog_data.density *= r_fog_data.density;
		if(n == 2)
			break;
	case 4:
		r_fog_data.color = 0;
		for(j = 0; j < 3; j++, i++){
			x = atof(Cmd_Argv(i));
			r_fog_data.color = r_fog_data.color << 8 | (int)(0xff * clamp(x, 0.0, 1.0));
		}
		break;
	}
}

void
R_ResetFog(void)
{
	r_fog_data.density = 0;
	r_fog_data.color = 0x808080;
	setcvar("r_skyfog", "0");
}

void
R_DrawFog(void)
{
	byte ca0, ca1, ca2, skyfogalpha, a;
	pixel_t *pix;
	int i, x, y;
	uzint *z;
	float d;

	if(r_fog.value <= 0 || r_fog_data.density <= 0.0)
		return;

	ca0 = r_fog_data.color>> 0;
	ca1 = r_fog_data.color>> 8;
	ca2 = r_fog_data.color>>16;

	skyfogalpha = 255 * clamp(r_skyfog.value, 0, 1.0);
	/* FIXME(sigrid): this is super slow */
	for(y = r_refdef.vrect.y; y < r_refdef.vrectbottom; y++){
		i = y * vid.width + r_refdef.vrect.x;
		pix = vid.buffer + i;
		z = d_pzbuffer + i;
		for(x = r_refdef.vrect.x; x < r_refdef.vrectright; x++, i++, pix++, z++){
			if(*z > 65536){
				d = 65536.0 / (float)(*z >> 16);
				d = 1.0 - exp2(-r_fog_data.density * d*d);
				a = 255*clamp(d, 0.0, 1.0);
			}else if(*z < 0){
				a = skyfogalpha;
			}else
				continue;

			if(a > 0){
				*pix =
					((a*ca0 + (255-a)*((*pix>> 0)&0xff)) >> 8) << 0 |
					((a*ca1 + (255-a)*((*pix>> 8)&0xff)) >> 8) << 8 |
					((a*ca2 + (255-a)*((*pix>>16)&0xff)) >> 8) << 16;
			}
		}
	}
}

void
R_InitFog(void)
{
	Cmd_AddCommand("fog", fog);
	Cvar_RegisterVariable(&r_fog);
	Cvar_RegisterVariable(&r_skyfog);
}
