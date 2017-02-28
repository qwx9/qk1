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
void*	loadcachelmp(char *, cache_user_t *);
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
void	initfs(void);
void	dprint(char*, ...);
void	fatal(char*, ...);

#pragma	varargck	argpos	dprint	1
#pragma varargck	argpos	fatal	1
