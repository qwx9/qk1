#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

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
