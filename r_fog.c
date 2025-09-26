#include "quakedef.h"

static cvar_t r_fullbrightfog = {"r_fullbrightfog", "0.9"};
static cvar_t r_skyfog = {"r_skyfog", "0.5"};
static cvar_t r_fog = {"r_fog", "1", true};

fogvars_t fogvars;

static void
fogrecalc(void)
{
	fogvars.pix = (fogvars.allowed && fogvars.enabled)
					  ? 0xff << 24 | fogvars.c2 << 16 | fogvars.c1 << 8 | fogvars.c0
					  : 0;
	fogvars.fbpix = mulalpha(fogvars.pix, 255 * clamp(r_fullbrightfog.value, 0.0, 1.0));
	fogvars.skypix = mulalpha(fogvars.pix, 255 * clamp(r_skyfog.value, 0.0, 1.0));
}

static void
r_skyfog_cb(cvar_t *var)
{
	USED(var);
	fogrecalc();
}

static void
r_fullbrightfog_cb(cvar_t *var)
{
	USED(var);
	fogrecalc();
}

static void
fog(cmd_t *c)
{
	const char *s;
	int i, n;
	float x;

	USED(c);
	i = 1;
	n = Cmd_Argc();
	switch(n) {
	case 2:
	case 5:
		x = atof(Cmd_Argv(i++));
		if(n == 2 && strncmp(s = Cmd_Argv(0), "gl_fog", 6) == 0 && s[6] != 'd') { // Nehahra
			x = 255 * clamp(x, 0.0, 1.0);
			if(s[6] == 'r')
				fogvars.c2 = x;
			else if(s[6] == 'g')
				fogvars.c1 = x;
			else if(s[6] == 'b')
				fogvars.c0 = x;
			else if(s[6] == 'e') {
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
		break;
	}
	fogrecalc();
}

void
R_ApplyFog(model_t *m)
{
	const char *s;

	fogvars.enabled = false;
	fogvars.allowed = r_fog.value > 0.0;

	fogvars.pix = fogvars.fbpix = fogvars.skypix = 0;
	fogvars.density = 0;
	fogvars.c0 = fogvars.c1 = fogvars.c2 = 0x80;

	if((s = Mod_FindInWorldspawn(m, "fog")) != nil)
		Cmd_ExecuteString(va("fog %s", s), src_command);
	else if((s = Mod_FindInWorldspawn(m, "fog_density")) != nil) {
		Cmd_ExecuteString(va("fog %s", s), src_command);
		if(fogvars.enabled && (s = Mod_FindInWorldspawn(m, "fog_colour")) != nil)
			Cmd_ExecuteString(va("fog %s", s), src_command);
	}
	if(fogvars.enabled) {
		if((s = Mod_FindInWorldspawn(m, "skyfog_density")) == nil)
			s = "0";
		setcvar("r_skyfog", s);
	}
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
	r_fullbrightfog.cb = r_fullbrightfog_cb;
	r_skyfog.cb = r_skyfog_cb;
	Cmd_AddCommand("fog", fog);
	Cvar_RegisterVariable(&r_fog);
	Cvar_RegisterVariable(&r_fullbrightfog);
	Cvar_RegisterVariable(&r_skyfog);

	// Nehahra
	Cmd_AddCommand("gl_fogenable", fog)->hidden = true;
	Cmd_AddCommand("gl_fogdensity", fog)->hidden = true;
	Cmd_AddCommand("gl_fogred", fog)->hidden = true;
	Cmd_AddCommand("gl_foggreen", fog)->hidden = true;
	Cmd_AddCommand("gl_fogblue", fog)->hidden = true;
}
