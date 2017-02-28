#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include "quakedef.h"

/* vid.c */
extern int resized;
extern Point center;

typedef struct Kev Kev;

enum{
	Nbuf = 24
};

struct Kev{
	int key;
	int down;
};
static Channel *kchan;

static cvar_t m_windowed = {"m_windowed", "0", true};
static cvar_t m_filter = {"m_filter", "0", true};
static int mouseon;
static int oldmwin;
static float mx;
static float my;
static float oldmx;
static float oldmy;
static int mb;
static int oldmb;
static int pfd[2];


char *
Sys_ConsoleInput(void)
{
	char buf[256];

	if(cls.state == ca_dedicated){
		if(flen(pfd[1]) < 1)	/* only poll for input */
			return nil;
		if(read(pfd[1], buf, sizeof buf) < 0)
			sysfatal("Sys_ConsoleInput:read: %r");
		return buf;
	}
	return nil;
}

void
Sys_SendKeyEvents(void)
{
	Kev ev;
	int r;

	if(kchan == nil)
		return;

	if(oldmwin != (int)m_windowed.value){
		oldmwin = (int)m_windowed.value;
		IN_Grabm(oldmwin);
	}

	while((r = nbrecv(kchan, &ev)) > 0)
		Key_Event(ev.key, ev.down);
	if(r < 0)
		fprint(2, "Sys_SendKeyEvents: %r\n");
}

void
IN_Commands(void)
{
	int i, r;

	if(!mouseon)
		return;

	for(i=0; i<3; i++){
		r = mb & 1<<i;
		if(r ^ oldmb & 1<<i)
			Key_Event(K_MOUSE1+i, r);
	}
	oldmb = mb;
}

