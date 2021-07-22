#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

int svonly;

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

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);

	for(p = buf; *p; p++){
		*p &= 0x7f;
		if(*p < 32 && *p != 10 && *p != 13 && *p != 9)
			print("[%02x]", *p);
		else
			print("%c", *p);
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
Sys_FileTime(char *path)
{
	uchar bs[1024];

	if(stat(path, bs, sizeof bs) < 0)
		return -1;
	return *((int *)(bs+2+2+4+1+4+8+4+4));	/* mtime[4] */
}

void
Sys_mkdir(char *path)
{
	int d;

	if((d = create(path, OREAD, DMDIR|0777)) < 0)
		fprint(2, "Sys_mkdir:create: %r\n");
	else
		close(d);
}

vlong
flen(int fd)
{
	uchar bs[1024];

	if(fstat(fd, bs, sizeof bs) < 0){
		fprint(2, "flen:fstat: %r\n");
		return -1;
	}
	return *((vlong *)(bs+2+2+4+1+4+8+4+4+4));	/* length[8] */
}

vlong
Sys_FileOpenRead(char *path, int *fd)
{
	if((*fd = open(path, OREAD)) < 0)
		return -1;
	return flen(*fd);
}

int
Sys_FileOpenWrite(char *path)
{
	int fd;

	if((fd = open(path, OREAD|OTRUNC)) < 0)
		sysfatal("Sys_FileOpenWrite:open: %r");
	return fd;
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
}
