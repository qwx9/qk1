#include "quakedef.h"
#include <thread.h>

int svonly, debug;

void *
emalloc(ulong n)
{
	void *p;

	if(p = mallocz(n, 1), p == nil)
		sysfatal("emalloc %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void
Sys_Printf(char *fmt, ...)
{
	char buf[1024];
	char *p;
	va_list arg;

	if(!svonly && !debug)
		return;
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	for(p = buf; *p; p++){
		*p &= 0x7f;
		if(*p < 32 && *p != 10 && *p != 13 && *p != 9)
			fprint(2, "[%02x]", *p);
		else
			fprint(2, "%c", *p);
	}
}

void
Sys_Error(char *fmt, ...)
{ 
	char s[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(s, s+sizeof s, fmt, arg);
	va_end(arg);
	Host_Shutdown();
	sysfatal("%s", s);
}

int
Sys_mkdir(char *path)
{
	int d;

	if(access(path, AEXIST) == 0)
		return 0;
	if((d = create(path, OREAD, DMDIR|0777)) < 0){
		fprint(2, "Sys_mkdir:create: %r\n");
		return -1;
	}
	close(d);
	return 0;
}

s64int
flen(int fd)
{
	s64int l;
	Dir *d;

	if((d = dirfstat(fd)) == nil)	/* file assumed extant and readable */
		sysfatal("flen: %r");
	l = d->length;
	free(d);
	return l;
}

double
Sys_DoubleTime(void)
{
	return nanosec()/1000000000.0;
}

void
Sys_HighFPPrecision(void)
{
}

void
Sys_LowFPPrecision(void)
{
}

void
initparm(quakeparms_t *q)
{
	int i;

	memset(q, 0, sizeof *q);
	q->argc = com_argc;
	q->argv = com_argv;
	q->memsize = 64*1024*1024;

	if(i = COM_CheckParm("-mem"))
		q->memsize = atoi(com_argv[i+1]) * 1024*1024;
	if(COM_CheckParm("-debug"))
		debug = 1;
	if((q->membase = malloc(q->memsize)) == nil)
		sysfatal("initparm:malloc: %r");
}

void
Sys_Quit(void)
{
	Host_Shutdown();
	threadexitsall(nil);
}

void
Sys_Init(void)
{
	m_random_init(time(nil));
	srand(time(nil));
}
