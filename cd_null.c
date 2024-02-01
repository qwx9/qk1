#include "quakedef.h"

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
playcd(int nt, bool loop)
{
	USED(nt); USED(loop);
}

int
initcd(void)
{
	Cvar_RegisterVariable(&bgmvolume);
	Cmd_AddCommand("cd", cdcmd);
	return 0;
}
