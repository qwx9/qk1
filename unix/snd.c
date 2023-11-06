#include "quakedef.h"

cvar_t volume = {"volume", "0.7", 1};

void
stepsnd(vec3_t origin, vec3_t forward, vec3_t right, vec3_t up)
{
}

void
stopallsfx(void)
{
}

void
stopsfx(int n, int ch)
{
}

void
startsfx(int entn, int entch, Sfx *sfx, vec3_t zp, float vol, float att)
{
}

void
localsfx(char *s)
{
}

void
staticsfx(Sfx *sfx, vec3_t zp, float vol, float att)
{
}

void
touchsfx(char *s)
{
}

Sfx *
precachesfx(char *s)
{
	return nil;
}

void
sfxbegin(void)
{
}

void
shutsnd(void)
{
}

int
initsnd(void)
{
	Cvar_RegisterVariable(&volume);
	return -1;
}
