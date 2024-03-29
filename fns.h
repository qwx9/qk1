void	setpal(uchar*);
void	stopfb(void);
void	flipfb(void);
void	initfb(void);
void	conscmd(void);
void	Sys_SendKeyEvents(void);
void	stepcd(void);
void	stepsnd(vec3_t, vec3_t, vec3_t, vec3_t);
void	stopcd(void);
void	pausecd(void);
void	resumecd(void);
void	playcd(int, int);
void	stopallsfx(void);
void	stopsfx(int, int);
void	startsfx(int, int, Sfx *, vec3_t, float, float);
void	localsfx(char *);
void	staticsfx(Sfx *, vec3_t, float, float);
void	sfxbegin(void);
void	touchsfx(char *);
Sfx*	precachesfx(char *);
void	shutcd(void);
void	shutsnd(void);
int	initcd(void);
int	initsnd(void);
void	setcvar(char*, char*);
void	setcvarv(char*, float);
void	abortdemo(void);
void	stopdemo(void);
int	readcl(void);
void	timedemo(void);
void	recdemo(void);
void	playdemo(void);
void	crc(u8int);
void	initcrc(void);
char*	ext(char*, char*);
void	radix(char*, char*);
void*	loadhunklmp(char *, int *);
void*	loadcachelmp(char *, mem_user_t *);
void*	loadstklmp(char *, void *, int, int *);
void	loadpoints(void);
void	dumpcfg(void);
void	savnames(void);
int	dumpsav(char*, char*);
int	loadsav(char*);
void	closedm(void);
void	writedm(void);
int	readdm(void);
int	loaddm(char*);
int	opendm(char*, int);
void	initfs(char **paths);
#pragma varargck	argpos	fatal	1
_Noreturn void	fatal(char*, ...);
void*	emalloc(ulong);
vlong	flen(int);
double	dtime(void);
void	game_shutdown(void);
uvlong	nanosec(void);

char *lerr(void);
int	sys_mkdir(char *path);

long sndqueued(void);
void sndstop(void);
void sndwrite(uchar *buf, long sz);
void sndclose(void);
int sndopen(void);
