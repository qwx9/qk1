// sys.h -- non-portable functions

enum{
	THin	= 1,
	THsnd	= 2,
	THnet	= 3
};
extern int svonly;

int Sys_mkdir(char *);
s64int flen(int);

void Sys_Error(char *, ...);
void Sys_Printf(char *, ...);
void Sys_Quit(void);
double Sys_DoubleTime(void);
char *Sys_ConsoleInput(void);
void Sys_Sleep(void);
void Sys_SendKeyEvents(void);	// Perform Key_Event () callbacks until the input que is empty
void Sys_LowFPPrecision(void);
void Sys_HighFPPrecision(void);
void Sys_SetFPCW(void);
void Sys_Init(void);
void initparm(quakeparms_t *);
u64int nanosec(void);

void*	emalloc(ulong);
