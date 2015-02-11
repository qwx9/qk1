#include <u.h>
#include <libc.h>
#include "quakedef.h"

int audio_fd;
int snd_inited;

int tryrates[] = { 11025, 22051, 44100, 8000 };

qboolean SNDDMA_Init(void)
{
	int i;

	snd_inited = 0;

	if((audio_fd = open("/dev/audio", OWRITE)) < 0){
		Con_Printf("%s open /dev/audio: %r\n", argv0);
		return 0;
	}

	// get size of dma buffer
	/*
	rc = ioctl(audio_fd, SNDCTL_DSP_RESET, 0);
	rc = ioctl(audio_fd, SNDCTL_DSP_GETCAPS, &caps);
	rc = ioctl(audio_fd, SNDCTL_DSP_GETOSPACE, &info);
	*/

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
	else if(COM_CheckParm("-sndstereo") != 0)
		shm->channels = 2;

	//shm->samples = info.fragstotal * info.fragsize / (shm->samplebits/8);
	shm->samples = 1024;
	shm->submission_chunk = 1;

	// memory map the dma buffer
	//shm->buffer = mmap(NULL, info.frags.total * info.fragsize, PROT_WRITE, MAP_FILE|MAP_SHARED, audio_fd, 0);

	if((shm->buffer = malloc(shm->samplebits/8 * shm->samples)) == nil){
		Con_Printf("%s malloc: %r\n", argv0);
		close(audio_fd);
		return 0;
	}

	/*
	rc = ioctl(audio_fd, SNDCTL_DSP_SPEED, &shm->speed);
	rc = shm->samplebits == 16 ? AFMT_S16_LE : AFMT_U8;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETFMT, &rc);

	// toggle the trigger & start her up
	rc = 0;
	rc  = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &rc);
	rc = PCM_ENABLE_OUTPUT;
	rc = ioctl(audio_fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	*/

	shm->samplepos = 0;
	snd_inited = 1;
	return 1;
}

int SNDDMA_GetDMAPos(void)
{
	if (!snd_inited)
		return 0;

	/* FIXME */
	//shm->samplepos = count.bytes/shm->samplebits/8 & shm->samples-1;
	//shm->samplepos = count.ptr / (shm->samplebits / 8);

	return shm->samplepos;
}

void SNDDMA_Shutdown(void)
{
	if(snd_inited){
		close(audio_fd);
		free(shm->buffer);	/* FIXME: fault read */
		snd_inited = 0;
	}
}

void SNDDMA_Submit(void)
{
	/* FIXME */
	//write(audio_fd, shm->buffer, sizeof shm->buffer);
	write(audio_fd, shm->buffer, shm->samplebits/8 * shm->samples);
}
