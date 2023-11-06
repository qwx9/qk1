#include "quakedef.h"

dprograms_t		*progs;
dfunction_t		*pr_functions;
char			*pr_strings;
static int		pr_strings_size;
ddef_t			*pr_fielddefs;
ddef_t			*pr_globaldefs;
dstatement_t	*pr_statements;
globalvars_t	*pr_global_struct;
float			*pr_globals;			// same as pr_global_struct
int				pr_edict_size;	// in bytes

int type_size[8] = {
	[ev_void] = 1,
	[ev_string] = sizeof(string_t)/4,
	[ev_float] = 1,
	[ev_vector] = 3,
	[ev_entity] = 1,
	[ev_field] = 1,
	[ev_function] = sizeof(func_t)/4,
	[ev_pointer] = sizeof(void *)/4,
};

ddef_t *ED_FieldAtOfs (int ofs);
qboolean	ED_ParseEpair (void *base, ddef_t *key, char *s);

cvar_t	nomonsters = {"nomonsters", "0"};
cvar_t	gamecfg = {"gamecfg", "0"};
cvar_t	scratch1 = {"scratch1", "0"};
cvar_t	scratch2 = {"scratch2", "0"};
cvar_t	scratch3 = {"scratch3", "0"};
cvar_t	scratch4 = {"scratch4", "0"};
cvar_t	savedgamecfg = {"savedgamecfg", "0", true};
cvar_t	saved1 = {"saved1", "0", true};
cvar_t	saved2 = {"saved2", "0", true};
cvar_t	saved3 = {"saved3", "0", true};
cvar_t	saved4 = {"saved4", "0", true};
cvar_t	pr_checkextension = {"pr_checkextension", "1"};

#define	MAX_FIELD_LEN	64
#define GEFV_CACHESIZE	2

typedef struct {
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache	gefvCache[GEFV_CACHESIZE] = {{nil, ""}, {nil, ""}};

#define MAX_PRSTR 1024
char **prstr;
int num_prstr;
int max_prstr;

#define MAX_PRTEMPSTR 1024
#define PRTEMPSTR_SIZE 1024
char *prtempstr;
int num_prtempstr;

char *
PR_StrTmp(void)
{
	return &prtempstr[PRTEMPSTR_SIZE * (num_prtempstr++ & MAX_PRTEMPSTR)];
}

int
PR_CopyStrTmp(char *s)
{
	char *t = PR_StrTmp();
	snprint(t, PRTEMPSTR_SIZE, "%s", s);
	return PR_SetStr(t);
}

int
PR_StrSlot(void)
{
	if(num_prstr >= max_prstr){
		max_prstr *= 2;
		prstr = realloc(prstr, max_prstr*sizeof(*prstr));
	}
	return num_prstr++;
}

int
PR_SetStr(char *s)
{
	int i;

	if(s == nil)
		return 0;
	if(s >= pr_strings && s < pr_strings+pr_strings_size-1)
		return s - pr_strings;
	for(i = 0; i < num_prstr; i++){
		if(s == prstr[i])
			return -1-i;
	}
	i = PR_StrSlot();
	prstr[i] = s;
	return -1-i;
}

char *
PR_Str(int i)
{
	if(i >= 0 && i < pr_strings_size)
		return pr_strings+i;
	if(i < 0 && i >= -num_prstr && prstr[-1-i] != nil)
		return prstr[-1-i];
	Host_Error("PR_Str: invalid offset %d", i);
	return "";
}

/*
=================
ED_ClearEdict

Sets everything to nil
=================
*/
void ED_ClearEdict (edict_t *e)
{
	memset (&e->v, 0, progs->entityfields * 4);
	e->free = false;
}

/*
=================
ED_Alloc

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *ED_Alloc (void)
{
	int			i;
	edict_t		*e;

	for ( i=svs.maxclients+1 ; i<sv.num_edicts ; i++)
	{
		e = EDICT_NUM(i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if (e->free && ( e->freetime < 2 || sv.time - e->freetime > 0.5 ) )
		{
			ED_ClearEdict (e);
			return e;
		}
	}
	
	if (i == MAX_EDICTS)
		fatal ("ED_Alloc: no free edicts");
		
	sv.num_edicts++;
	e = EDICT_NUM(i);
	ED_ClearEdict (e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void ED_Free (edict_t *ed)
{
	SV_UnlinkEdict (ed);		// unlink from world bsp

	ed->free = true;
	ed->v.model = 0;
	ed->v.takedamage = 0;
	ed->v.modelindex = 0;
	ed->v.colormap = 0;
	ed->v.skin = 0;
	ed->v.frame = 0;
	VectorCopy (vec3_origin, ed->v.origin);
	VectorCopy (vec3_origin, ed->v.angles);
	ed->v.nextthink = -1;
	ed->v.solid = 0;
	ed->alpha = DEFAULT_ALPHA;
	ed->freetime = sv.time;
}

//===========================================================================

/*
============
ED_GlobalAtOfs
============
*/
ddef_t *ED_GlobalAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return nil;
}

