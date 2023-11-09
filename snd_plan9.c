#include "quakedef.h"
#include <thread.h>

static int afd;
static QLock alock;
static uchar *mixbuf;
static Channel *ach;

long
sndqueued(void)
{
	Dir *d;
	long n;

	n = (d = dirfstat(afd)) != nil ? d->length : 0;
	free(d);
	return n;
}

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
	threadexits(nil);
}

void
sndstop(void)
{
}

void
sndwrite(uchar *buf, long sz)
{
	if(ach == nil)
		return;
	qlock(&alock);
	sendul(ach, sz);
	mixbuf = buf;
	qunlock(&alock);
}

void
sndclose(void)
{
	if(ach != nil){
		chanclose(ach);
		close(afd);
		afd = -1;
	}
}

int
sndopen(void)
{
	if((afd = open("/dev/audio", OWRITE)) < 0)
		return -1;
	ach = chancreate(sizeof(ulong), 0);
	proccreate(auproc, ach, 4096);
	return 0;
}
