#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <thread.h>
#include <bio.h>
#include <ndb.h>
#include <ip.h>
#include "quakedef.h"

extern Channel *fuckchan;	/* main fuck receptor */

sizebuf_t net_message;
netadr_t laddr = { .ip{ 192, 168, 1, 138} };		/* 0.0.0.0:0 */
netadr_t net_from;

typedef struct Conmsg Conmsg;
typedef struct Conlist Conlist;

enum{
	Nbuf	= 64,
	Bufsz	= 8192,
	Hdrsz	= 16+16+16+2+2,	/* sizeof Udphdr w/o padding */
};
static int cfd = -1, ufd = -1;
static char srv[6];
static uchar netbuf[Bufsz];
static Channel *udpchan;
static QLock cnlock;

struct Conlist{
	Conlist *p;
	uchar	u[IPaddrlen+2];
	char	addr[IPaddrlen*2+8+6];	/* ipv6 + separators + port in decimal */
	int	dfd;
	Udphdr	h;
};
Conlist *cnroot;

struct Conmsg{
	Conlist *p;
	int	n;
	uchar	buf[Bufsz];
};

static void	uproc(void *);
static void	dproc(void *);
static void	cninit(void);
static Conlist*	cnins(int, char *, uchar *, Udphdr *);
static Conlist*	cnfind(char *);
static void	cndel(Conlist *);
static void	cnnuke(void);


netadr_t	net_from;
int			net_socket;			// non blocking, for receives
int			net_send_socket;	// blocking, for sends


qboolean
NET_CompareBaseAdr(netadr_t a, netadr_t b)
{
	return a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3];
}

qboolean
NET_CompareAdr(netadr_t a, netadr_t b)
{
	return a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2] && a.ip[3] == b.ip[3] && a.port == b.port;
}

char *
NET_AdrToString(netadr_t a)
{
	static char s[256];

	seprint(s, s+sizeof s, "%ud.%ud.%ud.%ud:%hud", a.ip[0], a.ip[1], a.ip[2], a.ip[3], BigShort(a.port));
	return s;
}

char *
NET_BaseAdrToString(netadr_t a)
{
	static char s[256];

	seprint(s, s+sizeof s, "%ud.%ud.%ud.%ud", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);
	return s;
}

/*
idnewt
idnewt:28000
192.246.40.70
192.246.40.70:28000
*/
qboolean
NET_StringToAdr(char *addr, netadr_t *a)	/* assumes IPv4 */
{
	int i;
	char s[256], *p, *pp;
	Ndbtuple *t, *nt;

	strncpy(s, addr, sizeof s);
	s[sizeof(s)-1] = 0;

	/* FIXME: arbitrary length strings: ip.h limit? */
	if((p = strrchr(s, ':')) != nil){
		*p++ = '\0';
		a->port = BigShort(atoi(p));
	}

	/* FIXME: sys */
	if(strcmp(ipattr(s), "dom") == 0){
		if((t = dnsquery(nil, s, "ip")) == nil){
			fprint(2, "NET_StringToAdr:dnsquery %s: %r\n", s);
			return 0;
		}

		for(nt = t; nt != nil; nt = nt->entry)
			if(!strcmp(nt->attr, "ip")){
				strncpy(s, nt->val, sizeof(s)-1);
				break;
			}	
		ndbfree(t);
	}

	/* FIXMEGASHIT */
	for(i = 0, pp = s; i < IPv4addrlen; i++){
		if((p = strchr(pp, '.')) != nil)
			*p++ = 0;
		a->ip[i] = atoi(pp);
		pp = p;
	}
	return true;
}

static void
cninit(void)
{
	if(cnroot != nil)
		return;
	if((cnroot = malloc(sizeof *cnroot)) == nil)
		sysfatal("cninit:malloc: %r");
	cnroot->p = cnroot;
	memset(cnroot->u, 0, sizeof cnroot->u);
	memset(cnroot->addr, 0, sizeof cnroot->addr);
	cnroot->dfd = -1;
}

static Conlist *
cnins(int fd, char *addr, uchar *u, Udphdr *h)
{
	Conlist *p, *l;

	l = cnroot;
	if((p = malloc(sizeof *p)) == nil)
		sysfatal("cnins:malloc: %r");

	strncpy(p->addr, addr, sizeof p->addr);
	memcpy(p->u, u, sizeof p->u);
	p->dfd = fd;
	if(h != nil)
		memcpy(&p->h, h, sizeof p->h);
	p->p = l->p;
	l->p = p;
	return p;
}

static Conlist *
cnfind(char *raddr)
{
	Conlist *p = cnroot->p;

	while(p != cnroot){
		if(!strncmp(p->addr, raddr, strlen(p->addr)))
			return p;
		p = p->p;
	}
	return nil;
}

static void
cndel(Conlist *p)
{
	Conlist *l = cnroot;

	while(l->p != p){
		l = l->p;
		if(l == cnroot)
			sysfatal("cndel: bad unlink: cnroot 0x%p node 0x%p\n", cnroot, p);
	}
	l->p = p->p;
	if(p->dfd != ufd && p->dfd != -1)
		close(p->dfd);
	free(p);
}

static void
cnnuke(void)
{
	Conlist *p, *l = cnroot;

	if(cnroot == nil)
		return;
	do{
		p = l;
		l = l->p;
		if(p->dfd != -1)
			close(p->dfd);
		free(p);
	}while(l != cnroot);
	cnroot = nil;
}

