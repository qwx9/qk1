#include "quakedef.h"

static cvar_t r_fog = {"r_fog", "1", true};
static cvar_t r_skyfog = {"r_skyfog", "0.5"};

fogvars_t fogvars;

static void
fogrecalc(void)
{
	fogvars.pix = (fogvars.allowed && fogvars.enabled)
		? 0xff<<24 | fogvars.c2<<16 | fogvars.c1<<8 | fogvars.c0
		: 0;
	fogvars.skypix = mulalpha(fogvars.pix, 255 * clamp(r_skyfog.value, 0.0, 1.0));
}

static void
r_skyfog_cb(cvar_t *var)
{
	USED(var);
	fogrecalc();
}

static void
fog(cmd_t *c)
{
	int i, n;
	float x;
	char *s;

	USED(c);
	i = 1;
	n = Cmd_Argc();
	switch(n){
	case 5:
	case 2:
		x = atof(Cmd_Argv(i++));
		if(n == 2 && strncmp(s = Cmd_Argv(0), "gl_fog", 6) == 0 && s[6] != 'd'){ // Nehahra
			x = 255 * clamp(x, 0.0, 1.0);
			if(s[6] == 'r')
				fogvars.c2 = x;
			else if(s[6] == 'g')
				fogvars.c1 = x;
			else if(s[6] == 'b')
				fogvars.c0 = x;
			else if(s[6] == 'e'){
				fogvars.enabled = x > 0;
				setcvar("r_skyfog", x > 0 ? "1" : "0");
			}
			break;
		}
		fogvars.density = clamp(x, 0.0, 1.0) * 0.016;
		fogvars.density *= fogvars.density;
		fogvars.enabled = fogvars.density > 0.0;
		if(n == 2)
			break;
	case 4:
		x = atof(Cmd_Argv(i++));
		fogvars.c2 = 0xff * clamp(x, 0.0, 1.0);
		x = atof(Cmd_Argv(i++));
		fogvars.c1 = 0xff * clamp(x, 0.0, 1.0);
		x = atof(Cmd_Argv(i));
		fogvars.c0 = 0xff * clamp(x, 0.0, 1.0);
		r_skyfog_cb(&r_skyfog); /* recalculate sky fog */
		break;
	}
	fogrecalc();
}

void
R_ResetFog(void)
{
	memset(&fogvars, 0, sizeof(fogvars));
	fogvars.c2 = fogvars.c1 = fogvars.c0 = 0x80;
	fogvars.allowed = r_fog.value > 0.0;
	setcvar("r_skyfog", "0"); // calls fogrecalc
}

static void
r_fog_cb(cvar_t *var)
{
	fogvars.allowed = var->value > 0.0;
	fogrecalc();
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
