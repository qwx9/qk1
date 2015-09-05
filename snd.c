#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include "quakedef.h"

enum{
	Nbuf = 8
};
static int afd;
static int stid = -1;
static uint wpos;
static Channel *schan;
static QLock sndlock;


static void
sproc(void *)
{
	int n;

	for(;;){
		if(recv(schan, nil) < 0)
			break;
		if((n = write(afd, shm->buffer, SNBUF)) != SNBUF)
			break;
		qlock(&sndlock);
		wpos += n;
		qunlock(&sndlock);
	}
}

qboolean
SNDDMA_Init(void)
{
	if((afd = open("/dev/audio", OWRITE)) < 0){
		fprint(2, "open: %r\n");
		return 0;
	}

	shm = &sn;
	shm->splitbuffer = 0;
	shm->submission_chunk = 1;
	shm->samplebits = SAMPLESZ;
	shm->speed = RATE;
	shm->channels = 2;
	shm->samples = NSAMPLE;
	shm->samplepos = 0;
	if((shm->buffer = mallocz(SNBUF, 1)) == nil)
		sysfatal("mallocz: %r\n");

	wpos = 0;
	if((schan = chancreate(sizeof(int), Nbuf)) == nil)
		sysfatal("SNDDMA_Init:chancreate: %r");
	if((stid = proccreate(sproc, nil, 8192)) < 0)
		sysfatal("SNDDMA_Init:proccreate: %r");
	return 1;
}

uint
SNDDMA_GetDMAPos(void)
{
	if(stid < 0)
		return 0;
	qlock(&sndlock);
	shm->samplepos = wpos / SAMPLEB;
	qunlock(&sndlock);
	return shm->samplepos;
}

void
SNDDMA_Shutdown(void)
{
	if(stid < 0)
		return;

	threadint(stid);
	stid = -1;
	close(afd);
	free(shm->buffer);
	if(schan != nil)
		chanfree(schan);
	schan = nil;
}

void
SNDDMA_Submit(void)
{
	if(nbsend(schan, nil) < 0){
		fprint(2, "SNDDMA_Submit:nbsend: %r\n");
		SNDDMA_Shutdown();
	}
}
