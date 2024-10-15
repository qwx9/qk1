#include "quakedef.h"

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *name)
{
	cvar_t *v;

	v = Con_FindObject(name);
	if(v != nil && iscmd(v))
		v = nil;
	return v;
}

/*
============
Cvar_VariableValue
============
*/
float	Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof(var->string);
}


/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}

/*
============
Cvar_RegisterVariable

Adds a freestanding variable to the variable list.
============
*/
void Cvar_RegisterVariable (cvar_t *variable)
{
	char	*oldstr;

	// first check to see if it has already been defined
	if(Con_FindObject(variable->name) != nil){
		Con_Printf("Can't register variable %s, already defined\n", variable->name);
		return;
	}

	// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = Z_Malloc(strlen(variable->string)+1);
	strcpy(variable->string, oldstr);
	variable->value = atof(variable->string);
	Con_AddObject(variable->name, variable);
}

/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
bool	Cvar_Command (void)
{
	cvar_t			*v;

	// check variables
	v = Cvar_FindVar(Cmd_Argv(0));
	if(v == nil)
		return false;

	// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		if(v->cb != nil)
			v->cb(v);
		return true;
	}

	setcvar(v->name, Cmd_Argv(1));
	return true;
}

void
setcvar(char *k, char *v)
{
	int n;
	cvar_t *cv;

	cv = Cvar_FindVar(k);
	if(cv == nil){
		Con_DPrintf("setcvar: no such cvar %s\n", k);
		return;
	}
	n = strcmp(cv->string, k);
	Z_Free(cv->string);
	cv->string = Z_Malloc(strlen(v)+1);
	strcpy(cv->string, v);
	cv->value = atof(v);
	if(n && cv->server && sv.active)
		SV_BroadcastPrintf("\"%s\" changed to \"%s\"\n", cv->name, cv->string);
	if(cv->cb != nil)
		cv->cb(cv);
}

void
setcvarv(char *k, float v)
{
	char u[32];

	snprint(u, sizeof(u), "%f", v);
	setcvar(k, u);
}
