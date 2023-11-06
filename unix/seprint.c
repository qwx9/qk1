#include <stdio.h>
#include <stdarg.h>

char *
seprint(char *buf, char *e, char *fmt, ...)
{
	va_list a;
	int n, m;

	if(e <= buf)
		return e;

	va_start(a, fmt);
	m = e-buf-1;
	n = vsnprintf(buf, m, fmt, a);
	va_end(a);

	return buf + (n < m ? n : m);
}