void
IN_Move(usercmd_t *cmd)
{
	if(!mouseon)
		return;
   
	if(m_filter.value){
		mx = (mx + oldmx) * 0.5;
		my = (my + oldmy) * 0.5;
	}
	oldmx = mx;
	oldmy = my;
	mx *= sensitivity.value;
	my *= sensitivity.value;
   
	if(in_strafe.state & 1 || lookstrafe.value && in_mlook.state & 1)
		cmd->sidemove += m_side.value * mx;
	else
		cl.viewangles[YAW] -= m_yaw.value * mx;
	if(in_mlook.state & 1)
		V_StopPitchDrift();
   
	if(in_mlook.state & 1 && ~in_strafe.state & 1){
		cl.viewangles[PITCH] += m_pitch.value * my;
		if(cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if(cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}else{
		if(in_strafe.state & 1 && noclip_anglehack)
			cmd->upmove -= m_forward.value * my;
		else
			cmd->forwardmove -= m_forward.value * my;
	}
	mx = my = 0.0;
}

static int
runetokey(Rune r)
{
	int k = 0;

	switch(r){
	case Kpgup:	k = K_PGUP; break;
	case Kpgdown:	k = K_PGDN; break;
	case Khome:	k = K_HOME; break;
	case Kend:	k = K_END; break;
	case Kleft:	k = K_LEFTARROW; break;
	case Kright:	k = K_RIGHTARROW; break;
	case Kdown:	k = K_DOWNARROW; break;
	case Kup:	k = K_UPARROW; break;
	case Kesc:	k = K_ESCAPE; break;
	case '\n':	k = K_ENTER; break;
	case '\t':	k = K_TAB; break;
	case KF|1:	k = K_F1; break;
	case KF|2:	k = K_F2; break;
	case KF|3:	k = K_F3; break;
	case KF|4:	k = K_F4; break;
	case KF|5:	k = K_F5; break;
	case KF|6:	k = K_F6; break;
	case KF|7:	k = K_F7; break;
	case KF|8:	k = K_F8; break;
	case KF|9:	k = K_F9; break;
	case KF|10:	k = K_F10; break;
	case KF|11:	k = K_F11; break;
	case KF|12:	k = K_F12; break;
	case Kbs:	k = K_BACKSPACE; break;
	case Kdel:	k = K_DEL; break;
	case Kbreak:	k = K_PAUSE; break;
	case Kshift:	k = K_SHIFT; break;
	case Kctl:	k = K_CTRL; break;
	case Kalt:
	case Kaltgr:	k = K_ALT; break;
	case Kins:	k = K_INS; break;
	default:
		if(r < 0x80)
			k = r;
	}
	return k;
}

static void
kproc(void *)
{
	int n, k, fd;
	char buf[128], kdown[128], *s;
	Rune r;
	Kev ev;

	threadsetgrp(THin);

	kdown[1] = kdown[0] = 0;
	if((fd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("open /dev/kbd: %r");
	while((n = read(fd, buf, sizeof buf)) > 0){
		buf[n-1] = 0;
		switch(*buf){
		case 'c':
		default:
			continue;
		case 'k':
			s = buf+1;
			while(*s){
				s += chartorune(&r, s);
				if(utfrune(kdown+1, r) == nil){
					if(k = runetokey(r)){
						ev.key = k;
						ev.down = true;
						if(nbsend(kchan, &ev) < 0)
							fprint(2, "kproc: %r\n");
					}
				}
			}
			break;
		case 'K':
			s = kdown+1;
			while(*s){
				s += chartorune(&r, s);
				if(utfrune(buf+1, r) == nil){
					if(k = runetokey(r)){
						ev.key = k;
						ev.down = false;
						if(nbsend(kchan, &ev) < 0)
							fprint(2, "kproc: %r\n");
					}
				}
			}
			break;
		}
		strcpy(kdown, buf);
	}
}

static void
mproc(void *)
{
	int b, n, nerr, fd;
	char buf[1+5*12];
	float x, y;

	threadsetgrp(THin);

	if((fd = open("/dev/mouse", ORDWR)) < 0)
		sysfatal("open /dev/mouse: %r");

	nerr = 0;
	for(;;){
		if((n = read(fd, buf, sizeof buf)) != 1+4*12){
			if(n < 0 || ++nerr > 10)
				break;
			fprint(2, "mproc: bad count %d not 49: %r\n", n);
			continue;
		}
		nerr = 0;
		switch(*buf){
		case 'r':
			resized = 1;
			/* fall through */
		case 'm':
			if(!mouseon)
				break;

			/* FIXME: this sucks: mouse movement is much smoother
			 * and cursor rarely escapes the window, at the expense
			 * of much more cpu and devmouse use */
			x = atoi(buf+1+0*12) - center.x;
			y = atoi(buf+1+1*12) - center.y;
			b = atoi(buf+1+2*12);

			mx += x;
			my += y;
			if(x != 0.0 ||  y != 0.0)
				fprint(fd, "m%d %d", center.x, center.y);

			mb = b&1 | (b&2)<<1 | (b&4)>>1;
			break;
		}
	}
}

static void
iproc(void *)
{
	int n;
	char s[256];

	threadsetgrp(THin);

	for(;;){
		if((n = read(0, s, sizeof s)) <= 0)
			break;
		s[n-1] = 0;
		if((write(pfd[0], s, n)) != n)
			break;
	}
}

void
IN_Grabm(int on)
{
	static char nocurs[2*4+2*2*16];
	static int fd = -1;

	if(mouseon == on)
		return;
	if(mouseon = on && m_windowed.value){
		if((fd = open("/dev/cursor", ORDWR|OCEXEC)) < 0){
			fprint(2, "IN_Grabm:open: %r\n");
			return;
		}
		write(fd, nocurs, sizeof nocurs);
	}else if(fd >= 0){
		close(fd);
		fd = -1;
	}
}

void
IN_Shutdown(void)
{
	IN_Grabm(0);
	threadintgrp(THin);
	if(pfd[0] > 0)
		close(pfd[0]);
	if(pfd[1] > 0)
		close(pfd[1]);
	if(kchan != nil){
		chanfree(kchan);
		kchan = nil;
	}
}

void
IN_Init(void)
{
	if(cls.state == ca_dedicated){
		if(pipe(pfd) < 0)
			sysfatal("iproc:pipe: %r");
		if(proccreate(iproc, nil, 8192) < 0)
			sysfatal("proccreate iproc: %r");
		return;
	}
	Cvar_RegisterVariable(&m_windowed);
	Cvar_RegisterVariable(&m_filter);
	if((kchan = chancreate(sizeof(Kev), Nbuf)) == nil)
		sysfatal("chancreate kchan: %r");
	if(proccreate(kproc, nil, 8192) < 0)
		sysfatal("proccreate kproc: %r");
	if(COM_CheckParm("-nomouse"))
		return;
	if(proccreate(mproc, nil, 8192) < 0)
		sysfatal("proccreate mproc: %r");
	mx = my = 0.0;
}
