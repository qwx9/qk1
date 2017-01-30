#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

qboolean isDedicated;
mainstacksize = 512*1024;

void
Sys_Quit(void)
{
	Host_Shutdown();
	threadexitsall(nil);
}

vlong
flen(int fd)
{
	vlong l;
	Dir *d;

	if((d = dirfstat(fd)) == nil)	/* file assumed extant and readable */ 
		sysfatal("flen: %r");
	l = d->length;
	free(d);
	return l;
}

double
Sys_FloatTime(void)
{
	static long secbase;

	if(secbase == 0)
		secbase = time(nil);
	return nsec()/1000000000.0 - secbase;
}

void *
emalloc(ulong b)
{
	void *p;

	if((p = malloc(b)) == nil)
		sysfatal("malloc %lud: %r", b);
	return p;
}

void
Sys_HighFPPrecision(void)
{
}

void
Sys_LowFPPrecision(void)
{
}

static void
croak(void *, char *note)
{
	if(strncmp(note, "sys:", 4) == 0){
		IN_Shutdown();
		SNDDMA_Shutdown();
	}
	noted(NDFLT);
}

void
threadmain(int c, char **v)
{
	static char basedir[1024];
	int j;
	char *home;
	double time, oldtime, newtime;
	quakeparms_t parms;

	memset(&parms, 0, sizeof parms);

	/* ignore fp exceptions: rendering shit assumes they are */
	setfcr(getfcr() & ~(FPOVFL|FPUNFL|FPINVAL|FPZDIV));
	notify(croak);

	COM_InitArgv(c, v);
	parms.argc = com_argc;
	parms.argv = com_argv;
	argv0 = *v;

	parms.memsize = 8*1024*1024;
	if(j = COM_CheckParm("-mem"))
		parms.memsize = atoi(com_argv[j+1]) * 1024*1024;
	parms.membase = emalloc(parms.memsize);

	if(home = getenv("home")){
		snprint(basedir, sizeof basedir, "%s/lib/quake", home);
		free(home);
	}else
		snprint(basedir, sizeof basedir, "/sys/lib/quake");
	parms.basedir = basedir;

	Host_Init(&parms);

	oldtime = Sys_FloatTime() - 0.1;
	for(;;){
		// find time spent rendering last frame
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if(cls.state == ca_dedicated){   // play vcrfiles at max speed
			if(time < sys_ticrate.value){
				//usleep(1);
				continue;       // not time to run a server only tic yet
			}
			time = sys_ticrate.value;
        	}

		if(time > sys_ticrate.value*2)
			oldtime = newtime;
		else
			oldtime += time;

		Host_Frame(time);
	}
}
