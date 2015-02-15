#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include "quakedef.h"

cvar_t m_windowed = {"m_windowed","0", true};
cvar_t m_filter = {"m_filter","0", true};
cvar_t m_freelook = {"m_freelook", "0", true};
float oldm_windowed;
qboolean mouse_avail;
int mouse_buttons = 3;
int mouse_buttonstate, mouse_oldbuttonstate;
float mouse_x, mouse_y, old_mouse_x, old_mouse_y;

struct {
	int key;
	int down;
} keyq[64];
int keyq_head = 0;
int keyq_tail = 0;
int mouseactive;
int kpid = -1, mpid = -1;

/* vid_9.c */
extern int config_notify;
extern Point center;
extern Rectangle grabout;


void Sys_SendKeyEvents(void)
{
	/* FIXME: sloppy */
	if(oldm_windowed != m_windowed.value){
		oldm_windowed = m_windowed.value;
		if(!m_windowed.value)
			IN_Grabm(0);
		else
			IN_Grabm(1);
	}

	while(keyq_head != keyq_tail){
		Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down);
		keyq_tail = keyq_tail+1 & 63;
	}
}

void IN_Commands (void)
{
	int i;

	if(!mouse_avail)
		return;

	/* FIXMEGASHIT */
	for(i = 0; i < mouse_buttons; i++){
		if(mouse_buttonstate & 1<<i && ~mouse_oldbuttonstate & 1<<i)
			Key_Event(K_MOUSE1+i, true);
		if (~mouse_buttonstate & 1<<i && mouse_oldbuttonstate & 1<<i)
			Key_Event(K_MOUSE1+i, false);
	}
	mouse_oldbuttonstate = mouse_buttonstate;
}

void IN_Move (usercmd_t *cmd)
{
	if(!mouse_avail)
		return;
   
	if(m_filter.value){
		mouse_x = (mouse_x + old_mouse_x) * 0.5;
		mouse_y = (mouse_y + old_mouse_y) * 0.5;
	}
	old_mouse_x = mouse_x;
	old_mouse_y = mouse_y;
	mouse_x *= sensitivity.value;
	mouse_y *= sensitivity.value;
   
	if(in_strafe.state & 1 || lookstrafe.value && in_mlook.state & 1)
		cmd->sidemove += m_side.value * mouse_x;
	else
		cl.viewangles[YAW] -= m_yaw.value * mouse_x;
	if(m_freelook.value || in_mlook.state & 1)
		V_StopPitchDrift();
   
	if(m_freelook.value || in_mlook.state & 1 && ~in_strafe.state & 1){
		cl.viewangles[PITCH] += m_pitch.value * mouse_y;
		if(cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if(cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}else{
		if(in_strafe.state & 1 && noclip_anglehack)
			cmd->upmove -= m_forward.value * mouse_y;
		else
			cmd->forwardmove -= m_forward.value * mouse_y;
	}
	mouse_x = mouse_y = 0.0;
}

int runetokey (Rune r)
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

void kproc (void)
{
	int n, k, fd;
	char buf[128], kdown[128] = {0}, *s;
	Rune r;

	if((fd = open("/dev/kbd", OREAD)) < 0)
		sysfatal("open /dev/kbd: %r");

	/* FIXMESS */

	while((n = read(fd, buf, sizeof(buf))) > 0){
		buf[n-1] = 0;

		switch(*buf){
		case 'c':
			/*
			chartorune(&r, buf+1);
			if(r){
				keyq[keyq_head].key = runetokey(r);
				keyq[keyq_head].down = true;
				keyq_head = keyq_head+1 & 63;
			}
			*/
			/* fall through */
		default:
			continue;
		case 'k':
			s = buf+1;
			while(*s){
				s += chartorune(&r, s);
				if(utfrune(kdown+1, r) == nil){
					if(k = runetokey(r)){
						keyq[keyq_head].key = k;
						keyq[keyq_head].down = true;
						keyq_head = keyq_head+1 & 63;
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
						keyq[keyq_head].key = k;
						keyq[keyq_head].down = false;
						keyq_head = keyq_head+1 & 63;
					}
				}
			}
			break;
		}
		strcpy(kdown, buf);
	}
	close(fd);
}

void mproc (void)
{
	int b, n, nerr, fd;
	char buf[1+5*12];
	Point m;

	if((fd = open("/dev/mouse", ORDWR)) < 0)
		sysfatal("open /dev/mouse: %r");

	memset(&m, 0, sizeof m);
	nerr = 0;
	for(;;){
		if((n = read(fd, buf, sizeof buf)) != 1+4*12){
			Sys_Warn("mproc:read: bad count %d not 49", n);
			if(n < 0 || ++nerr > 10)
				break;
			continue;
		}
		nerr = 0;
		switch(*buf){
		case 'r':
			config_notify = 1;
			/* fall through */
		case 'm':
			if(!mouseactive)
				break;

			m.x = atoi(buf+1+0*12);
			m.y = atoi(buf+1+1*12);
			//m.buttons = atoi(buf+1+2*12);
			b = atoi(buf+1+2*12);

			/* FIXME: cursor can still get out */
			mouse_x += (float)(m.x - center.x);
			mouse_y += (float)(m.y - center.y);
			if(m_windowed.value)
				fprint(fd, "m%d %d", center.x, center.y);

			mouse_buttonstate = b&1 | (b&2)<<1 | (b&4)>>1;
			break;
		}
	}
	close(fd);
}

void IN_Grabm (int on)
{
	static char nocurs[2*4+2*2*16] = {0};
	static int fd = -1;

	if(mouseactive == on)
		return;
	if(mouseactive = on){
		if((fd = open("/dev/cursor", ORDWR|OCEXEC)) < 0){
			Sys_Warn("IN_Grabm:open");
			return;
		}
		write(fd, nocurs, sizeof(nocurs));
	}else if(fd >= 0){
		close(fd);
		fd = -1;
	}
}

void IN_Shutdown (void)
{
	IN_Grabm(0);
	if(kpid != -1){
		postnote(PNPROC, kpid, "shutdown");
		kpid = -1;
	}
	if(mpid != -1){
		postnote(PNPROC, mpid, "shutdown");
		mpid = -1;
	}
	mouse_avail = 0;
}

void sucks(void *, char *note)
{
	if(!strncmp(note, "sys:", 4))
		IN_Shutdown();	/* FIXME: safe? */
	noted(NDFLT);
}

void IN_Init (void)
{
	int pid;

	Cvar_RegisterVariable(&m_windowed);
	Cvar_RegisterVariable(&m_filter);
	Cvar_RegisterVariable(&m_freelook);
	notify(sucks);
	if(m_windowed.value)
		IN_Grabm(1);
	if((pid = rfork(RFPROC|RFMEM|RFFDG)) == 0){
		kproc();
		exits(nil);
	}
	kpid = pid;
	if(COM_CheckParm("-nomouse"))
		return;
	if((pid = rfork(RFPROC|RFMEM|RFFDG)) == 0){
		mproc();
		exits(nil);
	}
	mpid = pid;
	mouse_x = mouse_y = 0.0;
	/* FIXME: both kind of do the same thing */
	//mouse_avail = mouseactive = 1;
	mouse_avail = 1;
}
