#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

mainstacksize = 256*1024;

extern Channel *echan;
extern netadr_t cons[MAX_CLIENTS];

quakeparms_t q;

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
	if(iop < 0)
		return;
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
		send(echan, nil);
	}
	iop = -1;
}

void
threadmain(int argc, char *argv[])
{
	double time, oldtime, newtime;
	netadr_t *a;

	svonly = 1;
	COM_InitArgv (argc, argv);
	initparm(&q);
	if((echan = chancreate(sizeof(int), 1)) == nil)
		sysfatal("chancreate: %r");
	SV_Init(&q);
	if(proccreate(iproc, nil, 8192) < 0)
		sysfatal("proccreate iproc: %r");
	SV_Frame(0.1);	/* run one frame immediately for first heartbeat */
	oldtime = Sys_DoubleTime() - 0.1;
	for(;;){
		if(nbrecv(echan, nil) == 0){
			for(a=cons; a<cons+nelem(cons); a++)
				if(a->fd >= 0 && flen(a->fd) > 0)
					break;
			if(a == cons + nelem(cons))	
				sleep(1);
		}
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;
		SV_Frame(time);
	}	
}
