#include "quakedef.h"
#include <time.h>
#include <errno.h>

char *game;
int debug;
char lasterr[256] = {0};
static const char *disabled[32];
static int ndisabled;

int
nrand(int n)
{
	if(n < 1)
		return 0;
	return m_random_63() % n;
}

int
mkdir(const char *path, mode_t mode)
{
	USED(path); USED(mode);
	return -1;
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
	return (mkdir(path, 0770) == 0 || errno == EEXIST) ? 0 : -1;
}

char *
sys_timestamp(void)
{
	static char ts[32];
	struct tm *tm;
	time_t t;

	if((t = time(nil)) == (time_t)-1 || (tm = localtime(&t)) == nil)
		return nil;
	snprint(ts, sizeof(ts),
		"%04d%02d%02d-%02d%02d%02d",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec
	);

	return ts;
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
emalloc(long n)
{
	void *p;

	if(p = calloc(1, n), p == nil)
		fatal("emalloc");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

u64int
nanosec(void)
{
	return 0;
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
	int c, nargs, i;
	static char *paths[8] = {0};

	i = 0;
	paths[i++] = ".";

	m_random_init(time(nil));
	srand(time(nil));

	paths[i] = sys_wrpath();
	Host_Init(nargs, argv, paths);

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