/*
============
ED_FieldAtOfs
============
*/
ddef_t *ED_FieldAtOfs (int ofs)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (def->ofs == ofs)
			return def;
	}
	return nil;
}

/*
============
ED_FindField
============
*/
ddef_t *ED_FindField (char *name)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numfielddefs ; i++)
	{
		def = &pr_fielddefs[i];
		if (!strcmp(PR_Str(def->s_name),name) )
			return def;
	}
	return nil;
}


/*
============
ED_FindGlobal
============
*/
ddef_t *ED_FindGlobal (char *name)
{
	ddef_t		*def;
	int			i;
	
	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		def = &pr_globaldefs[i];
		if (!strcmp(PR_Str(def->s_name),name) )
			return def;
	}
	return nil;
}


/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction (char *name)
{
	dfunction_t		*func;
	int				i;
	
	for (i=0 ; i<progs->numfunctions ; i++)
	{
		func = &pr_functions[i];
		if (!strcmp(PR_Str(func->s_name),name) )
			return func;
	}
	return nil;
}


eval_t *GetEdictFieldValue(edict_t *ed, char *field)
{
	ddef_t			*def;
	int				i;
	static int		rep = 0;

	for (i=0 ; i<GEFV_CACHESIZE ; i++)
	{
		if (!strcmp(field, gefvCache[i].field))
		{
			def = gefvCache[i].pcache;
			goto Done;
		}
	}

	def = ED_FindField (field);

	if (strlen(field) < MAX_FIELD_LEN)
	{
		gefvCache[rep].pcache = def;
		strcpy (gefvCache[rep].field, field);
		rep ^= 1;
	}

Done:
	if (def == nil)
		return nil;

	return (eval_t *)((char *)&ed->v + def->ofs*4);
}


/*
============
PR_ValueString

Returns a string describing *data in a type specific manner
=============
*/
char *PR_ValueString (etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;
	
	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		sprint (line, "%s", PR_Str(val->string));
		break;
	case ev_entity:	
		sprint (line, "entity %d", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)) );
		break;
	case ev_function:
		f = pr_functions + val->function;
		sprint (line, "%s()", PR_Str(f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		sprint (line, ".%s", PR_Str(def->s_name));
		break;
	case ev_void:
		sprint (line, "void");
		break;
	case ev_float:
		sprint (line, "%5.1f", val->_float);
		break;
	case ev_vector:
		sprint (line, "'%5.1f %5.1f %5.1f'", val->vector[0], val->vector[1], val->vector[2]);
		break;
	case ev_pointer:
		sprint (line, "pointer");
		break;
	default:
		sprint (line, "bad type %d", type);
		break;
	}
	
	return line;
}

