#include <u.h>
#include <time.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

char *game;
int debug;

void
fatal(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vfprintf(stderr, fmt, arg);
	va_end(arg);
	Host_Shutdown();
	exit(1);
}

void *
emalloc(ulong n)
{
	void *p;

	if(p = calloc(1, n), p == nil)
		fatal("emalloc");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

uvlong
nanosec(void)
{
	static time_t sec0;
	struct timespec t;

	if(clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		fatal("clock_gettime");
	if(sec0 == 0)
		sec0 = t.tv_sec;
	t.tv_sec -= sec0;
	return t.tv_sec*1000000000ULL + t.tv_nsec;
}

double
dtime(void)
{
	return nanosec()/1000000000.0;
}

void
game_shutdown(void)
{
	stopfb();
	Host_Shutdown();
	exit(0);
}

int
main(int argc, char **argv)
{
	double t, t2, dt;
/*
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
	case 'x':
		netmtpt = EARGF(usage());
		break;
	default: usage();
	}ARGEND
*/

	game = "ad";
	srand(getpid());
	Host_Init(argc, argv);
	t = dtime() - 1.0 / Fpsmax;
	for(;;){
		t2 = dtime();
		dt = t2 - t;
		if(cls.state == ca_dedicated){
			if(dt < sys_ticrate.value)
				continue;
			dt = sys_ticrate.value;
        }
		if(dt > sys_ticrate.value * 2)
			t = t2;
		else
			t += dt;
		Host_Frame(dt);
	}
	return 0;
}
