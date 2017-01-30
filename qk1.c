#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "quakedef.h"

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

void
fatal(char *fmt, ...)
{
	va_list arg;

	Host_Shutdown();
	va_start(arg, fmt);
	sysfatal(fmt, arg);
	va_end(arg);
}
