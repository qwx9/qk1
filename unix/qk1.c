#include "quakedef.h"
#include "parg.h"
#include <time.h>
#include <errno.h>
#include <fenv.h>

char *game;
int debug;
char lasterr[256] = {0};

char *
lerr(void)
{
	static char lasterrcopy[256];

	if(*lasterr == 0 && errno != 0)
		return strerror(errno);
	strcpy(lasterrcopy, lasterr);
	return lasterrcopy;
}

int
sys_mkdir(char *path)
{
	return mkdir(path, 0770);
}

int
nrand(int n)
{
	return random() % n;
}

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
	struct parg_state ps;
	int c, nargs;
	static char *paths[] = {
		"/usr/games/quake",
		nil,
		nil,
	};

	parg_init(&ps);
	nargs = 0;
	while((c = parg_getopt(&ps, argc, argv, "Ddg:")) >= 0){
		switch(c){
		case 1:
			argv[nargs++] = (char*)ps.optarg;
			break;
		case 'D':
			debug++;
			break;
		case 'd':
			dedicated = 1;
			break;
		case 'g':
			game = (char*)ps.optarg;
			break;
		case 'h':
			fprintf(stderr, "usage: qk1 [-g game]\n");
			return 0;
			break;
		case '?':
			fprintf(stderr, "unknown option -%c\n", ps.optopt);
			return 1;
			break;
		default:
			fprintf(stderr, "unhandled option -%c\n", c);
			return 1;
			break;
		}
	}

	srand(nanosec() + time(nil));

	paths[1] = strdup(va("%s/.quake", getenv("HOME")));
	Host_Init(nargs, argv, paths);

	fesetround(FE_TOWARDZERO);

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
