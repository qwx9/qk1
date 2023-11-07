#include "quakedef.h"
#include <thread.h>

static int afd;
static QLock alock;
static uchar *mixbuf;
static Channel *ach;

static void
auproc(void *p)
{
	long sz, n;

	for(;;){
		if((sz = recvul(p)) < 1)
			break;
		qlock(&alock);
		n = write(afd, mixbuf, sz);
		qunlock(&alock);
		if(n != sz){
			Con_DPrintf("sndwrite: %r\n");
			break;
		}
	}
	chanclose(p);
	threadexits(nil);
}

void
sndstop(void)
{
}

void
sndwrite(uchar *buf, long sz)
{
	if(afd < 0)
		return;
	if(ach == nil){
		ach = chancreate(sizeof(ulong), 0);
		proccreate(auproc, ach, 4096);
	}
	qlock(&alock);
	sendul(ach, sz);
	mixbuf = buf;
	qunlock(&alock);
}

void
sndclose(void)
{
	close(afd);
	afd = -1;
}

int
sndopen(void)
{
	if((afd = open("/dev/audio", OWRITE)) < 0)
		return -1;
	return 0;
}
