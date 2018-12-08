#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

Addr myip;

static int lpid = -1, afd;
static Channel *lchan;

int
UDP_Init(void)
{
	char *s;
	uchar ip[IPaddrlen];

	fmtinstall('I', eipfmt);
	if(strcmp(hostname.string, "UNNAMED") == 0)
		if((s = getenv("sysname")) != nil){
			setcvar("hostname", s);
			free(s);
		}
	myipaddr(ip, nil);
	snprint(myip.ip, sizeof myip.ip, "%I", ip);
	snprint(myip.srv, sizeof myip.srv, "%hud", Udpport);
	return 0;
}

void
UDP_Shutdown(void)
{
	UDP_Listen(false);
}

static void
lproc(void *)
{
	int fd, lfd;
	char adir[40], ldir[40], data[100];

	snprint(data, sizeof data, "%s/udp!*!%s", netmtpt, myip.srv);
	if((afd = announce(data, adir)) < 0)
		sysfatal("announce: %r");
	for(;;){
		if((lfd = listen(adir, ldir)) < 0
			|| (fd = accept(lfd, ldir)) < 0
			|| close(lfd) < 0
			|| send(lchan, &fd) < 0)
			break;
	}
}

static void
udpname(void)
{
	if((lchan = chancreate(sizeof(int), 0)) == nil)
		sysfatal("chancreate: %r");
	if((lpid = proccreate(lproc, nil, 8192)) < 0)
		sysfatal("proccreate lproc: %r");
}

void
UDP_Listen(qboolean on)
{
	if(lpid < 0){
		if(on)
			udpname();
		return;
	}
	if(on)
		return;
	close(afd);
	chanclose(lchan);
	threadint(lpid);
	chanfree(lchan);
	lpid = -1;
}

void
udpinfo(Addr *a)
{
	NetConnInfo *nc;

	if((nc = getnetconninfo(nil, a->fd)) == nil){
		fprint(2, "getnetconninfo: %r\n");
		return;
	}
	strncpy(a->ip, nc->raddr, sizeof(a->ip)-1);
	strncpy(a->srv, nc->rserv, sizeof(a->srv)-1);
	strncpy(a->sys, nc->rsys, sizeof(a->sys)-1);
	free(nc);
}

int
UDP_Connect(Addr *a)
{
	if((a->fd = dial(netmkaddr(a->ip, "udp", a->srv), myip.srv, nil, nil)) < 0){
		fprint(2, "dial: %r\n");
		return -1;
	}
	return 0;
}

int
getnewcon(Addr *a)
{
	if(lpid < 0)
		return 0;
	if(nbrecv(lchan, &a->fd) == 0)
		return 0;
	udpinfo(a);
	return 1;
}

int
udpread(byte *buf, int len, Addr *a)
{
	int n;

	if(flen(a->fd) < 1)
		return 0;
	if((n = read(a->fd, buf, len)) <= 0){
		fprint(2, "udpread: %r\n");
		return -1;
	}
	return n;
}

int
udpwrite(uchar *buf, int len, Addr *a)
{
	if(write(a->fd, buf, len) != len){
		fprint(2, "udpwrite: %r\n");
		return -1;
	}
	return len;
}

char *
UDP_AddrToString(Addr *a)
{
	char *p;
	static char buf[52];

	strncpy(buf, a->sys, sizeof(buf)-1);
	if((p = strrchr(buf, '!')) != nil)
		*p = ':';
	return buf;
}

int
UDP_Broadcast(uchar *buf, int len)
{
	int fd;
	char ip[46];

	snprint(ip, sizeof ip, "%I", IPv4bcast);
	if((fd = dial(netmkaddr(ip, "udp", myip.srv), myip.srv, nil, nil)) < 0){
		fprint(2, "UDP_Broadcast: %r\n");
		return -1;
	}
	if(write(fd, buf, len) != len)
		fprint(2, "write: %r\n");
	close(fd);
	return 0;
}

int
getip(char *s, Addr *a)
{
	int fd, n;
	char buf[128], *f[4], *p;

	snprint(buf, sizeof buf, "%s/cs", netmtpt);
	if((fd = open(buf, ORDWR)) < 0)
		sysfatal("open: %r");

	if((p = strrchr(s, '!')) == nil)
		p = myip.srv;
	else
		p++;
	snprint(buf, sizeof buf, "udp!%s!%s", s, p);
	n = strlen(buf);
	if(write(fd, buf, n) != n){
		fprint(2, "translating %s: %r\n", s);
		return -1;
	}
	seek(fd, 0, 0);
	if((n = read(fd, buf, sizeof(buf)-1)) <= 0){
		fprint(2, "reading cs tables: %r");
		return -1;
	}
	buf[n] = 0;
	close(fd);
	if(getfields(buf, f, 4, 0, " !") < 2)
		goto err;
	strncpy(a->ip, f[1], sizeof(a->ip)-1);
	a->ip[sizeof(a->ip)-1] = 0;
	strncpy(a->srv, f[2], sizeof(a->srv)-1);
	a->srv[sizeof(a->srv)-1] = 0;
	snprint(a->sys, sizeof a->sys, "%s!%s", a->ip, a->srv);
	return 0;
err:
	fprint(2, "bad cs entry %s", buf);
	return -1;
}

int
UDP_AddrCompare(Addr *a1, Addr *a2)
{
	if(strcmp(a1->ip, a2->ip) != 0)
		return -1;
	if(strcmp(a1->srv, a2->srv) != 0)
		return 1;
	return 0;
}

ushort
UDP_GetSocketPort(Addr *a)
{
	ushort p;

	p = atoi(a->srv);
	return p;
}

void
UDP_SetSocketPort(Addr *a, ushort port)
{
	snprint(a->srv, sizeof a->srv, "%hud", port);	/* was htons'ed */
}
