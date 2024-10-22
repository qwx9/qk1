//
// these are the key numbers that should be passed to Key_Event
//
enum {
	K_TAB = '\t',
	K_ENTER = '\r',
	K_ESCAPE = '\e',
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

	// mouse buttons generate virtual keys
	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,
	K_MOUSE4,
	K_MOUSE5,
	K_MWHEELUP,
	K_MWHEELDOWN,

	// joystick buttons
	K_JOY1,
	K_JOY2,
	K_JOY3,
	K_JOY4,

	// aux keys are for multi-buttoned joysticks to generate so they can use
	// the normal binding process
	K_AUX1,
	K_AUX2,
	K_AUX3,
	K_AUX4,
	K_AUX5,
	K_AUX6,
	K_AUX7,
	K_AUX8,
	K_AUX9,
	K_AUX10,
	K_AUX11,
	K_AUX12,
	K_AUX13,
	K_AUX14,
	K_AUX15,
	K_AUX16,
	K_AUX17,
	K_AUX18,
	K_AUX19,
	K_AUX20,
	K_AUX21,
	K_AUX22,
	K_AUX23,
	K_AUX24,
	K_AUX25,
	K_AUX26,
	K_AUX27,
	K_AUX28,
	K_AUX29,
	K_AUX30,
	K_AUX31,
	K_AUX32,
};

typedef enum {key_game, key_console, key_message, key_menu} keydest_t;

extern keydest_t	key_dest;
extern char *keybindings[256];
extern	int		key_count;			// incremented every key event
extern	int		key_lastpress;

void Key_Event (int key, bool down);
void Key_Init (void);
void Key_SetBinding (int keynum, char *binding);
