#include <u.h>
#include <libc.h>
#include <thread.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

mainstacksize = 512*1024;
uchar *membase;
int memsize;

enum{
	KB = 1024*1024,
	Nmem = 8 * KB
};

static int debug;

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
	fprint(2, "%s\n", s);
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
	fprint(2, "%s: %s\n", argv0, s);
	exits(s);
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
	return nsec() / 1000000000.0 - t0;
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
	if(strncmp(note, "sys:", 4) == 0)
		IN_Grabm(0);
	noted(NDFLT);
}

void
threadmain(int c, char **v)
{
	int n;
	double t, t´, Δt;

	/* ignore fp exceptions: rendering shit assumes they are */
	setfcr(getfcr() & ~(FPOVFL|FPUNFL|FPINVAL|FPZDIV));
	notify(croak);
	COM_InitArgv(c, v);
	argv0 = *v;
	memsize = Nmem;
	if(n = COM_CheckParm("-mem"))
		memsize = atoi(com_argv[n+1]) * KB;
	membase = emalloc(memsize);
	srand(getpid());
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
