#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

mainstacksize = 512*1024;
Channel *fuckchan;
quakeparms_t q;

static cvar_t extrasleep = {"extrasleep", "0"};	/* in ms */
static int iop = -1, pfd[2];


/* FIXME: stupid-ass linking kludges */
client_static_t	cls;
void Draw_BeginDisc(void){}
void Draw_EndDisc(void){}
void Host_Shutdown(void){}


char *
Sys_ConsoleInput(void)
{
	static char buf[256];

	if(iop < 0)
		return nil;
	if(flen(pfd[1]) < 1)	/* only poll for input */
		return nil;
	if(read(pfd[1], buf, sizeof buf) < 0)
		sysfatal("Sys_ConsoleInput:read: %r");
	return buf;
}

void
killiop(void)
{
	threadkillgrp(THin);
	close(pfd[0]);
	close(pfd[1]);
	iop = -1;
}

static void
iproc(void *)
{
	int n;
	char s[256];

	threadsetgrp(THin);

	if((iop = pipe(pfd)) < 0)
		sysfatal("iproc:pipe: %r");
	for(;;){
		if((n = read(0, s, sizeof s)) <= 0)
			break;
		s[n-1] = 0;
		if((write(pfd[0], s, n)) != n)
			break;
		if(nbsend(fuckchan, nil) < 0)
			sysfatal("chan really wanted to, but it's just too tired. %r");
	}
	fprint(2, "iproc %d: %r\n", threadpid(threadid()));
	iop = -1;
}

static void
croak(void *, char *note)
{
	if(!strncmp(note, "sys:", 4)){
		killiop();
		NET_Shutdown();
	}
	noted(NDFLT);
}

void
threadmain(int argc, char *argv[])
{
	double time, oldtime, newtime;

	svonly = 1;
	COM_InitArgv (argc, argv);
	initparm(&q);
	notify(croak);
	if((fuckchan = chancreate(sizeof(int), 1)) == nil)
		sysfatal("chancreate fuckchan: %r");
	SV_Init(&q);
	Cvar_RegisterVariable(&extrasleep);
	if(proccreate(iproc, nil, 8192) < 0)
		sysfatal("proccreate iproc: %r");

	SV_Frame(0.1);	/* run one frame immediately for first heartbeat */

	oldtime = Sys_DoubleTime() - 0.1;
	for(;;){
		recv(fuckchan, nil);	/* we just don't give any for free */

		newtime = Sys_DoubleTime();	/* time since last docking */
		time = newtime - oldtime;
		oldtime = newtime;
		SV_Frame(time);		

		/* just a way to screw up fucking conformation on purpose */
		if(extrasleep.value)
			sleep(extrasleep.value);
	}	
}
