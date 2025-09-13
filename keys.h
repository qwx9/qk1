#pragma once

// these are the key numbers that should be passed to Key_Event
enum {
	K_TAB = '\t',
	K_ENTER = '\r',
	K_ESCAPE = 0x1b,
	K_SPACE = ' ',
	// normal keys should be passed as lowercased ascii

	K_BACKSPACE = 127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,
	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_CAPSLOCK,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,
	K_PAUSE,
	K_PRINTSCREEN,

	// mouse buttons generate virtual keys
	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MOUSE6,
	K_MOUSE7,
	K_MOUSE8,
	K_MWHEELUP,
	K_MWHEELDOWN,
	K_MWHEELLEFT,
	K_MWHEELRIGHT,
};

typedef enum {
	key_game,
	key_console,
	key_message,
	key_menu
} keydest_t;

extern keydest_t key_dest;
extern char *keybindings[256];
extern int key_count; // incremented every key event
extern int key_lastpress;

void Key_Event(int key, bool down);
void Key_Init(void);
void Key_SetBinding(int keynum, char *binding);
