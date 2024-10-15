//
// console
//
extern int con_totallines;
extern int con_backscroll;
extern	bool con_forcedup;	// because no entities to refresh
extern bool con_initialized;
extern byte *con_chars;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines, bool drawinput);
#pragma varargck	argpos	Con_Printf	1
void Con_Printf (char *fmt, ...);
#pragma varargck	argpos	Con_DPrintf	1
void Con_DPrintf (char *fmt, ...);
void Con_Clear_f (cmd_t *c);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (cmd_t *c);

void Con_AddObject(char *name, void *obj);
void *Con_FindObject(char *name);
int Con_SearchObject(char *prefix, int len, void (*f)(char *, void *, void *), void *aux);
