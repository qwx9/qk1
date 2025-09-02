#include "quakedef.h"

cvar_t *
Cvar_FindVar(const char *name)
{
	cvar_t *v;

	v = Con_FindObject(name);
	if(v != nil && iscmd(v))
		v = nil;
	return v;
}

float
Cvar_VariableValue(const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if(!var)
		return 0;
	return atof(var->string);
}

const char *
Cvar_VariableString(const char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar(var_name);
	if(!var)
		return "";
	return var->string;
}

cvar_t *
Cvar_RegisterVariable(cvar_t *v)
{
	char *oldstr;

	// first check to see if it has already been defined
	if(Con_FindObject(v->name) != nil) {
		Con_Printf("Can't register variable %s, already defined\n", v->name);
		return v;
	}

	// copy the value off, because future sets will Z_Free it
	oldstr = v->string;
	v->string = Z_Malloc(strlen(v->string) + 1);
	strcpy(v->string, oldstr);
	v->value = atof(v->string);
	Con_AddObject(v->name, v);
	return v;
}

/* Handles variable inspection and changing from the console */
bool
Cvar_Command(void)
{
	cvar_t *v;

	// check variables
	v = Cvar_FindVar(Cmd_Argv(0));
	if(v == nil)
		return false;

	// perform a variable print or set
	if(Cmd_Argc() == 1) {
		Con_Printf("\"%s\" is \"%s\"\n", v->name, v->string);
		if(v->cb != nil)
			v->cb(v);
		return true;
	}

	setcvar(v->name, Cmd_Argv(1));
	return true;
}

void
setcvar(const char *k, const char *v)
{
	int n;
	cvar_t *cv;

	cv = Cvar_FindVar(k);
	if(cv == nil) {
		Con_DPrintf("setcvar: no such cvar %s\n", k);
		return;
	}
	n = strcmp(cv->string, k);
	Z_Free(cv->string);
	cv->string = Z_Malloc(strlen(v) + 1);
	strcpy(cv->string, v);
	cv->value = atof(v);
	if(n && cv->server && sv.active)
		SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", cv->name, cv->string);
	if(cv->cb != nil)
		cv->cb(cv);
}

void
setcvarv(const char *k, float v)
{
	char u[32];

	snprint(u, sizeof(u), "%f", v);
	setcvar(k, u);
}
