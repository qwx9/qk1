#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

static int afd, sndon;
static uint wpos;
enum{
	Nbuf	= 16
};
static Channel *schan;
static QLock sndlock;


static void
sproc(void *)
{
	int n;

	threadsetgrp(THsnd);

	for(;;){
		if(recv(schan, nil) < 0)
			break;
		if((n = write(afd, shm->buffer, shm->samplebits/8 * shm->samples)) < 0)
			break;
		qlock(&sndlock);
		wpos += n;
		qunlock(&sndlock);
	}
	fprint(2, "sproc %d: %r\n", threadpid(threadid()));
}

qboolean
SNDDMA_Init(void)
{
	int i;

	sndon = 0;

	if((afd = open("/dev/audio", OWRITE)) < 0){
		fprint(2, "open: %r\n");
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
		sysfatal("SNDDMA_Init:mallocz: %r\n");
	shm->samplepos = 0;
	sndon = 1;
	wpos = 0;
	if((schan = chancreate(sizeof(int), Nbuf)) == nil)
		sysfatal("SNDDMA_Init:chancreate: %r");
	if(proccreate(sproc, nil, 8192) < 0)
		sysfatal("SNDDMA_Init:proccreate: %r");
	return 1;
}

int
SNDDMA_GetDMAPos(void)
{
	if(!sndon)
		return 0;
	qlock(&sndlock);
	shm->samplepos = wpos / (shm->samplebits/8);
	qunlock(&sndlock);
	return shm->samplepos;
}

void
SNDDMA_Shutdown(void)
{
	if(!sndon)
		return;

	threadkillgrp(THsnd);
	close(afd);
	free(shm->buffer);
	if(schan != nil){
		chanfree(schan);
		schan = nil;
	}
	sndon = 0;
}

void
SNDDMA_Submit(void)
{
	if(nbsend(schan, nil) < 0){
		fprint(2, "SNDDMA_Submit:nbsend: %r\n");
		SNDDMA_Shutdown();
	}
}