qboolean
NET_GetPacket(void)
{
	int n;
	Conmsg m;

	if(cfd == -1)
		return false;

	if((n = nbrecv(udpchan, &m)) < 0)
		sysfatal("NET_GetPacket:nbrecv: %r");
	if(n == 0)
		return false;
	memcpy(net_from.ip, m.p->u+12, 4);
	net_from.port = m.p->u[17] << 8 | m.p->u[16];
	net_message.cursize = m.n;
	memcpy(netbuf, m.buf, m.n);

	return true;
}

void
NET_SendPacket(int length, void *data, netadr_t to)
{
	int fd;
	char *addr, *s, *lport;
	uchar b[Bufsz+Hdrsz], u[IPaddrlen+2];
	Conlist *p;

	if(cfd == -1)
		return;

	addr = NET_AdrToString(to);
	qlock(&cnlock);
	p = cnfind(addr);
	qunlock(&cnlock);
	if(p != nil){
		fd = p->dfd;
		if(fd == ufd){
			memcpy(b, &p->h, Hdrsz);
			memcpy(b+Hdrsz, data, length);
			write(fd, b, length+Hdrsz);
			return;
		}else if(write(fd, data, length) != length)
			sysfatal("NET_SendPacket:write: %r");
	}else{
		lport = strrchr(addr, ':');
		*lport++ = '\0';
		s = netmkaddr(addr, "udp", lport);
		if((fd = dial(s, srv, nil, nil)) < 0)
			sysfatal("NET_SendPacket:dial: %r");

		memcpy(u, v4prefix, sizeof v4prefix);
		memcpy(u+IPv4off, to.ip, IPv4addrlen);
		u[16] = to.port;
		u[17] = to.port >> 8;
		*(lport-1) = ':';
		qlock(&cnlock);
		p = cnins(fd, addr, u, nil);
		qunlock(&cnlock);

		if(proccreate(dproc, p, 8196*4) < 0)
			sysfatal("NET_SendPacket:proccreate: %r");
	}
	if(write(fd, data, length) != length)
		sysfatal("NET_SendPacket:write: %r");
}

static void
dproc(void *me)
{
	int n, fd;
	Conmsg m;
	Conlist *p;

	threadsetgrp(THnet);

	m.p = p = me;
	fd = p->dfd;

	for(;;){
		if((n = read(fd, m.buf, sizeof m.buf)) <= 0)
			break;
		m.n = n;
		if(send(udpchan, &m) < 0)
			sysfatal("uproc:send: %r\n");
		if(svonly && nbsend(fuckchan, nil) < 0)
			sysfatal("fuckchan %r\n");
	}
	fprint(2, "dproc %d: %r\n", threadpid(threadid()));
	cndel(me);
}

static void
uproc(void *)
{
	int n;
	uchar udpbuf[Bufsz+Hdrsz], u[IPaddrlen+2];
	char a[IPaddrlen*2+8+6];
	Udphdr h;
	Conmsg m;
	Conlist *p;

	threadsetgrp(THnet);

	for(;;){
		if((n = read(ufd, udpbuf, sizeof udpbuf)) <= 0)
			sysfatal("uproc:read: %r");

		memcpy(&h, udpbuf, Hdrsz);
		memcpy(u, h.raddr, IPaddrlen);
		memcpy(u+IPaddrlen, h.rport, 2);
		snprint(a, sizeof a, "%ud.%ud.%ud.%ud:%hud", u[12], u[13], u[14], u[15], u[16]<<8 | u[17]);
		qlock(&cnlock);
		if((p = cnfind(a)) == nil)
			p = cnins(ufd, a, u, &h);
		qunlock(&cnlock);
		m.p = p;

		if(n - Hdrsz < 0){	/* FIXME */
			m.n = n;
			memcpy(m.buf, udpbuf, m.n);
		}else{
			m.n = n - Hdrsz;
			memcpy(m.buf, udpbuf+Hdrsz, m.n);
		}
		if(send(udpchan, &m) < 0)
			sysfatal("uproc:send: %r\n");
		if(svonly && nbsend(fuckchan, nil) < 0)
			sysfatal("fucking chan refused the fuck: %r\n");
	}
}

void
NET_Init(int port)
{
	char data[64], adir[40];

	if(cfd != -1)
		return;

	cninit();
	snprint(srv, sizeof srv, "%hud", port);
	if((cfd = announce(netmkaddr("*", "udp", srv), adir)) < 0)
		sysfatal("announce udp!*!%s: %r", srv);
	if(fprint(cfd, "headers") < 0)
		sysfatal("NET_Init: failed to set header mode: %r");
	snprint(data, sizeof data, "%s/data", adir);
	if((ufd = open(data, ORDWR)) < 0)
		sysfatal("open: %r");
	if((udpchan = chancreate(sizeof(Conmsg), Nbuf)) == nil)
		sysfatal("chancreate udpchan: %r");
	if(proccreate(uproc, udpchan, 8192*4) < 0)
		sysfatal("proccreate: %r");

	net_message.maxsize = sizeof netbuf;
	net_message.data = netbuf;
}

void
NET_Shutdown(void)
{
	threadkillgrp(THnet);
	cnnuke();
	if(udpchan != nil){
		chanfree(udpchan);
		udpchan = nil;
	}
	if(cfd != -1){
		close(cfd);
		cfd = -1;
	}
	if(ufd != -1){
		close(ufd);
		ufd = -1;
	}
}
