#include "quakedef.h"
#include <SDL.h>

/* vid.c */
extern int resized;

static cvar_t m_windowed = {"m_windowed", "1", true};
static cvar_t m_filter = {"m_filter", "0", true};
static cvar_t m_raw = {"m_raw", "1", true};
static int mouseon, oldmwin, focuslost, oldraw = -1;
static float dx, dy, olddx, olddy;

static int mbuttons[] = {
	K_MOUSE1,
	K_MOUSE3,
	K_MOUSE2,
};

void
conscmd(void)
{
}

void
Sys_SendKeyEvents(void)
{
	SDL_Event event;
	int key, b;

	if(cls.state == ca_dedicated)
		return;
	if(oldmwin != (int)m_windowed.value){
		oldmwin = (int)m_windowed.value;
		IN_Grabm(oldmwin);
	}

	while(SDL_PollEvent(&event)){
		switch(event.type){
		case SDL_QUIT:
			Cbuf_AddText("menu_quit\n");
			break;
		case SDL_WINDOWEVENT:
			switch(event.window.event){
			case SDL_WINDOWEVENT_RESIZED:
				resized = 1;
				break;
			case SDL_WINDOWEVENT_CLOSE:
				Cbuf_AddText("menu_quit\n");
				break;
			case SDL_WINDOWEVENT_LEAVE:
				focuslost = mouseon;
				IN_Grabm(0);
				break;
			case SDL_WINDOWEVENT_ENTER:
				IN_Grabm(focuslost);
				break;
			}
			break;
		case SDL_MOUSEMOTION:
			if(mouseon){
				dx += event.motion.xrel;
				dy += event.motion.yrel;
			}
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if(mouseon && (b = event.button.button-1) >= 0 && b < nelem(mbuttons))
				Key_Event(mbuttons[b], event.type == SDL_MOUSEBUTTONDOWN);
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			switch(key = event.key.keysym.sym){
			case SDLK_BACKQUOTE: key = '~'; break;
			case SDLK_DELETE: key = K_DEL; break;
			case SDLK_BACKSPACE: key = K_BACKSPACE; break;
			case SDLK_F1: key = K_F1; break;
			case SDLK_F2: key = K_F2; break;
			case SDLK_F3: key = K_F3; break;
			case SDLK_F4: key = K_F4; break;
			case SDLK_F5: key = K_F5; break;
			case SDLK_F6: key = K_F6; break;
			case SDLK_F7: key = K_F7; break;
			case SDLK_F8: key = K_F8; break;
			case SDLK_F9: key = K_F9; break;
			case SDLK_F10: key = K_F10; break;
			case SDLK_F11: key = K_F11; break;
			case SDLK_F12: key = K_F12; break;
			case SDLK_PAUSE: key = K_PAUSE; break;
			case SDLK_UP: key = K_UPARROW; break;
			case SDLK_DOWN: key = K_DOWNARROW; break;
			case SDLK_RIGHT: key = K_RIGHTARROW; break;
			case SDLK_LEFT: key = K_LEFTARROW; break;
			case SDLK_RSHIFT:
			case SDLK_LSHIFT: key = K_SHIFT; break;
			case SDLK_RCTRL:
			case SDLK_LCTRL: key = K_CTRL; break;
			case SDLK_RALT:
			case SDLK_LALT: key = K_ALT; break;
			default:
				if(key >= 128)
					key = 0;
				break;
			}
			if(key > 0)
				Key_Event(key, event.key.state);
			break;
		}
	}
}

void
IN_Commands(void)
{
}

void
IN_Move(usercmd_t *cmd)
{
	if(!mouseon)
		return;

	if(oldraw != (int)m_raw.value){
		oldraw = m_raw.value;
		SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, oldraw ? "0" : "1");
	}

	if(m_filter.value){
		dx = (dx + olddx) * 0.5;
		dy = (dy + olddy) * 0.5;
	}
	olddx = dx;
	olddy = dy;
	dx *= sensitivity.value;
	dy *= sensitivity.value;
	if(in_strafe.state & 1 || (lookstrafe.value && in_mlook.state & 1))
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
	dx = 0;
	dy = 0;
}

void
IN_Grabm(int on)
{
	SDL_SetRelativeMouseMode(mouseon = on);
}

void
IN_Shutdown(void)
{
	IN_Grabm(0);
}

void
IN_Init(void)
{
	Cvar_RegisterVariable(&m_windowed);
	Cvar_RegisterVariable(&m_filter);
	Cvar_RegisterVariable(&m_raw);
}
