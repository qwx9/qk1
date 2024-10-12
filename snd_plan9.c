#include "quakedef.h"
#include <thread.h>

typedef struct Mixbuf Mixbuf;
struct Mixbuf{
	byte *buf;
	long sz;
};

static int afd;
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
	Mixbuf m;
	long n;

	for(;;){
		if(recv(p, &m) < 0)
			break;
		n = write(afd, m.buf, m.sz);
		if(n != m.sz){
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
	Mixbuf m;

	if(ach == nil)
		return;
	m = (Mixbuf){buf, sz};
	send(ach, &m);
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
	ach = chancreate(sizeof(Mixbuf), 1);
	proccreate(auproc, ach, 4096);
	return 0;
}
