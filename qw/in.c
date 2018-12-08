#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include "quakedef.h"

extern Channel *fuckchan;	/* main fuck receptor */

/* vid_9.c */
extern int resized;
extern Point center;

cvar_t m_windowed = {"m_windowed", "0", true};

static cvar_t m_filter = {"m_filter", "0", true};

static int mouseon, oldmwin;
static float mx, my, oldmx, oldmy;
static int mb, oldmb;

typedef struct Kev Kev;
struct Kev{
	int key;
	int down;
};
enum{
	Nbuf	= 64
};
static Channel *kchan;


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
		fprint(2, "Sys_SendKeyEvents:nbrecv: %r\n");
}

void
IN_Commands(void)
{
	int i;

	if(!mouseon)
		return;

	/* FIXMEGASHIT */
	for(i = 0; i < 3; i++){
		if(mb & 1<<i && ~oldmb & 1<<i)
			Key_Event(K_MOUSE1+i, true);
		if (~mb & 1<<i && oldmb & 1<<i)
			Key_Event(K_MOUSE1+i, false);
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

	if((fd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("open /dev/kbd: %r");
	kdown[0] = kdown[1] = buf[0] = 0;
	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			n = read(fd, buf, sizeof(buf)-1);
			if(n <= 0)
				break;
			buf[n-1] = 0;
			buf[n] = 0;
		}
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
							fprint(2, "kproc:nbsend: %r\n");
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
							fprint(2, "kproc:nbsend: %r\n");
					}
				}
			}
			break;
		}
		strcpy(kdown, buf);
	}
	close(fd);
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
			fprint(2, "mproc:read: bad count %d not 49: %r\n", n);
			if(n < 0 || ++nerr > 10)
				break;
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
	close(fd);
}

void
IN_Shutdown(void)
{
	threadkillgrp(THin);
	IN_Grabm(0);
	if(kchan != nil){
		chanfree(kchan);
		kchan = nil;
	}
}

void
IN_Init(void)
{
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
	IN_Grabm(0);
}
