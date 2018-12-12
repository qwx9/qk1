#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include <bio.h>
#include "quakedef.h"

mainstacksize = 256*1024;

extern Channel *echan;
extern netadr_t cons[MAX_CLIENTS];

quakeparms_t q;

static Channel *inchan;

/* FIXME: stupid-ass linking kludges */
client_static_t	cls;
void Draw_BeginDisc(void){}
void Draw_EndDisc(void){}
void Host_Shutdown(void){}

char *
Sys_ConsoleInput(void)
{
	return nbrecvp(inchan);
}

static void
cproc(void *)
{
	char *s;
	Biobuf *bf;

	if(bf = Bfdopen(0, OREAD), bf == nil)
		sysfatal("Bfdopen: %r");
	for(;;){
		if(s = Brdstr(bf, '\n', 1), s == nil)
			break;
		if(sendp(inchan, s) < 0){
			free(s);
			break;
		}
		send(echan, nil);
	}
	Bterm(bf);
}

void
threadmain(int argc, char *argv[])
{
	double time, oldtime, newtime;
	netadr_t *a;

	svonly = 1;
	COM_InitArgv (argc, argv);
	initparm(&q);
	if((echan = chancreate(sizeof(int), 1)) == nil
	|| (inchan = chancreate(sizeof(void *), 2)) == nil)
		sysfatal("chancreate: %r");
	SV_Init(&q);
	if(proccreate(cproc, nil, 8192) < 0)
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
