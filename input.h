// input.h -- external (non-keyboard) input devices

#define MAX_PITCH 90
#define MAX_ROLL 50

void IN_Init (void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

void IN_Move (usercmd_t *cmd);
// add additional movement on top of the keyboard move cmd

void IN_ClearStates (void);
// restores all button and position states to defaults

void IN_Grabm (bool on);
// grab or ungrab mouse
