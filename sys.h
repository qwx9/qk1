// sys.h -- non-portable functions

enum{
	THin	= 1,
	THnet	= 3
};

void	Sys_Error(char *, ...);
void	Sys_Quit(void);
void	Sys_LowFPPrecision(void);
void	Sys_HighFPPrecision(void);
void*	emalloc(ulong);
ulong	Sys_FileTime(char *);
vlong	flen(int);
void	eread(int, void *, long);
void	ewrite(int, void *, long);
double	Sys_FloatTime(void);
char*	Sys_ConsoleInput(void);
void	Sys_SendKeyEvents(void);	// call Key_Event() until the input queue is empty
