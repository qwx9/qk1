//
// console
//
extern int con_totallines;
extern int con_backscroll;
extern	qboolean con_forcedup;	// because no entities to refresh
extern qboolean con_initialized;
extern byte *con_chars;
extern	int	con_notifylines;		// scan lines to clear for notify lines

void Con_DrawCharacter (int cx, int line, int num);

void Con_CheckResize (void);
void Con_Init (void);
void Con_DrawConsole (int lines, qboolean drawinput);
#pragma varargck	argpos	Con_Printf	1
void Con_Printf (char *fmt, ...);
#pragma varargck	argpos	Con_DPrintf	1
void Con_DPrintf (char *fmt, ...);
void Con_Clear_f (void);
void Con_DrawNotify (void);
void Con_ClearNotify (void);
void Con_ToggleConsole_f (void);
