#include "quakedef.h"
#include <SDL3/SDL.h>

static cvar_t m_windowed = {"m_windowed", "1", true};
static cvar_t m_filter = {"m_filter", "0", true};
static cvar_t m_raw = {"m_raw", "1", true};
static bool mouseon, focuslost = true;
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

	while(SDL_PollEvent(&event)){
		switch(event.type){
		case SDL_EVENT_QUIT:
			Cbuf_AddText("menu_quit\n");
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			vid.resized = true;
			break;
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			Cbuf_AddText("menu_quit\n");
			break;
		case SDL_EVENT_WINDOW_MOUSE_LEAVE:
			focuslost = mouseon;
			IN_Grabm(false);
			break;
		case SDL_EVENT_WINDOW_MOUSE_ENTER:
			IN_Grabm(focuslost);
			break;
		case SDL_EVENT_MOUSE_MOTION:
			if(mouseon){
				dx += event.motion.xrel;
				dy += event.motion.yrel;
			}
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if((b = event.button.button-1) >= 0 && b < nelem(mbuttons))
				Key_Event(mbuttons[b], event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
			break;
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
			if(event.key.repeat)
				break;
			switch(key = event.key.key){
			case SDLK_GRAVE: key = '~'; break;
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
				Key_Event(key, event.key.down);
			break;
		}
	}
}

void
IN_Commands(void)
{
}

static void
m_raw_cb(cvar_t *var)
{
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, var->value > 0 ? "0" : "1");
}

void
IN_Move(usercmd_t *cmd)
{
	if(!mouseon)
		return;

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
IN_Grabm(bool on)
{
	extern SDL_Window *win;
	SDL_SetWindowRelativeMouseMode(win, mouseon = on);
}

void
IN_Shutdown(void)
{
	IN_Grabm(false);
}

void
IN_Init(void)
{
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
	m_raw.cb = m_raw_cb;
	Cvar_RegisterVariable(&m_windowed);
	Cvar_RegisterVariable(&m_filter);
	Cvar_RegisterVariable(&m_raw);
}
