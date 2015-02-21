#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

qboolean isDedicated;
int nostdout = 0;
mainstacksize = 512*1024;	/* FIXME */

void Sys_Printf (char *fmt, ...)
{
	char buf[1024];
	uchar *p;
	va_list arg;

	if(nostdout)
		return;
	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	//write(1, buf, out-buf);
	for(p = (uchar *)buf; *p; p++){
		*p &= 0x7f;
		if((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			print("[%02x]", *p);
		else
			print("%c", *p);
	}
}

void Sys_Quit (void)
{
	Host_Shutdown();
	exits(nil);
}

void Sys_Warn (char *msg, ...)
{
	char buf[1024], *out;
	va_list arg;

	out = seprint(buf, buf+sizeof(buf), "%s: ", argv0);
	va_start(arg, msg);
	out = vseprint(out, buf+sizeof(buf), msg, arg);
	va_end(arg);
	out = seprint(out, buf+sizeof(buf), ": %r\n");
	write(2, buf, out-buf);
}

void Sys_Error (char *error, ...)
{
	char buf[1024], *out;
	va_list arg;

	va_start(arg, error);
	out = vseprint(buf, buf+sizeof(buf), error, arg);
	va_end(arg);
	out = seprint(out, buf+sizeof(buf), "\n%s: %r\n", argv0);
	write(2, buf, out-buf);
	Host_Shutdown();
	sysfatal("ending");
} 

int Sys_FileTime (char *path)
{
	uchar	sb[1024];

	if(stat(path, sb, sizeof sb) < 0){
		Sys_Warn("Sys_FileTime:stat");
		return -1;
	}
	return *((int *)(sb+25));
}

void Sys_mkdir (char *path)
{
	int	d;

	if((d = create(path, OREAD, DMDIR|0777)) < 0)
		Sys_Warn("Sys_mkdir:create %s", path);
	else
		close(d);
}

vlong Sys_FileOpenRead (char *path, int *handle)
{
	int	d;
	uchar	bs[1024];

	d = open (path, OREAD);
	*handle = d;
	if(d < 0)
		return -1;
	if(fstat(d, bs, sizeof bs) < 0)
		Sys_Error("Sys_FileOpenRead %s failed", path);
	return *((vlong *)(bs+33));
}

int Sys_FileOpenWrite (char *path)
{
	int     d;

	if((d = open(path, OREAD|OTRUNC)) < 0)
		Sys_Error("Sys_FileOpenWrite %s failed", path);
	return d;
}

int Sys_FileWrite (int handle, void *src, int count)
{
	return write(handle, src, count);
}

void Sys_FileClose (int handle)
{
	close(handle);
}

void Sys_FileSeek (int handle, int position)
{
	seek(handle, position, 0);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	return read(handle, dest, count);
}

double Sys_FloatTime (void)
{
	static long	secbase;

	if(secbase == 0)
		secbase = time(nil);
	return nsec()/1000000000.0 - secbase;
}

char *Sys_ConsoleInput(void)
{
	static char	text[256];
	int     len;

	if(cls.state == ca_dedicated){
		if((len = read(0, text, sizeof(text))) < 1)
			return nil;
		text[len-1] = '\0';
		return text;
	}
	return nil;
}

void Sys_HighFPPrecision (void)
{
}

void Sys_LowFPPrecision (void)
{
}

void threadmain (int c, char **v)
{
	static char basedir[1024];
	int j;
	char *home;
	double time, oldtime, newtime;
	quakeparms_t parms;
	extern int vcrFile;
	extern int recording;

	memset(&parms, 0, sizeof(parms));

	/* ignore fp exceptions (bad), rendering shit assumes they are */
	setfcr(getfcr() & ~(FPOVFL|FPUNFL|FPINVAL|FPZDIV));	/* FIXME */

	COM_InitArgv(c, v);
	parms.argc = com_argc;
	parms.argv = com_argv;
	argv0 = *v;

	parms.memsize = 8*1024*1024;
	if(j = COM_CheckParm("-mem"))
		parms.memsize = (int)(Q_atof(com_argv[j+1]) * 1024*1024);
	parms.membase = malloc(parms.memsize);

	if(home = getenv("home")){
		snprintf(basedir, sizeof basedir, "%s/lib/quake", home);
		free(home);
	}else
		snprintf(basedir, sizeof basedir, "/sys/lib/quake");
	parms.basedir = basedir;

	Host_Init(&parms);

	if(COM_CheckParm("-nostdout"))
		nostdout = 1;
	else
		print("(9)quake %4.2f\n", (float)VERSION);

	oldtime = Sys_FloatTime() - 0.1;
	for(;;){
		// find time spent rendering last frame
		newtime = Sys_FloatTime();
		time = newtime - oldtime;

		if(cls.state == ca_dedicated){   // play vcrfiles at max speed
			if(time < sys_ticrate.value && (vcrFile == -1 || recording)){
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
