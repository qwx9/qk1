#include "quakedef.h"

static cvar_t r_fog = {"r_fog", "1", true};
static cvar_t r_skyfog = {"r_skyfog", "0.5"};

static struct {
	float density;
	byte c0, c1, c2, sky;
	bool allowed;
	int enabled;
}r_fog_data;

enum {
	Enfog = 1<<0,
	Enskyfog = 1<<1,
};

static void
fog(void)
{
	int i, n;
	float x;
	char *s;

	i = 1;
	n = Cmd_Argc();
	switch(n){
	case 5:
	case 2:
		x = atof(Cmd_Argv(i++));
		if(n == 2 && strncmp(s = Cmd_Argv(0), "gl_fog", 6) == 0 && s[6] != 'd'){ // Nehahra
			x = 255 * clamp(x, 0.0, 1.0);
			if(s[6] == 'r')
				r_fog_data.c2 = x;
			else if(s[6] == 'g')
				r_fog_data.c1 = x;
			else if(s[6] == 'b')
				r_fog_data.c0 = x;
			else if(s[6] == 'e'){
				r_fog_data.enabled = x > 0 ? (Enfog | Enskyfog) : 0;
				setcvar("r_skyfog", x > 0 ? "1" : "0");
			}
			return;
		}
		r_fog_data.density = clamp(x, 0.0, 1.0) * 0.016;
		r_fog_data.density *= r_fog_data.density;
		if(n == 2)
			break;
	case 4:
		x = atof(Cmd_Argv(i++));
		r_fog_data.c2 = 0xff * clamp(x, 0.0, 1.0);
		x = atof(Cmd_Argv(i++));
		r_fog_data.c1 = 0xff * clamp(x, 0.0, 1.0);
		x = atof(Cmd_Argv(i));
		r_fog_data.c0 = 0xff * clamp(x, 0.0, 1.0);
		break;
	}
	if(r_fog_data.density > 0.0)
		r_fog_data.enabled |= Enfog;
	else
		r_fog_data.enabled &= ~Enfog;
}

void
R_ResetFog(void)
{
	r_fog_data.density = 0;
	r_fog_data.c0 = r_fog_data.c1 = r_fog_data.c2 = 0x80;
	r_fog_data.enabled = 0;
	r_fog_data.allowed = r_fog.value > 0.0;
	setcvar("r_skyfog", "0");
}

static inline pixel_t
blend_fog(pixel_t pix, uzint z)
{
	byte a;
	float d;

	if(z > 65536){
		d = 65536ULL*65536ULL / (u64int)z;
		if((pix & ~0xffffff) == 0)
			d /= 1.5;
		d = 1.0 - exp2(-r_fog_data.density * d*d);
		a = 255*d;
	}else if(z < 0){
		a = r_fog_data.sky;
	}else{
		a = 0;
	}

	if(a == 0)
		return pix;

	return
		((a*r_fog_data.c0 + (255-a)*((pix>> 0)&0xff)) >> 8) << 0 |
		((a*r_fog_data.c1 + (255-a)*((pix>> 8)&0xff)) >> 8) << 8 |
		((a*r_fog_data.c2 + (255-a)*((pix>>16)&0xff)) >> 8) << 16;
}

pixel_t
R_BlendFog(pixel_t pix, uzint z)
{
	if(r_fog_data.enabled && r_fog_data.allowed)
		pix = blend_fog(pix, z);
	return pix;
}

void
R_DrawFog(void)
{
	pixel_t *pix;
	int i, x, y;
	uzint *z;

	if(!r_fog_data.enabled || !r_fog_data.allowed)
		return;

	/* FIXME(sigrid): this is super slow */
	for(y = r_refdef.vrect.y; y < r_refdef.vrectbottom; y++){
		i = y * vid.width + r_refdef.vrect.x;
		pix = vid.buffer + i;
		z = d_pzbuffer + i;
		for(x = r_refdef.vrect.x; x < r_refdef.vrectright; x++, i++, pix++, z++)
			*pix = blend_fog(*pix, *z);
	}
}

static void
r_fog_cb(cvar_t *var)
{
	r_fog_data.allowed = var->value > 0.0;
}

static void
r_skyfog_cb(cvar_t *var)
{
	if(var->value > 0.0)
		r_fog_data.enabled |= Enskyfog;
	else
		r_fog_data.enabled &= ~Enskyfog;
	r_fog_data.sky = 255 * clamp(var->value, 0.0, 1.0);
}

void
R_InitFog(void)
{
	r_fog.cb = r_fog_cb;
	r_skyfog.cb = r_skyfog_cb;
	Cmd_AddCommand("fog", fog);
	Cvar_RegisterVariable(&r_fog);
	Cvar_RegisterVariable(&r_skyfog);

	// Nehahra
	Cmd_AddCommand("gl_fogenable", fog)->hidden = true;
	Cmd_AddCommand("gl_fogdensity", fog)->hidden = true;
	Cmd_AddCommand("gl_fogred", fog)->hidden = true;
	Cmd_AddCommand("gl_foggreen", fog)->hidden = true;
	Cmd_AddCommand("gl_fogblue", fog)->hidden = true;
}
