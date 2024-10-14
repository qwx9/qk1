#include "quakedef.h"
#include "colormatrix.h"

cvar_t v_saturation = {"v_saturation", "1", true};
cvar_t v_contrast = {"v_contrast", "1", true};
cvar_t v_brightness = {"v_brightness", "1", true};

s16int cm[4*4];
static s16int cm0[4*4] = {
	CM(1), CM(0), CM(0), CM(0),
	CM(0), CM(1), CM(0), CM(0),
	CM(0), CM(0), CM(1), CM(0),
	CM(0), CM(0), CM(0), CM(1),
};
static bool cmident = true;

static float cmf0[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1,
};
static float cmbrightness[4*4];
static float cmcontrast[4*4];
static float cmsaturation[4*4];
static float cmvblend[4*4];
static float gs[3] = {0.3086, 0.6094, 0.0820};

static void
mmul(float d[4*4], float a[4*4], float b[4*4])
{
	int i, j;

	for(i = 0; i < 4; i++){
		for(j = 0; j < 4; j++)
			d[4*i+j] = a[4*i+0]*b[4*0+j] + a[4*i+1]*b[4*1+j] + a[4*i+2]*b[4*2+j] + a[4*i+3]*b[4*3+j];
	}
}

static void
cmrecalc(void)
{
	float t[4*4], t2[4*4];
	int i;

	mmul(t, cmbrightness, cmcontrast);
	mmul(t2, t, cmsaturation);
	mmul(t, t2, cmvblend);
	for(i = 0; i < 4*4; i++)
		cm[i] = CM(t[i]);
	cmident = memcmp(cm, cm0, sizeof(cm)) == 0;
}

void
cmsetvblend(float b[4])
{
	float r;

	r = 1 - b[3];
	cmvblend[4*0+0] = r;
	cmvblend[4*0+3] = b[2];
	cmvblend[4*1+1] = r;
	cmvblend[4*1+3] = b[1];
	cmvblend[4*2+2] = r;
	cmvblend[4*2+3] = b[0];
	cmrecalc();
}

static void
cmcvarcb(cvar_t *var)
{
	float r, v, *m;

	if(var == &v_brightness){
		v = clamp(var->value, 0.5, 4.5);
		m = cmbrightness;
		m[4*0+0] = v;
		m[4*0+1] = 0;
		m[4*0+2] = 0;
		m[4*0+3] = 0;
		m[4*1+0] = 0;
		m[4*1+1] = v;
		m[4*1+2] = 0;
		m[4*1+3] = 0;
		m[4*2+0] = 0;
		m[4*2+1] = 0;
		m[4*2+2] = v;
		m[4*2+3] = 0;
	}else if(var == &v_contrast){
		v = clamp(var->value, 0.5, 1.5);
		m = cmcontrast;
		r = (1.0 - v)/2.0;
		m[4*0+0] = v;
		m[4*0+1] = 0;
		m[4*0+2] = 0;
		m[4*0+3] = r;
		m[4*1+0] = 0;
		m[4*1+1] = v;
		m[4*1+2] = 0;
		m[4*1+3] = r;
		m[4*2+0] = 0;
		m[4*2+1] = 0;
		m[4*2+2] = v;
		m[4*2+3] = r;
	}else if(var == &v_saturation){
		v = clamp(var->value, 0, 2);
		m = cmsaturation;
		r = 1 - v;
		m[4*0+0] = r*gs[0] + v;
		m[4*0+1] = r*gs[1];
		m[4*0+2] = r*gs[2];
		m[4*0+3] = 0;
		m[4*1+0] = r*gs[0];
		m[4*1+1] = r*gs[1] + v;
		m[4*1+2] = r*gs[2];
		m[4*1+3] = 0;
		m[4*2+0] = r*gs[0];
		m[4*2+1] = r*gs[1];
		m[4*2+2] = r*gs[2] + v;
		m[4*2+3] = 0;
	}else
		return;
	m[4*3+0] = 0;
	m[4*3+1] = 0;
	m[4*3+2] = 0;
	m[4*3+3] = 1;
	cmrecalc();
}

void
cminit(void)
{
	memmove(cm, cm0, sizeof(cm0));
	memmove(cmvblend, cmf0, sizeof(cmf0));

	Cvar_RegisterVariable(&v_brightness);
	v_brightness.cb = cmcvarcb;
	memmove(cmbrightness, cmf0, sizeof(cmf0));

	Cvar_RegisterVariable(&v_contrast);
	v_contrast.cb = cmcvarcb;
	memmove(cmcontrast, cmf0, sizeof(cmf0));

	Cvar_RegisterVariable(&v_saturation);
	v_saturation.cb = cmcvarcb;
	memmove(cmsaturation, cmf0, sizeof(cmf0));
}
