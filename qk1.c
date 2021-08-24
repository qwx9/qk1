#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

mainstacksize = 512*1024;
char *netmtpt = "/net";
uchar *membase;
int memsize;
char *game;
int debug;

enum{
	KB = 1024*1024,
	Nmem = 64 * KB
};

void
dprint(char *fmt, ...)
{
	char s[256];
	va_list arg;

	if(!debug)
		return;
	va_start(arg, fmt);
	vseprint(s, s+sizeof s, fmt, arg);
	va_end(arg);
	fprint(2, "%s", s);
}

/* FIXME: merge dprint/fatal? */
void
fatal(char *fmt, ...)
{
	char s[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(s, s+sizeof s, fmt, arg);
	va_end(arg);
	Host_Shutdown();
	sysfatal("%s", s);
}

void *
emalloc(ulong n)
{
	void *p;

	if(p = mallocz(n, 1), p == nil)
		sysfatal("emalloc %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
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
dtime(void)
{
	static double t0;

	if(t0 == 0.0)
		t0 = time(nil);
	return nanosec() / 1000000000.0 - t0;
}

void
fppdbl(void)
{
}

void
fppsgl(void)
{
}

void
shutdown(void)
{
	Host_Shutdown();
	threadexitsall(nil);
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

static void
usage(void)
{
	fprint(2, "usage: %s [-d] [-g game] [-m kB] [-s width height] [-x netmtpt]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	double t, t´, Δt;

	memsize = Nmem;
	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	case 'd':
		dedicated = 1;
		break;
	case 'g':
		game = EARGF(usage());
		break;
	case 'm':
		memsize = strtol(EARGF(usage()), nil, 0) * KB;
		if(memsize <= 0)
			sysfatal("invalid memsize");
		break;
	case 's':
		scaleon = 1;
		vid.width = strtol(EARGF(usage()), nil, 0);
		vid.height = strtol(EARGF(usage()), nil, 0);
		if(vid.width < 320 || vid.height < 200)
			sysfatal("invalid scale resolution");
		break;
	case 'x':
		netmtpt = EARGF(usage());
		break;
	default: usage();
	}ARGEND
	membase = emalloc(memsize);
	srand(getpid());
	/* ignore fp exceptions: rendering shit assumes they are */
	setfcr(getfcr() & ~(FPOVFL|FPUNFL|FPINVAL|FPZDIV));
	notify(croak);
	Host_Init();
	t = dtime() - 1.0 / Fpsmax;
	for(;;){
		t´ = dtime();
		Δt = t´ - t;
		if(cls.state == ca_dedicated){
			if(Δt < sys_ticrate.value)
				continue;
			Δt = sys_ticrate.value;
        	}
		if(Δt > sys_ticrate.value * 2)
			t = t´;
		else
			t += Δt;
		Host_Frame(Δt);
	}
}
