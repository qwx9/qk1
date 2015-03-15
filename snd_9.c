#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

int audio_fd;
int sndon;
int wpos;
int stid = -1;
enum{
	Nbuf	= 16
};
Channel *schan;


void sproc (void *)
{
	int n;

	for(;;){
		if(recv(schan, nil) < 0){
			Con_Printf("sproc:recv %r\n");
			break;
		}
		if((n = write(audio_fd, shm->buffer, shm->samplebits/8 * shm->samples)) < 0){
			Con_Printf("sproc:write: %r\n");
			break;
		}
		wpos += n;
	}
	stid = -1;
}

qboolean SNDDMA_Init(void)
{
	int i;

	sndon = 0;

	if((audio_fd = open("/dev/audio", OWRITE)) < 0){
		Con_Printf("Could not open /dev/audio: %r\n");
		return 0;
	}

	shm = &sn;
	shm->splitbuffer = 0;

	if((i = COM_CheckParm("-sndbits")) != 0)
		shm->samplebits = atoi(com_argv[i+1]);
	if(shm->samplebits != 16 && shm->samplebits != 8)
		shm->samplebits = 16;

	if((i = COM_CheckParm("-sndspeed")) != 0)
		shm->speed = atoi(com_argv[i+1]);
	else
		shm->speed = 44100;

	shm->channels = 2;
	if(COM_CheckParm("-sndmono") != 0)
		shm->channels = 1;

	shm->samples = 4096;
	shm->submission_chunk = 1;

	if((shm->buffer = mallocz(shm->samplebits/8 * shm->samples, 1)) == nil)
		Sys_Error("SNDDMA_Init:mallocz: %r\n");
	shm->samplepos = 0;
	sndon = 1;
	schan = chancreate(sizeof(int), Nbuf);
	if((stid = proccreate(sproc, nil, 8192)) < 0){
		stid = -1;
		SNDDMA_Shutdown();
		Sys_Error("SNDDMA_Init:proccreate: %r\n");
	}
	return 1;
}

int SNDDMA_GetDMAPos(void)
{
	if(!sndon)
		return 0;
	shm->samplepos = wpos / (shm->samplebits/8);
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if(!sndon)
		return;

	close(audio_fd);
	free(shm->buffer);
	if(stid != -1){
		threadkill(stid);
		stid = -1;
	}
	if(schan != nil){
		chanfree(schan);
		schan = nil;
	}
	sndon = 0;
}

void SNDDMA_Submit(void)
{
	if(nbsend(schan, nil) < 0){
		Con_Printf("SNDDMA_Submit:nbsend: %r\n");
		SNDDMA_Shutdown();
	}
}
