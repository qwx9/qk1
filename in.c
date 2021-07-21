#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

/* vid.c */
extern int resized;
extern Point center;
extern Rectangle grabr;

typedef struct Kev Kev;

enum{
	Nbuf = 20
};
struct Kev{
	int key;
	int down;
};
static Channel *inchan;
static QLock mlck;

static cvar_t m_windowed = {"m_windowed", "1", true};
static cvar_t m_filter = {"m_filter", "0", true};
static int mouseon, fixms;
static int oldmwin;
static float olddx, olddy;
static int mΔx, mΔy, mb, oldmb;

void
conscmd(void)
{
	char *p;

	if(cls.state != ca_dedicated)
		return;
	while(p = nbrecvp(inchan), p != nil){
		Cbuf_AddText(p);
		free(p);
	}
}

static void
cproc(void *)
{
	char *s;
	Biobuf *bf;

	if(bf = Bfdopen(0, OREAD), bf == nil)
		sysfatal("Bfdopen: %r");
	for(;;){
		if(s = Brdstr(bf, '\n', 1), s == nil)
			break;
		if(sendp(inchan, s) < 0){
			free(s);
			break;
		}
	}
	Bterm(bf);
}

void
Sys_SendKeyEvents(void)
{
	Kev ev;
	int r;

	if(cls.state == ca_dedicated)
		return;
	if(oldmwin != (int)m_windowed.value){
		oldmwin = (int)m_windowed.value;
		IN_Grabm(oldmwin);
	}

	while(r = nbrecv(inchan, &ev), r > 0)
		Key_Event(ev.key, ev.down);
	if(r < 0)
		fprint(2, "Sys_SendKeyEvents: %r\n");
}

void
IN_Commands(void)
{
	int b, i, k, r;

	if(!mouseon || cls.state == ca_dedicated)
		return;
	qlock(&mlck);
	b = mb;
	qunlock(&mlck);
	b = b & 0x19 | (b & 2) << 1 | (b & 4) >> 1;
	for(i=0, k=K_MOUSE1; i<5; i++, k++){
		if(i == 3)
			k = K_MWHEELUP;
		r = b & 1<<i;
		if(r ^ oldmb & 1<<i)
			Key_Event(k, r);
	}
	oldmb = b & 7;
}

void
IN_Move(usercmd_t *cmd)
{
	float dx, dy;

	if(!mouseon)
		return;
	qlock(&mlck);
	dx = mΔx;
	dy = mΔy;
	mΔx = 0;
	mΔy = 0;
	qunlock(&mlck);
	if(m_filter.value){
		dx = (dx + olddx) * 0.5;
		dy = (dy + olddy) * 0.5;
	}
	olddx = dx;
	olddy = dy;
	dx *= sensitivity.value;
	dy *= sensitivity.value;
	if(in_strafe.state & 1 || lookstrafe.value && in_mlook.state & 1)
		cmd->sidemove += m_side.value * dx;
	else
		cl.viewangles[YAW] -= m_yaw.value * dx;
	if(in_mlook.state & 1)
		V_StopPitchDrift();
	if(in_mlook.state & 1 && ~in_strafe.state & 1){
		cl.viewangles[PITCH] += m_pitch.value * dy;
		if(cl.viewangles[PITCH] > 80)
			cl.viewangles[PITCH] = 80;
		if(cl.viewangles[PITCH] < -70)
			cl.viewangles[PITCH] = -70;
	}else{
		if(in_strafe.state & 1 && noclip_anglehack)
			cmd->upmove -= m_forward.value * dy;
		else
			cmd->forwardmove -= m_forward.value * dy;
	}
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
	case L'§':	k = '~'; break;
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
	char buf[256], kdown[128], *s, *p;
	Rune r;
	Kev ev, evc;

	fd = open("/dev/kbd", OREAD);
	if(fd < 0)
		sysfatal("open /dev/kbd: %r");
	memset(buf, 0, sizeof buf);
	memset(kdown, 0, sizeof kdown);
	evc.key = K_ENTER;
	evc.down = true;
	for(;;){
		if(buf[0] != 0){
			n = strlen(buf)+1;
			memmove(buf, buf+n, sizeof(buf)-n);
		}
		if(buf[0] == 0){
			if(n = read(fd, buf, sizeof(buf)-1), n <= 0)
				break;
			buf[n-1] = 0;
			buf[n] = 0;
		}
		switch(buf[0]){
		case 'c':
			if(send(inchan, &evc) < 0)
				threadexits(nil);
		default:
			continue;
		case 'k':
			ev.down = true;
			s = buf+1;
			p = kdown+1;
			break;
		case 'K':
			ev.down = false;
			s = kdown+1;
			p = buf+1;
			break;
		}
		while(*s != 0){
			s += chartorune(&r, s);
			if(utfrune(p, r) == nil){
				k = runetokey(r);
				if(k == 0)
					continue;
				ev.key = k;
				if(send(inchan, &ev) < 0)
					threadexits(nil);
				if(ev.down)
					evc.key = k;
			}
		}
		strcpy(kdown, buf);
	}
}

static void
mproc(void *)
{
	int b, n, nerr, fd;
	char buf[1+5*12];
	Point p, o;

	fd = open("/dev/mouse", ORDWR);
	if(fd < 0)
		sysfatal("open /dev/mouse: %r");
	nerr = 0;
	o = center;
	for(;;){
		if(n = read(fd, buf, sizeof buf), n != 1+4*12){
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
			if(fixms){
				fixms = 0;
				goto res;
			}
			p.x = atoi(buf+1+0*12);
			p.y = atoi(buf+1+1*12);
			b = atoi(buf+1+2*12);
			qlock(&mlck);
			mΔx += p.x - o.x;
			mΔy += p.y - o.y;
			mb = b;
			qunlock(&mlck);
			if(!ptinrect(p, grabr)){
		res:
				fprint(fd, "m%d %d", center.x, center.y);
				p = center;
			}
			o = p;
			break;
		}
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
		fd = open("/dev/cursor", ORDWR|OCEXEC);
		if(fd < 0){
			fprint(2, "IN_Grabm:open: %r\n");
			return;
		}
		write(fd, nocurs, sizeof nocurs);
		fixms++;
	}else if(fd >= 0){
		close(fd);
		fd = -1;
	}
}

void
IN_Shutdown(void)
{
	if(inchan != nil)
		chanfree(inchan);
	IN_Grabm(0);
}

void
IN_Init(void)
{
	if(cls.state == ca_dedicated){
		if(inchan = chancreate(sizeof(void *), 2), inchan == nil)
			sysfatal("chancreate: %r");
		if(proccreate(cproc, nil, 8192) < 0)
			sysfatal("proccreate iproc: %r");
		return;
	}
	Cvar_RegisterVariable(&m_windowed);
	Cvar_RegisterVariable(&m_filter);
	if(inchan = chancreate(sizeof(Kev), Nbuf), inchan == nil)
		sysfatal("chancreate: %r");
	if(proccreate(kproc, nil, 8192) < 0)
		sysfatal("proccreate kproc: %r");
	if(proccreate(mproc, nil, 8192) < 0)
		sysfatal("proccreate mproc: %r");
}