/*
============
PR_UglyValueString

Returns a string describing *data in a type specific manner
Easier to parse than PR_ValueString
=============
*/
char *PR_UglyValueString (etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;
	
	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		sprint (line, "%s", PR_Str(val->string));
		break;
	case ev_entity:	
		sprint (line, "%d", NUM_FOR_EDICT(PROG_TO_EDICT(val->edict)));
		break;
	case ev_function:
		f = pr_functions + val->function;
		sprint (line, "%s", PR_Str(f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs ( val->_int );
		sprint (line, "%s", PR_Str(def->s_name));
		break;
	case ev_void:
		sprint (line, "void");
		break;
	case ev_float:
		sprint (line, "%f", val->_float);
		break;
	case ev_vector:
		sprint (line, "%f %f %f", val->vector[0], val->vector[1], val->vector[2]);
		break;
	default:
		sprint (line, "bad type %d", type);
		break;
	}
	
	return line;
}

/*
============
PR_GlobalString

Returns a string with a description and the contents of a global,
padded to 20 field width
============
*/
char *PR_GlobalString (int ofs)
{
	char	*s;
	int		i;
	ddef_t	*def;
	void	*val;
	static char	line[128];
	
	val = (void *)&pr_globals[ofs];
	def = ED_GlobalAtOfs(ofs);
	if (!def)
		sprint (line,"%d(?)", ofs);
	else
	{
		s = PR_ValueString (def->type, val);
		sprint (line,"%d(%s)%s", ofs, PR_Str(def->s_name), s);
	}
	
	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}

char *PR_GlobalStringNoContents (int ofs)
{
	int		i;
	ddef_t	*def;
	static char	line[128];
	
	def = ED_GlobalAtOfs(ofs);
	if (!def)
		sprint (line,"%d(?)", ofs);
	else
		sprint (line,"%d(%s)", ofs, PR_Str(def->s_name));
	
	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");
		
	return line;
}


/*
=============
ED_Print

For debugging
=============
*/
void ED_Print (edict_t *ed)
{
	int		l;
	ddef_t	*d;
	int		*v;
	int		i, j;
	char	*name;
	int		type;

	if (ed->free)
	{
		Con_Printf ("FREE\n");
		return;
	}

	Con_Printf("\nEDICT %d:\n", NUM_FOR_EDICT(ed));
	for (i=1 ; i<progs->numfielddefs ; i++)
	{
		d = &pr_fielddefs[i];
		name = PR_Str(d->s_name);
		if (name[strlen(name)-2] == '_')
			continue;	// skip _x, _y, _z vars
			
		v = (int *)((char *)&ed->v + d->ofs*4);

	// if the value is still all 0, skip the field
		if((type = d->type & ~DEF_SAVEGLOBAL) >= nelem(type_size))
			continue;
		for (j=0 ; j<type_size[type] ; j++)
			if (v[j])
				break;
		if (j == type_size[type])
			continue;
	
		Con_Printf ("%s",name);
		l = strlen (name);
		while (l++ < 15)
			Con_Printf (" ");

		Con_Printf ("%s\n", PR_ValueString(d->type, (eval_t *)v));		
	}
}

void ED_PrintNum (int ent)
{
	ED_Print (EDICT_NUM(ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void ED_PrintEdicts (void)
{
	int		i;
	
	Con_Printf ("%d entities\n", sv.num_edicts);
	for (i=0 ; i<sv.num_edicts ; i++)
		ED_PrintNum (i);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edicy
=============
*/
void ED_PrintEdict_f (void)
{
	int		i;
	
	i = atoi(Cmd_Argv(1));
	if (i >= sv.num_edicts)
	{
		Con_Printf("Bad edict number\n");
		return;
	}
	ED_PrintNum (i);
}

/*
=============
ED_Count

For debugging
=============
*/
void ED_Count (void)
{
	int		i;
	edict_t	*ent;
	int		active, models, solid, step;

	active = models = solid = step = 0;
	for (i=0 ; i<sv.num_edicts ; i++)
	{
		ent = EDICT_NUM(i);
		if (ent->free)
			continue;
		active++;
		if (ent->v.solid)
			solid++;
		if (ent->v.model)
			models++;
		if (ent->v.movetype == MOVETYPE_STEP)
			step++;
	}

	Con_Printf ("num_edicts:%3d\n", sv.num_edicts);
	Con_Printf ("active    :%3d\n", active);
	Con_Printf ("view      :%3d\n", models);
	Con_Printf ("touch     :%3d\n", solid);
	Con_Printf ("step      :%3d\n", step);

}

/*
==============================================================================

					ARCHIVING GLOBALS

FIXME: need to tag constants, doesn't really work
==============================================================================
*/

/*
=============
ED_ParseGlobals
=============
*/
void ED_ParseGlobals (char *data)
{
	char	keyname[64];
	ddef_t	*key;

	while (1)
	{	
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			fatal ("ED_ParseGlobals: EOF without closing brace");

		strcpy (keyname, com_token);

	// parse value	
		data = COM_Parse (data);
		if (!data)
			fatal ("ED_ParseGlobals: EOF without closing brace");

		if (com_token[0] == '}')
			fatal ("ED_ParseGlobals: closing brace without data");

		key = ED_FindGlobal (keyname);
		if (!key)
		{
			Con_Printf ("ED_ParseGlobals: '%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair ((void *)pr_globals, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
string_t ED_NewString (char *string)
{
	char	*new, *new_p;
	int		i,l, slot;
	
	l = strlen(string) + 1;
	new = Hunk_Alloc (l);
	new_p = new;
	slot = PR_StrSlot();

	for (i=0 ; i< l ; i++)
	{
		if (string[i] == '\\' && i < l-1)
		{
			i++;
			if (string[i] == 'n')
				*new_p++ = '\n';
			else
				*new_p++ = '\\';
		}
		else
			*new_p++ = string[i];
	}
	prstr[slot] = new;
	
	return -1-slot;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
qboolean	ED_ParseEpair (void *base, ddef_t *key, char *s)
{
	ddef_t	*def;
	char	*v, *w;
	void	*d;
	dfunction_t	*func;
	int		i;
	
	d = (void *)((int *)base + key->ofs);
	
	switch (key->type & ~DEF_SAVEGLOBAL)
	{
	case ev_string:
		*(string_t *)d = ED_NewString(s);
		break;
		
	case ev_float:
		*(float *)d = atof(s);
		break;
		
	case ev_vector:
		memset(d, 0, sizeof(float)*3);
		w = s;
		for(i = 0; i < 3; i++, w = v){
			((float*)d)[i] = strtod(w, &v);
			if(w == v)
				break;
			while(*v == ',' || *v == ' ')
				v++;
		}
		for(; i < 3; i++)
			((float*)d)[i] = 0;
		break;
		
	case ev_entity:
		*(int *)d = EDICT_TO_PROG(EDICT_NUM(atoi (s)));
		break;
		
	case ev_field:
		def = ED_FindField (s);
		if (!def)
		{
			Con_Printf ("Can't find field %s\n", s);
			return false;
		}
		*(int *)d = G_INT(def->ofs);
		break;
	
	case ev_function:
		func = ED_FindFunction (s);
		if (!func)
		{
			Con_Printf ("Can't find function %s\n", s);
			return false;
		}
		*(func_t *)d = func - pr_functions;
		break;
		
	default:
		break;
	}
	return true;
}

/*
====================
ED_ParseEdict

Parses an edict out of the given string, returning the new position
ed should be a properly initialized empty edict.
Used for initial level load and for savegames.
====================
*/
char *ED_ParseEdict (char *data, edict_t *ent)
{
	ddef_t		*key;
	qboolean	anglehack;
	qboolean	init;
	char		keyname[256];
	int			n;

	init = false;

// clear it
	if (ent != sv.edicts)	// hack
		memset (&ent->v, 0, progs->entityfields * 4);

// go through all the dictionary pairs
	while (1)
	{	
	// parse key
		data = COM_Parse (data);
		if (com_token[0] == '}')
			break;
		if (!data)
			fatal ("ED_ParseEdict: EOF without closing brace");
		
		// anglehack is to allow QuakeEd to write single scalar angles
		// and allow them to be turned into vectors. (FIXME...)
		if (!strcmp(com_token, "angle"))
		{
			strcpy (com_token, "angles");
			anglehack = true;
		}
		else
			anglehack = false;

		// FIXME: change light to _light to get rid of this hack
		if (!strcmp(com_token, "light"))
			strcpy (com_token, "light_lev");	// hack for single light def

		strcpy (keyname, com_token);

		// another hack to fix heynames with trailing spaces
		n = strlen(keyname);
		while (n && keyname[n-1] == ' ')
		{
			keyname[n-1] = 0;
			n--;
		}

	// parse value	
		data = COM_Parse (data);
		if (!data)
			fatal ("ED_ParseEdict: EOF without closing brace");

		if (com_token[0] == '}')
			fatal ("ED_ParseEdict: closing brace without data");

		init = true;	

	// keynames with a leading underscore are used for utility comments,
	// and are immediately discarded by quake
		if (keyname[0] == '_')
			continue;
		
		key = ED_FindField (keyname);
		if (!key)
		{
			if(strcmp(keyname, "alpha") == 0)
				ent->alpha = f2alpha(atof(com_token));
			else
				Con_Printf ("ED_ParseEdict: '%s' is not a field\n", keyname);
			continue;
		}

		if (anglehack){
			char	temp[32];
			strcpy (temp, com_token);
			sprint (com_token, "0 %s 0", temp);
		}

		if (!ED_ParseEpair ((void *)&ent->v, key, com_token))
			Host_Error ("ED_ParseEdict: parse error");
	}

	if (!init)
		ent->free = true;

	return data;
}


/*
================
ED_LoadFromFile

The entities are directly placed in the array, rather than allocated with
ED_Alloc, because otherwise an error loading the map would have entity
number references out of order.

Creates a server's entity / program execution context by
parsing textual entity definitions out of an ent file.

Used for both fresh maps and savegame loads.  A fresh map would also need
to call ED_CallSpawnFunctions () to let the objects initialize themselves.
================
*/
void ED_LoadFromFile (char *data)
{	
	edict_t		*ent;
	int			inhibit;
	dfunction_t	*func;
	
	ent = nil;
	inhibit = 0;
	pr_global_struct->time = sv.time;
// parse ents
	while (1)
	{
// parse the opening brace	
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			fatal ("ED_LoadFromFile: found %s when expecting {",com_token);

		if (!ent)
			ent = EDICT_NUM(0);
		else
			ent = ED_Alloc ();
		data = ED_ParseEdict (data, ent);

// remove things from different skill levels or deathmatch
		if (deathmatch.value)
		{
			if (((int)ent->v.spawnflags & SPAWNFLAG_NOT_DEATHMATCH))
			{
				ED_Free (ent);	
				inhibit++;
				continue;
			}
		}
		else if ((current_skill == 0 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_EASY))
				|| (current_skill == 1 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_MEDIUM))
				|| (current_skill >= 2 && ((int)ent->v.spawnflags & SPAWNFLAG_NOT_HARD)) )
		{
			ED_Free (ent);	
			inhibit++;
			continue;
		}

//
// immediately call spawn function
//
		if (!ent->v.classname)
		{
			Con_Printf ("No classname for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

	// look for the spawn function
		func = ED_FindFunction ( PR_Str(ent->v.classname) );
		if (!func)
		{
			Con_Printf ("No spawn function for:\n");
			ED_Print (ent);
			ED_Free (ent);
			continue;
		}

		pr_global_struct->self = EDICT_TO_PROG(ent);
		PR_ExecuteProgram (func - pr_functions);
	}	

	Con_DPrintf("%d entities inhibited\n", inhibit);
}

typedef struct extra_field_t extra_field_t;

struct extra_field_t {
	int type;
	char *name;
};

static extra_field_t extra_fields[] = {
	{ev_float, "alpha"},
};

static void
PR_FieldDefs(ddef_t *in)
{
	extra_field_t *e;
	ddef_t *d;
	int i, n;

	// allocate to fit *all* extra fields, if needed, and copy over
	n = progs->numfielddefs;
	for(i = 0; i < nelem(extra_fields); i++)
		n += extra_fields[i].type == ev_vector ? 4 : 1;
	d = malloc(n * sizeof(*in));
	memmove(d, in, progs->numfielddefs * sizeof(*in));
	free(pr_fielddefs);
	pr_fielddefs = d;

	// convert endianess of fields that loaded from disk
	for(i = 0 ; i < progs->numfielddefs; i++, d++){
		d->type = LittleShort(d->type);
		if(d->type & DEF_SAVEGLOBAL)
			fatal("PR_FieldDefs: d->type & DEF_SAVEGLOBAL");
		d->ofs = LittleShort(d->ofs);
		d->s_name = LittleLong(d->s_name);
	}

	// add missing extra fields
	d = &pr_fielddefs[progs->numfielddefs];
	for(i = 0, e = extra_fields; i < nelem(extra_fields); i++, e++){
		if(ED_FindField(e->name) != nil)
			continue;
		d->type = e->type;
		d->s_name = ED_NewString(e->name);
		d->ofs = progs->numfielddefs++;
		d++;
		if(e->type == ev_vector){
			for(n = 0; n < 3; n++){
				d->type = ev_float;
				d->s_name = ED_NewString(va("%s_%c", e->name, 'x'+n));
				d->ofs = progs->numfielddefs++;
				d++;
			}
		}
		progs->entityfields += type_size[e->type];
	}
}

/*
===============
PR_LoadProgs
===============
*/
void PR_LoadProgs (void)
{
	int i, n;

// flush the non-C variable lookup cache
	for (i=0 ; i<GEFV_CACHESIZE ; i++)
		gefvCache[i].field[0] = 0;

	if(prstr == nil){
		max_prstr = MAX_PRSTR;
		prstr = malloc(max_prstr*sizeof(*prstr));
	}
	if(prtempstr == nil)
		prtempstr = malloc(MAX_PRTEMPSTR*PRTEMPSTR_SIZE);

	memset(prstr, 0, max_prstr*sizeof(*prstr));
	memset(prtempstr, 0, MAX_PRTEMPSTR*PRTEMPSTR_SIZE);
	num_prstr = 0;
	num_prtempstr = 0;
	PR_SetStr("");

	initcrc();

	progs = loadhunklmp("progs.dat", &n);
	if(progs == nil)
		fatal("PR_LoadProgs: %r");
	Con_DPrintf("Programs occupy %dK.\n", n/1024);

	for (i=0 ; i<n ; i++)
		crc (((byte *)progs)[i]);

// byte swap the header
	for (i=0 ; i<sizeof(*progs)/4 ; i++)
		((int *)progs)[i] = LittleLong ( ((int *)progs)[i] );		

	if (progs->version != PROG_VERSION)
		fatal ("progs.dat has wrong version number (%d should be %d)", progs->version, PROG_VERSION);
	if (progs->crc != PROGHEADER_CRC)
		fatal ("progs.dat system vars have been modified, progdefs.h is out of date");

	pr_functions = (dfunction_t *)((byte *)progs + progs->ofs_functions);
	pr_strings = (char *)progs + progs->ofs_strings;
	pr_strings_size = progs->numstrings;
	pr_globaldefs = (ddef_t *)((byte *)progs + progs->ofs_globaldefs);
	pr_statements = (dstatement_t *)((byte *)progs + progs->ofs_statements);
	pr_global_struct = (globalvars_t *)((byte *)progs + progs->ofs_globals);
	pr_globals = (float *)pr_global_struct;
		
// byte swap the lumps
	for (i=0 ; i<progs->numstatements ; i++)
	{
		pr_statements[i].op = LittleShort(pr_statements[i].op);
		pr_statements[i].a = LittleShort(pr_statements[i].a);
		pr_statements[i].b = LittleShort(pr_statements[i].b);
		pr_statements[i].c = LittleShort(pr_statements[i].c);
	}

	for (i=0 ; i<progs->numfunctions; i++)
	{
	pr_functions[i].first_statement = LittleLong (pr_functions[i].first_statement);
	pr_functions[i].parm_start = LittleLong (pr_functions[i].parm_start);
	pr_functions[i].s_name = LittleLong (pr_functions[i].s_name);
	pr_functions[i].s_file = LittleLong (pr_functions[i].s_file);
	pr_functions[i].numparms = LittleLong (pr_functions[i].numparms);
	pr_functions[i].locals = LittleLong (pr_functions[i].locals);
	}	

	for (i=0 ; i<progs->numglobaldefs ; i++)
	{
		pr_globaldefs[i].type = LittleShort (pr_globaldefs[i].type);
		pr_globaldefs[i].ofs = LittleShort (pr_globaldefs[i].ofs);
		pr_globaldefs[i].s_name = LittleLong (pr_globaldefs[i].s_name);
	}

	for (i=0 ; i<progs->numglobals ; i++)
		((int *)pr_globals)[i] = LittleLong (((int *)pr_globals)[i]);

	PR_FieldDefs((ddef_t *)((byte *)progs + progs->ofs_fielddefs));
	pr_edict_size = progs->entityfields * 4 + sizeof(edict_t) - sizeof(entvars_t);
}


/*
===============
PR_Init
===============
*/
void PR_Init (void)
{
	Cmd_AddCommand ("edict", ED_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED_PrintEdicts);
	Cmd_AddCommand ("edictcount", ED_Count);
	Cmd_AddCommand ("profile", PR_Profile_f);
	Cvar_RegisterVariable (&nomonsters);
	Cvar_RegisterVariable (&gamecfg);
	Cvar_RegisterVariable (&scratch1);
	Cvar_RegisterVariable (&scratch2);
	Cvar_RegisterVariable (&scratch3);
	Cvar_RegisterVariable (&scratch4);
	Cvar_RegisterVariable (&savedgamecfg);
	Cvar_RegisterVariable (&saved1);
	Cvar_RegisterVariable (&saved2);
	Cvar_RegisterVariable (&saved3);
	Cvar_RegisterVariable (&saved4);
	Cvar_RegisterVariable (&pr_checkextension);
}

edict_t *EDICT_NUM(int n)
{
	if (n < 0 || n >= sv.max_edicts)
		fatal ("EDICT_NUM: bad number %d", n);
	return (edict_t *)((byte *)sv.edicts+ (n)*pr_edict_size);
}

int NUM_FOR_EDICT(edict_t *e)
{
	int		b;
	
	b = (byte *)e - (byte *)sv.edicts;
	b = b / pr_edict_size;
	
	if (b < 0 || b >= sv.num_edicts)
		fatal ("NUM_FOR_EDICT: bad pointer");
	return b;
}
