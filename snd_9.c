#include <u.h>
#include <libc.h>
#include "quakedef.h"

int audio_fd;
int snd_inited;
int wpos;

int tryrates[] = { 11025, 22051, 44100, 8000 };

qboolean SNDDMA_Init(void)
{
	int i;

	snd_inited = 0;

	if((audio_fd = open("/dev/audio", OWRITE)) < 0){
		Sys_Warn("SNDDMA_Init:open");
		Con_Printf("Could not open /dev/audio\n");
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
	//else if(COM_CheckParm("-sndstereo") != 0)
	//	shm->channels = 2;

	//shm->samples = info.fragstotal * info.fragsize / (shm->samplebits/8);
	shm->samples = 1024;
	shm->submission_chunk = 1;

	/* FIXME: use Hunk_AllocName? */
	if((shm->buffer = mallocz(shm->samplebits/8 * shm->samples, 1)) == nil){
		Sys_Warn("SNDDMA_Init:malloc");
		close(audio_fd);
		return 0;
	}
	shm->samplepos = 0;
	snd_inited = 1;
	return 1;
}

int SNDDMA_GetDMAPos(void)
{
	if (!snd_inited)
		return 0;

	/* FIXME: utter bullshit */
	//shm->samplepos = count.bytes/shm->samplebits/8 & shm->samples-1;
	//shm->samplepos = count.ptr / (shm->samplebits / 8);
	shm->samplepos = wpos / shm->samplebits/8 & shm->samples-1;
	//wpos = 0;
	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if(snd_inited){
		close(audio_fd);
		free(shm->buffer);
		snd_inited = 0;
	}
}

void SNDDMA_Submit(void)
{
	int n;

	/* FIXME: utter bullshit */
	if((n = write(audio_fd, shm->buffer, shm->samplebits/8 * shm->samples)) < 0){
		Sys_Warn("SNDDMA_Submit:write");
		SNDDMA_Shutdown();
	}
	n += wpos;
}
