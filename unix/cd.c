#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

cvar_t bgmvolume = {"bgmvolume", "1", 1};

void
stopcd(void)
{
}

void
pausecd(void)
{
}

void
resumecd(void)
{
}

void
shutcd(void)
{
}

void
stepcd(void)
{
}

void
playcd(int nt, int loop)
{
	USED(nt); USED(loop);
}

static void
cdcmd(void)
{
}

int
initcd(void)
{
	Cvar_RegisterVariable(&bgmvolume);
	Cmd_AddCommand("cd", cdcmd);
	return -1;
}
