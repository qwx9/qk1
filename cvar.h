#pragma once

typedef struct cvar_s cvar_t;

struct cvar_s {
	char *name;
	char *string;
	bool archive; // set to true to cause it to be saved to vars.rc
	bool hidden;
	float value;
	void (*cb)(cvar_t *);
	bool server; // notifies players when changed
};

// registers a cvar that already has the name, string, and optionally the
// archive elements set.
cvar_t *Cvar_RegisterVariable(cvar_t *variable);

// returns 0 if not defined or non numeric
float Cvar_VariableValue(const char *var_name);

// returns an empty string if not defined
const char *Cvar_VariableString(const char *var_name);

// attempts to match a partial variable name for command line completion
// returns NULL if nothing fits
char *Cvar_CompleteVariable(const char *partial);

// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known
// command.  Returns true if the command was a variable reference that
// was handled. (print or change)
bool Cvar_Command(void);

cvar_t *Cvar_FindVar(const char *var_name);

extern cvar_t *cvar_vars;
