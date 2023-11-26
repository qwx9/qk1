#include "quakedef.h"

cvar_t	*cvar_vars;

/*
============
Cvar_FindVar
============
*/
cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;

	for (var=cvar_vars ; var ; var=var->next)
		if(strcmp(var_name, var->name) == 0)
			return var;
	return nil;
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
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*cvar;
	int			len;

	len = strlen(partial);
	if (!len)
		return nil;
	// check functions
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if(strncmp(partial, cvar->name, len) == 0)
			return cvar->name;
	return nil;
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
	if (Cvar_FindVar (variable->name))
	{
		Con_Printf ("Can't register variable %s, already defined\n", variable->name);
		return;
	}

	// check for overlap with a command
	if (Cmd_Exists (variable->name))
	{
		Con_Printf ("Cvar_RegisterVariable: %s is a command\n", variable->name);
		return;
	}

	// copy the value off, because future sets will Z_Free it
	oldstr = variable->string;
	variable->string = Z_Malloc(strlen(variable->string)+1);
	strcpy(variable->string, oldstr);
	variable->value = atof(variable->string);

	// link the variable in
	variable->next = cvar_vars;
	cvar_vars = variable;
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
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;

	// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		Con_Printf ("\"%s\" is \"%s\"\n", v->name, v->string);
		return true;
	}

	setcvar (v->name, Cmd_Argv(1));
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
}

void
setcvarv(char *k, float v)
{
	char u[32];

	snprint(u, sizeof(u), "%f", v);
	setcvar(k, u);
}
