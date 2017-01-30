void	setcvar(char*, char*);
void	setcvarv(char*, float);
int	clmsg(void);
void	abortdemo(void);
void	stopdemo(void);
void	recdemo(void);
void	playdemo(void);
void	timedemo(void);
void*	loadhunklmp(char *, int *);
void*	loadcachelmp(char *, cache_user_t *);
void*	loadstklmp(char *, void *, int, int *);
void	pointlmp(void);
void	endlmp(void);
int	rlmpmsg(void);
void	wlmpmsg(void);
int	reclmp(char*, int);
int	demolmp(char*);
void	crc(u8int);
void	initcrc(void);
void	ext(char*, char*);
void	radix(char*, char*);
void	unloadfs(void);
void	initfs(void);
void	dprint(char*, ...);
void	fatal(char*, ...);

#pragma	varargck	argpos	dprint	1
#pragma varargck	argpos	fatal	1
