#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include <ip.h>
#include "quakedef.h"

Channel *echan, *lchan;
char *netmtpt = "/net";
sizebuf_t net_message;
netadr_t *net_from;
netadr_t cons[2*MAX_CLIENTS];
static uchar netbuf[8192];
static int afd = -1, lpid;

bool
NET_CompareBaseAdr(netadr_t *a, netadr_t *b)
{
	return strcmp(a->addr, b->addr) == 0;
}

bool
NET_CompareAdr(netadr_t *a, netadr_t *b)
{
	return strcmp(a->sys, b->sys) == 0;
}

bool
NET_StringToAdr(char *s, netadr_t *a, char *port)
{
	int fd, n;
	char buf[128], *f[4], *p;

	snprint(buf, sizeof buf, "%s/cs", netmtpt);
	if((fd = open(buf, ORDWR)) < 0)
		sysfatal("open: %r");
	if((p = strrchr(s, '!')) == nil && (p = strrchr(s, ':')) == nil)
		p = port == nil ? "27500" : port;
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
	memset(a, 0, sizeof *a);
	strncpy(a->addr, f[1], sizeof(a->addr)-1);
	strncpy(a->srv, f[2], sizeof(a->srv)-1);
	snprint(a->sys, sizeof a->sys, "%s!%s", f[1], f[2]);
	return true;
err:
	fprint(2, "bad cs entry %s", buf);
	return false;
}

static void
getinfo(netadr_t *a)
{
	NetConnInfo *nc;

	if((nc = getnetconninfo(nil, a->fd)) == nil){
		fprint(2, "getnetconninfo: %r\n");
		return;
	}
	strncpy(a->addr, nc->raddr, sizeof(a->addr)-1);
	strncpy(a->srv, nc->rserv, sizeof(a->srv)-1);
	snprint(a->sys, sizeof a->sys, "%s!%s", a->addr, a->srv);
	free(nc);
}

bool
NET_GetPacket(void)
{
	int n, fd;
	netadr_t *a;

	if(svonly && nbrecv(lchan, &fd) > 0){
		for(a=cons; a<cons+nelem(cons); a++)
			if(a->fd < 0)
				break;
		if(a == cons + nelem(cons)){
			close(fd);
			return false;
		}
		a->fd = fd;
		getinfo(a);
	}else{
		for(a=cons; a<cons+nelem(cons); a++)
			if(a->fd >= 0 && flen(a->fd) > 0)
				break;
		if(a == cons + nelem(cons))
			return false;
	}
	if((n = read(a->fd, netbuf, sizeof netbuf)) <= 0){
		fprint(2, "read: %r\n");
		return false;
	}
	net_message.cursize = n;
	net_from = a;
	return true;
}

void
NET_SendPacket(int length, void *data, netadr_t *to)
{
	netadr_t *a;

	for(a=cons; a<cons+nelem(cons); a++)
		if(strcmp(a->sys, to->sys) == 0 || a->fd < 0)
			break;
	if(a == cons + nelem(cons))
		return;
	else if(a->fd < 0){
		memcpy(a, to, sizeof *a);
		if((a->fd = dial(netmkaddr(a->addr, "udp", a->srv), nil, nil, nil)) < 0){
			fprint(2, "dial: %r\n");
			return;
		}
	}
	if(write(a->fd, data, length) != length)
		fprint(2, "write: %r\n");
}

void
NET_Close(netadr_t *to)
{
	netadr_t *a;

	for(a=cons; a<cons+nelem(cons); a++)
		if(strcmp(a->sys, to->sys) == 0){
			close(a->fd);
			a->fd = -1;
		}
}

static void
lproc(void *port)
{
	int fd, lfd;
	char adir[40], ldir[40], data[100];

	snprint(data, sizeof data, "%s/udp!*!%d", netmtpt, *(int *)port);
	if((afd = announce(data, adir)) < 0)
		sysfatal("announce: %r");
	for(;;){
		if((lfd = listen(adir, ldir)) < 0
		|| (fd = accept(lfd, ldir)) < 0)
			break;
		close(lfd);
		send(echan, nil);
		send(lchan, &fd);
	}
}

void
NET_Init(int port)
{
	int n;
	netadr_t *a;

	n = COM_CheckParm("-net");
	if(n && n < com_argc-1)
		if((netmtpt = strdup(com_argv[n+1])) == nil)
			sysfatal("strdup: %r");
	if(svonly && afd < 0){
		if((lchan = chancreate(sizeof(int), 0)) == nil)
			sysfatal("chancreate: %r");
		if((lpid = proccreate(lproc, &port, 8192)) < 0)
			sysfatal("proccreate lproc: %r");
	}
	for(a=cons; a<cons+nelem(cons); a++)
		a->fd = -1;	
	net_message.maxsize = sizeof netbuf;
	net_message.data = netbuf;
}

void
NET_Shutdown(void)
{
	netadr_t *a;

	for(a=cons; a<cons+nelem(cons); a++)
		if(a->fd >= 0){
			close(a->fd);
			a->fd = -1;
		}
	if(afd < 0)
		return;
	close(afd);
	threadkill(lpid);
	afd = -1;
}
