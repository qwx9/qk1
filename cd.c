#include "quakedef.h"

cvar_t bgmvolume = {"bgmvolume", "0.5", true};

int cdtrk = 0, cdntrk = 0;
bool cdloop = false;

void
cdcmd(void)
{
	char *c;
	bool loop;

	if(Cmd_Argc() < 2){
usage:
		Con_Printf("cd (play|loop|stop|pause|resume|info) [track]\n");
		return;
	}
	c = Cmd_Argv(1);
	if((loop = cistrcmp(c, "loop") == 0) || cistrcmp(c, "play") == 0){
		if(Cmd_Argc() < 3)
			goto usage;
		playcd(atoi(Cmd_Argv(2)), loop);
	}else if(cistrcmp(c, "stop") == 0)
		stopcd();
	else if(cistrcmp(c, "pause") == 0)
		pausecd();
	else if(cistrcmp(c, "resume") == 0)
		resumecd();
	else if(cistrcmp(c, "info") == 0)
		Con_Printf("track %d/%d; loop %d; vol %.1f\n", cdtrk, cdntrk, cdloop, bgmvolume.value);
	else
		goto usage;
}
