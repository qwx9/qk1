#include "quakedef.h"
#include <thread.h>

int mainstacksize = 1*1024*1024;
char *netmtpt = "/net";
char *game;
int debug;
static const char *disabled[32];
static int ndisabled;

int
qctz(unsigned x)
{
	unsigned r;

	if(x == 0)
		r = 32;
	else
		for(r = 0; (x & 1) == 0; x >>= 1, r++);

	return r;
}

bool
isdisabled(char *s)
{
	int i;

	for(i = 0; i < ndisabled; i++){
		if(strcmp(disabled[i], s) == 0)
			return true;
	}
	return false;
}

int
sys_mkdir(char *path)
{
	int d;

	if(access(path, AEXIST) == 0)
		return 0;
	if((d = create(path, OREAD, DMDIR|0777)) < 0){
		Con_DPrintf("Sys_mkdir: create: %r\n");
		return -1;
	}
	close(d);
	return 0;
}

char *
sys_timestamp(void)
{
	static char ts[32];
	Tm *tm;
	long t;

	if((t = time(nil)) < 0 || (tm = localtime(t)) == nil)
		return nil;
	snprint(ts, sizeof(ts),
		"%04d%02d%02d-%02d%02d%02d",
		tm->year + 1900, tm->mon + 1, tm->mday, tm->hour, tm->min, tm->sec
	);

	return ts;
}

char *
lerr(void)
{
	static char err[ERRMAX];
	rerrstr(err, sizeof(err));
	return err;
}

_Noreturn void
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
emalloc(long n)
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
	return nanosec() / 1000000000.0;
}

void
game_shutdown(void)
{
	stopfb();
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
	fprint(2, "usage: %s [-b basepath] [-d] [-g game] [-x netmtpt] [-N cd|snd]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char **argv)
{
	double t, t´, Δt;
	char *e;
	int i;
	static char *paths[8] = {0};

	i = 0;
	paths[i++] = ".";
	paths[i++] = "/sys/games/lib/quake";

	ARGBEGIN{
	case 'b':
		if(i < nelem(paths)-2)
			paths[i++] = EARGF(usage());
		break;
	case 'D':
		debug = 1;
		break;
	case 'd':
		dedicated = 1;
		break;
	case 'g':
		game = EARGF(usage());
		break;
	case 'x':
		netmtpt = EARGF(usage());
		break;
	case 'N':
		if(ndisabled < nelem(disabled))
			disabled[ndisabled++] = EARGF(usage());
		break;
	default: usage();
	}ARGEND

	m_random_init(time(nil));
	srand(time(nil));
	/* ignore fp exceptions: rendering shit assumes they are */
	setfcr(getfcr() & ~(FPOVFL|FPUNFL|FPINVAL|FPZDIV));
	notify(croak);

	e = getenv("home");
	paths[i] = smprint("%s/lib/quake", e);
	free(e);
	Host_Init(argc, argv, paths);

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
