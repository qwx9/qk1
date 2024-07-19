#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

mainstacksize = 512*1024;

/* FIXME: stupid-ass linking kludges */
server_static_t	svs;
bool ServerPaused(void)
{
	return 0;
}
void SV_SendServerInfoChange(char *, char *)
{
}

static void
croak(void *, char *note)
{
	if(strncmp(note, "sys:", 4) == 0){
		IN_Grabm(0);
		threadkillgrp(0);
	}
	noted(NDFLT);
}

void
threadmain(int argc, char *argv[])
{
	double time, oldtime, newtime;
	quakeparms_t q;

	/* ignore fp exceptions: rendering shit assumes they are */
	setfcr(getfcr() & ~(FPOVFL|FPUNFL|FPINVAL|FPZDIV));
	notify(croak);

	COM_InitArgv(argc, argv);
	initparm(&q);
	Sys_Init();
	Host_Init(&q);

	oldtime = Sys_DoubleTime ();
	for(;;){
		newtime = Sys_DoubleTime();	/* time spent rendering last frame */
		time = newtime - oldtime;
		Host_Frame(time);
		oldtime = newtime;
	}
}
