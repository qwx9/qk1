// sys.h -- non-portable functions

//
// file IO
//
// returns the file size, -1 if file not found
vlong Sys_FileOpenRead (char *path, int *hndl);
int Sys_FileOpenWrite (char *path);
void Sys_FileClose (int handle);
void Sys_FileSeek (int handle, int position);
int Sys_FileRead (int handle, void *dest, int count);
int Sys_FileWrite (int handle, void *data, int count);
int Sys_FileTime (char *path);
void Sys_mkdir (char *path);

//
// system IO
//
void Sys_Error (char *error, ...);
void Sys_Printf (char *fmt, ...);
void Sys_Quit (void);
double Sys_FloatTime (void);
char *Sys_ConsoleInput (void);

void Sys_SendKeyEvents (void);
// Perform Key_Event () callbacks until the input que is empty

void Sys_LowFPPrecision (void);
void Sys_HighFPPrecision (void);
