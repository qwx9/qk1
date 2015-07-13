#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

qboolean isDedicated;
mainstacksize = 512*1024;

static char end1[] =
	"                QUAKE: The Doomed Dimension by id Software\n"
	"  ----------------------------------------------------------------------------\n"
	"           CALL 1-800-IDGAMES TO ORDER OR FOR TECHNICAL SUPPORT\n"
	"             PRICE: $45.00 (PRICES MAY VARY OUTSIDE THE US.)\n"
	"\n"
	"  Yes! You only have one fourth of this incredible epic. That is because most\n"
	"   of you have paid us nothing or at most, very little. You could steal the\n"
	"   game from a friend. But we both know you'll be punished by God if you do.\n"
	"        WHY RISK ETERNAL DAMNATION? CALL 1-800-IDGAMES AND BUY NOW!\n"
	"             Remember, we love you almost as much as He does.\n"
	"\n"
	"            Programming: John Carmack, Michael Abrash, John Cash\n"
	"      Design: John Romero, Sandy Petersen, American McGee, Tim Willits\n"
	"                     Art: Adrian Carmack, Kevin Cloud\n"
	"               Biz: Jay Wilbur, Mike Wilson, Donna Jackson\n"
	"            Projects: Shawn Green   Support: Barrett Alexander\n"
	"              Sound Effects: Trent Reznor and Nine Inch Nails\n"
	"  For other information or details on ordering outside the US, check out the\n"
	"     files accompanying QUAKE or our website at http://www.idsoftware.com.\n"
	"    Quake is a trademark of Id Software, inc., (c)1996 Id Software, inc.\n"
	"     All rights reserved. NIN logo is a registered trademark licensed to\n"
	"                 Nothing Interactive, Inc. All rights reserved.\n";
static char end2[] =
	"        QUAKE by id Software\n"
	" -----------------------------------------------------------------------------\n"
	"        Why did you quit from the registered version of QUAKE? Did the\n"
	"        scary monsters frighten you? Or did Mr. Sandman tug at your\n"
	"        little lids? No matter! What is important is you love our\n"
	"        game, and gave us your money. Congratulations, you are probably\n"
	"        not a thief.\n"
	"                                                           Thank You.\n"
	"        id Software is:\n"
	"        PROGRAMMING: John Carmack, Michael Abrash, John Cash\n"
	"        DESIGN: John Romero, Sandy Petersen, American McGee, Tim Willits\n"
	"        ART: Adrian Carmack, Kevin Cloud\n"
	"        BIZ: Jay Wilbur, Mike Wilson     PROJECTS MAN: Shawn Green\n"
	"        BIZ ASSIST: Donna Jackson        SUPPORT: Barrett Alexander\n"
	"        SOUND EFFECTS AND MUSIC: Trent Reznor and Nine Inch Nails\n"
	"\n"
	"        If you need help running QUAKE refer to the text files in the\n"
	"        QUAKE directory, or our website at http://www.idsoftware.com.\n"
	"        If all else fails, call our technical support at 1-800-IDGAMES.\n"
	"      Quake is a trademark of Id Software, inc., (c)1996 Id Software, inc.\n"
	"        All rights reserved. NIN logo is a registered trademark licensed\n"
	"             to Nothing Interactive, Inc. All rights reserved.\n";


void
Sys_Printf(char *fmt, ...)
{
	char buf[1024], *p;
	va_list arg;

	va_start(arg, fmt);
	vsnprint(buf, sizeof buf, fmt, arg);
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
Sys_Quit(void)
{
	Host_Shutdown();
	print("\n");
	if(registered.value)
		print("%s\n", end2);
	else
		print("%s\n", end1);
	threadexitsall(nil);
}

void
Sys_Error(char *fmt, ...)
{
	char buf[1024], *p;
	va_list arg;

	va_start(arg, fmt);
	p = vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);
	p = seprint(p, buf+sizeof buf, "\n");
	ewrite(2, buf, p-buf);
	Host_Shutdown();
	sysfatal("ending.");
} 

ulong
Sys_FileTime(char *path)
{
	ulong t;
	Dir *d;

	if((d = dirstat(path)) == nil){
		fprint(2, "dirstat: %r");
		return -1;
	}
	t = d->mtime;
	free(d);
	return t;
}

void
Sys_mkdir(char *path)
{
	int d;

	/* don't care if it isn't a directory, caller doesn't check */
	if(access(path, AEXIST) == 0)
		return;
	if((d = create(path, OREAD, DMDIR|0777)) < 0)
		fprint(2, "Sys_mkdir:create: %r\n");
	else
		close(d);
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

void
eread(int fd, void *buf, long n)
{
	if(read(fd, buf, n) <= 0)
		sysfatal("eread: %r");
}

void
ewrite(int fd, void *buf, long n)
{
	if(write(fd, buf, n) != n)
		sysfatal("ewrite: %r");
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
