#pragma once

// external (non-keyboard) input devices

#define MAX_PITCH 90
#define MAX_ROLL 50

// oportunity for devices to stick commands on the script buffer
void IN_Commands(void);

// add additional movement on top of the keyboard move cmd
void IN_Move(usercmd_t *cmd);

// restores all button and position states to defaults
void IN_ClearStates(void);

// grab or ungrab mouse
void IN_Grabm(bool on);

void IN_Init(void);
void IN_Shutdown(void);
