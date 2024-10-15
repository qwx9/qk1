#include "quakedef.h"

const int type_size[8] = {
	[ev_void] = 1,
	[ev_string] = sizeof(string_t)/4,
	[ev_float] = 1,
	[ev_vector] = 3,
	[ev_entity] = 1,
	[ev_field] = 1,
	[ev_function] = sizeof(func_t)/4,
	[ev_pointer] = sizeof(void *)/4,
};

bool	ED_ParseEpair (pr_t *pr, void *base, ddef_t *key, char *s);

cvar_t	nomonsters = {"nomonsters", "0"};

static cvar_t scratch1 = {"scratch1", "0"};
static cvar_t scratch2 = {"scratch2", "0"};
static cvar_t scratch3 = {"scratch3", "0"};
static cvar_t scratch4 = {"scratch4", "0"};
static cvar_t saved1 = {"saved1", "0", true};
static cvar_t saved2 = {"saved2", "0", true};
static cvar_t saved3 = {"saved3", "0", true};
static cvar_t saved4 = {"saved4", "0", true};
static cvar_t gamecfg = {"gamecfg", "0"};
static cvar_t savedgamecfg = {"savedgamecfg", "0", true};
static cvar_t pr_checkextension = {"pr_checkextension", "1"};

enum {
	MAX_PRSTR = 1024,
	MAX_PRTEMPSTR = 1024,
	PRTEMPSTR_SIZE = 1024,

	MAX_FIELD_LEN = 64,
	GEFV_CACHESIZE = 2,
};

typedef struct {
	ddef_t	*pcache;
	char	field[MAX_FIELD_LEN];
} gefv_cache;

static gefv_cache	gefvCache[GEFV_CACHESIZE] = {{nil, ""}, {nil, ""}};

char *
PR_StrTmp(pr_t *pr)
{
	return &pr->tempstr[PRTEMPSTR_SIZE * (pr->num_tempstr++ % MAX_PRTEMPSTR)];
}

int
PR_CopyStrTmp(pr_t *pr, char *s)
{
	char *t = PR_StrTmp(pr);
	snprint(t, PRTEMPSTR_SIZE, "%s", s);
	return PR_SetStr(pr, t);
}

static int
PR_StrSlot(pr_t *pr)
{
	if(pr->num_str >= pr->max_str){
		pr->str = Hunk_Double(pr->str);
		pr->max_str *= 2;
	}
	return pr->num_str++;
}

int
PR_SetStr(pr_t *pr, char *s)
{
	int i;

	if(s == nil || pr->strings == nil)
		return 0;
	if(s >= pr->strings && s < pr->strings+pr->strings_size-1)
		return s - pr->strings;
	for(i = 0; i < pr->num_str; i++){
		if(s == pr->str[i])
			return -1-i;
	}
	i = PR_StrSlot(pr);
	pr->str[i] = s;
	return -1-i;
}

char *
PR_Str(pr_t *pr, int i)
{
	if(i >= 0 && i < pr->strings_size)
		return pr->strings+i;
	if(i < 0 && i >= -pr->num_str && pr->str[-1-i] != nil)
		return pr->str[-1-i];
	Con_Printf("PR_Str: invalid offset (strings=%d num_str=%d): %d", pr->strings_size, pr->num_str, i);
	return nil;
}

/*
=================
ED_ClearEdict

Sets everything to nil
=================
*/
static void
ED_ClearEdict(pr_t *pr, edict_t *e)
{
	memset(&e->v, 0, pr->entityfields * 4);
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
edict_t *
ED_Alloc(pr_t *pr)
{
	int			i;
	edict_t		*e;

	for(i = svs.maxclients+1; i<pr->num_edicts ; i++){
		e = EDICT_NUM(pr, i);
		// the first couple seconds of server time can involve a lot of
		// freeing and allocating, so relax the replacement policy
		if(e->free && (e->freetime < 2 || sv.time - e->freetime > 0.5)){
			ED_ClearEdict(pr, e);
			return e;
		}
	}

	if(i == pr->max_edicts)
		fatal("ED_Alloc: no free edicts");

	pr->num_edicts++;
	e = EDICT_NUM(pr, i);
	ED_ClearEdict(pr, e);

	return e;
}

/*
=================
ED_Free

Marks the edict as free
FIXME: walk all entities and NULL out references to this entity
=================
*/
void
ED_Free(edict_t *ed)
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
static ddef_t *
ED_GlobalAtOfs(pr_t *pr, int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<pr->numglobaldefs ; i++)
	{
		def = &pr->globaldefs[i];
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
ddef_t *
ED_FieldAtOfs(pr_t *pr, int ofs)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<pr->numfielddefs ; i++)
	{
		def = &pr->fielddefs[i];
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
static ddef_t *
ED_FindField(pr_t *pr, char *name)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<pr->numfielddefs ; i++)
	{
		def = &pr->fielddefs[i];
		if (!strcmp(PR_Str(pr, def->s_name),name) )
			return def;
	}
	return nil;
}


/*
============
ED_FindGlobal
============
*/
static ddef_t *
ED_FindGlobal(pr_t *pr, char *name)
{
	ddef_t		*def;
	int			i;

	for (i=0 ; i<pr->numglobaldefs ; i++)
	{
		def = &pr->globaldefs[i];
		if (!strcmp(PR_Str(pr, def->s_name),name) )
			return def;
	}
	return nil;
}


/*
============
ED_FindFunction
============
*/
dfunction_t *ED_FindFunction (pr_t *pr, char *name)
{
	dfunction_t		*func;
	int				i;

	for (i=0 ; i<pr->numfunctions ; i++)
	{
		func = &pr->functions[i];
		if (!strcmp(PR_Str(pr, func->s_name),name) )
			return func;
	}
	return nil;
}


eval_t *GetEdictFieldValue(pr_t *pr, edict_t *ed, char *field)
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

	def = ED_FindField (pr, field);

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
static char *
PR_ValueString(pr_t *pr, etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		sprint (line, "%s", PR_Str(pr, val->string));
		break;
	case ev_entity:
		sprint (line, "entity %d", NUM_FOR_EDICT(pr, PROG_TO_EDICT(pr, val->edict)) );
		break;
	case ev_function:
		f = pr->functions + val->function;
		sprint (line, "%s()", PR_Str(pr, f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs (pr, val->_int );
		sprint (line, ".%s", PR_Str(pr, def->s_name));
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
char *PR_UglyValueString (pr_t *pr, etype_t type, eval_t *val)
{
	static char	line[256];
	ddef_t		*def;
	dfunction_t	*f;

	type &= ~DEF_SAVEGLOBAL;

	switch (type)
	{
	case ev_string:
		sprint (line, "%s", PR_Str(pr, val->string));
		break;
	case ev_entity:
		sprint (line, "%d", NUM_FOR_EDICT(pr, PROG_TO_EDICT(pr, val->edict)));
		break;
	case ev_function:
		f = pr->functions + val->function;
		sprint (line, "%s", PR_Str(pr, f->s_name));
		break;
	case ev_field:
		def = ED_FieldAtOfs (pr, val->_int );
		sprint (line, "%s", PR_Str(pr, def->s_name));
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
char *
PR_GlobalString(pr_t *pr, int ofs)
{
	char	*s;
	int		i;
	ddef_t	*def;
	void	*val;
	static char	line[128];

	val = (void *)&pr->globals[ofs];
	def = ED_GlobalAtOfs(pr, ofs);
	if (!def)
		sprint (line,"%d(?)", ofs);
	else
	{
		s = PR_ValueString(pr, def->type, val);
		sprint(line,"%d(%s)%s", ofs, PR_Str(pr, def->s_name), s);
	}

	i = strlen(line);
	for ( ; i<20 ; i++)
		strcat (line," ");
	strcat (line," ");

	return line;
}

char *
PR_GlobalStringNoContents(pr_t *pr, int ofs)
{
	int		i;
	ddef_t	*def;
	static char	line[128];

	def = ED_GlobalAtOfs(pr, ofs);
	if (!def)
		sprint (line,"%d(?)", ofs);
	else
		sprint (line,"%d(%s)", ofs, PR_Str(pr, def->s_name));

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
void ED_Print (pr_t *pr, edict_t *ed)
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

	Con_Printf("\nEDICT %d:\n", NUM_FOR_EDICT(pr, ed));
	for (i=1 ; i<pr->numfielddefs ; i++)
	{
		d = &pr->fielddefs[i];
		name = PR_Str(pr, d->s_name);
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

		Con_Printf ("%s\n", PR_ValueString(pr, d->type, (eval_t *)v));
	}
}

void ED_PrintNum (pr_t *pr, int ent)
{
	ED_Print (pr, EDICT_NUM(pr, ent));
}

/*
=============
ED_PrintEdicts

For debugging, prints all the entities in the current server
=============
*/
void
ED_PrintEdicts(cmd_t *c)
{
	int		i;

	USED(c);
	Con_Printf ("%d entities\n", sv.pr->num_edicts);
	for (i=0 ; i<sv.pr->num_edicts ; i++)
		ED_PrintNum(sv.pr, i);
}

/*
=============
ED_PrintEdict_f

For debugging, prints a single edicy
=============
*/
static void
ED_PrintEdict_f(cmd_t *c)
{
	int		i;

	USED(c);
	i = atoi(Cmd_Argv(1));
	if (i >= sv.pr->num_edicts)
	{
		Con_Printf("Bad edict number\n");
		return;
	}
	ED_PrintNum (sv.pr, i);
}

/*
=============
ED_Count

For debugging
=============
*/
static void
ED_Count(cmd_t *c)
{
	int		i;
	edict_t	*ent;
	int		active, models, solid, step;

	USED(c);
	active = models = solid = step = 0;
	for (i=0 ; i<sv.pr->num_edicts ; i++)
	{
		ent = EDICT_NUM(sv.pr, i);
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

	Con_Printf ("num_edicts:%3d\n", sv.pr->num_edicts);
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
void ED_ParseGlobals (pr_t *pr, char *data)
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

		key = ED_FindGlobal (pr, keyname);
		if (!key)
		{
			Con_Printf ("ED_ParseGlobals: '%s' is not a global\n", keyname);
			continue;
		}

		if (!ED_ParseEpair (pr, (void *)pr->globals, key, com_token))
			Host_Error ("ED_ParseGlobals: parse error");
	}
}

//============================================================================


/*
=============
ED_NewString
=============
*/
string_t ED_NewString (pr_t *pr, char *string)
{
	char	*new, *new_p;
	int		i,l, slot;

	l = strlen(string) + 1;
	new = Hunk_Alloc(l);
	new_p = new;
	slot = PR_StrSlot(pr);

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
	pr->str[slot] = new;

	return -1-slot;
}


/*
=============
ED_ParseEval

Can parse either fields or globals
returns false if error
=============
*/
bool	ED_ParseEpair (pr_t *pr, void *base, ddef_t *key, char *s)
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
		*(string_t *)d = ED_NewString(pr, s);
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
		*(int *)d = EDICT_TO_PROG(pr, EDICT_NUM(pr, atoi (s)));
		break;

	case ev_field:
		def = ED_FindField (pr, s);
		if (!def)
		{
			Con_Printf ("Can't find field %s\n", s);
			return false;
		}
		*(int *)d = G_INT(pr, def->ofs);
		break;

	case ev_function:
		func = ED_FindFunction (pr, s);
		if (!func)
		{
			Con_Printf ("Can't find function %s\n", s);
			return false;
		}
		*(func_t *)d = func - pr->functions;
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
char *ED_ParseEdict (pr_t *pr, char *data, edict_t *ent)
{
	ddef_t		*key;
	bool	anglehack;
	bool	init;
	char		keyname[256];
	int			n;

	init = false;

	// clear it
	if (ent != pr->edicts)	// hack
		memset (&ent->v, 0, pr->entityfields * 4);

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

		if(strcmp(keyname, "taregtname") == 0) // you'd be surprised (rrp)
			strcpy(keyname, "targetname");

		key = ED_FindField (pr, keyname);
		if (!key)
		{
			if(strcmp(keyname, "alpha") == 0){
				ent->alpha = f2alpha(atof(com_token));
			}else if(strcmp(keyname, "renderamt") == 0){
				ent->alpha = atoi(com_token);
				if(ent->alpha == ZERO_ALPHA)
					ent->alpha++;
				else if(ent->alpha == 0)
					ent->alpha = ZERO_ALPHA;
			}else if(strcmp(keyname, "rendermode") == 0){
				switch(atoi(com_token)){
				case 5:
					ent->v.effects = (int)ent->v.effects | EF_ADDITIVE;
					break;
				default:
					/* FIXME(sigrid): support more? */
					break;
				}
			}else{
				Con_Printf ("ED_ParseEdict: '%s' is not a field\n", keyname);
			}
			continue;
		}

		if (anglehack){
			char	temp[32];
			strcpy (temp, com_token);
			sprint (com_token, "0 %s 0", temp);
		}

		if (!ED_ParseEpair (pr, (void *)&ent->v, key, com_token))
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
void ED_LoadFromFile (pr_t *pr, char *data)
{
	edict_t		*ent;
	int			inhibit;
	dfunction_t	*func;

	ent = nil;
	inhibit = 0;
	pr->global_struct->time = sv.time;
	// parse ents
	while (1)
	{
		// parse the opening brace
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			fatal ("ED_LoadFromFile: found %s when expecting {", com_token);

		if (!ent)
			ent = EDICT_NUM(pr, 0);
		else
			ent = ED_Alloc(pr);
		data = ED_ParseEdict (pr, data, ent);

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

		// immediately call spawn function
		if (!ent->v.classname)
		{
			Con_Printf ("No classname for:\n");
			ED_Print (pr, ent);
			ED_Free (ent);
			continue;
		}

		// look for the spawn function
		func = ED_FindFunction (pr,  PR_Str(pr, ent->v.classname) );
		if (!func)
		{
			Con_Printf ("No spawn function for:\n");
			ED_Print (pr, ent);
			ED_Free (ent);
			continue;
		}

		SV_SignonFrame();
		pr->global_struct->self = EDICT_TO_PROG(pr, ent);
		PR_ExecuteProgram (pr, func - pr->functions);
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
PR_FieldDefs(pr_t *pr, byte *in, int num)
{
	extra_field_t *e;
	ddef_t *d;
	int i, n;

	// allocate to fit *all* extra fields, if needed, and copy over
	pr->numfielddefs = n = num;
	for(i = 0; i < nelem(extra_fields); i++)
		n += extra_fields[i].type == ev_vector ? 4 : 1;
	pr->fielddefs = d = Hunk_Alloc(n * sizeof(*d));
	for(i = 0; i < pr->numfielddefs; i++){
		d[i].type = le16u(in);
		d[i].ofs = le16u(in);
		d[i].s_name = le32(in);
	}

	// add missing extra fields
	d += pr->numfielddefs;
	for(i = 0, e = extra_fields; i < nelem(extra_fields); i++, e++){
		if(ED_FindField(pr, e->name) != nil)
			continue;
		d->type = e->type;
		d->s_name = ED_NewString(pr, e->name);
		d->ofs = pr->numfielddefs++;
		d++;
		if(e->type == ev_vector){
			for(n = 0; n < 3; n++){
				d->type = ev_float;
				d->s_name = ED_NewString(pr, va("%s_%c", e->name, 'x'+n));
				d->ofs = pr->numfielddefs++;
				d++;
			}
		}
		pr->entityfields += type_size[e->type];
	}
}

/*
===============
PR_LoadProgs
===============
*/
pr_t *PR_LoadProgs (char *name)
{
	int i, n, version, hdrcrc;
	byte *in, *in0;
	dfunction_t *f;
	dproglump_t *pl;
	pr_t *pr;
	static const int elsz[NUM_PR_LUMPS] = {
		[PR_LUMP_STATEMENTS] = 4*2,
		[PR_LUMP_GLOBALDEFS] = 2*2+1*4,
		[PR_LUMP_FIELDDEFS] = 2*2+1*4,
		[PR_LUMP_FUNCTIONS] = 7*4+MAX_PARMS*1,
		[PR_LUMP_STRINGS] = 1,
		[PR_LUMP_GLOBALS] = 4,
	};
	dproglump_t lumps[NUM_PR_LUMPS];

	// flush the non-C variable lookup cache
	for (i=0 ; i<GEFV_CACHESIZE ; i++)
		gefvCache[i].field[0] = 0;

	pr = Hunk_Alloc(sizeof(*pr));

	pr->max_str = MAX_PRSTR;
	pr->str = Hunk_Alloc(pr->max_str*sizeof(*pr->str));
	pr->num_str = 0;
	pr->tempstr = Hunk_Alloc(MAX_PRTEMPSTR*PRTEMPSTR_SIZE);
	pr->num_tempstr = 0;
	PR_SetStr(pr, "");

	in = in0 = loadhunklmp(name, &n);
	if(in == nil){
err:
		fatal("PR_LoadProgs: %s", lerr());
	}
	if(n < 15*4){
		werrstr("too small");
		goto err;
	}

	version = le32(in);
	if(version != PROG_VERSION){
		werrstr("wrong version number (%d should be %d)", version, PROG_VERSION);
		goto err;
	}

	hdrcrc = le32(in);
	initcrc();
	for(i = 0; i < n; i++)
		crc(in0[i]);
	if(hdrcrc != PROGHEADER_CRC){
		werrstr("system vars have been modified, progdefs.h is out of date");
		goto err;
	}

	for(i = 0, pl = lumps; i < nelem(lumps); i++, pl++){
		pl->off = le32(in);
		pl->num = le32(in);
		if(pl->num <= 0 || pl->off < 2*4+nelem(lumps)*2*4+4 || pl->off+pl->num*elsz[i] > n){
			werrstr("invalid lump: off=%d num=%d elsz=%d total=%d", pl->off, pl->num, elsz[i], n);
			goto err;
		}
	}
	if((pr->entityfields = le32(in)) <= 0){
		werrstr("invalid entityfields");
		goto err;
	}

	pl = &lumps[PR_LUMP_FUNCTIONS];
	pr->functions = f = Hunk_Alloc(pl->num * sizeof(*f));
	pr->numfunctions = pl->num;
	for(i = 0, in = in0 + pl->off; i < pl->num; i++, f++){
		f->first_statement = le32(in);
		f->parm_start = le32(in);
		f->locals = le32(in);
		f->profile = le32(in);
		f->s_name = le32(in);
		f->s_file = le32(in);
		f->numparms = le32(in);
		memmove(f->parm_size, in, MAX_PARMS);
		in += MAX_PARMS;
	}

	pl = &lumps[PR_LUMP_STATEMENTS];
	pr->statements = Hunk_Alloc(pl->num * sizeof(*pr->statements));
	for(i = 0, in = in0 + pl->off; i < pl->num; i++){
		pr->statements[i].op = le16u(in);
		pr->statements[i].a = le16(in);
		pr->statements[i].b = le16(in);
		pr->statements[i].c = le16(in);
	}

	pl = &lumps[PR_LUMP_STRINGS];
	pr->strings = Hunk_Alloc(pl->num + 1);
	memmove(pr->strings, in0 + pl->off, pl->num);
	pr->strings[pr->strings_size = pl->num] = 0;

	pl = &lumps[PR_LUMP_GLOBALDEFS];
	pr->globaldefs = Hunk_Alloc(pl->num * sizeof(*pr->globaldefs));
	pr->numglobaldefs = pl->num;
	for(i = 0, in = in0 + pl->off; i < pl->num; i++){
		// FIXME(sigrid): verify all of these as well
		pr->globaldefs[i].type = le16u(in);
		pr->globaldefs[i].ofs = le16u(in);
		pr->globaldefs[i].s_name = le32(in);
	}

	pl = &lumps[PR_LUMP_GLOBALS];
	pr->globals = (float *)(pr->global_struct = Hunk_Alloc(pl->num * 4));
	for(i = 0, in = in0 + pl->off; i < pl->num; i++)
		((int*)pr->globals)[i] = le32(in);

	pl = &lumps[PR_LUMP_FIELDDEFS];
	PR_FieldDefs(pr, in0 + pl->off, pl->num);
	pr->edict_size = pr->entityfields*4 + sizeof(edict_t) - sizeof(entvars_t);
	// 
	pr->edict_size = (pr->edict_size + 7) & ~7;

	Con_DPrintf("Programs occupy %dK.\n", n/1024);

	pr->max_edicts = MAX_EDICTS;
	pr->edicts = Hunk_Alloc(pr->max_edicts*pr->edict_size);

	PR_InitSV(pr);

	return pr;
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

edict_t *EDICT_NUM(pr_t *pr, int n)
{
	if (n < 0 || n >= pr->max_edicts)
		fatal ("EDICT_NUM: bad number %d", n);
	return (edict_t *)((byte *)pr->edicts+ (n)*pr->edict_size);
}

int NUM_FOR_EDICT(pr_t *pr, edict_t *e)
{
	int		b;

	b = (byte *)e - (byte *)pr->edicts;
	b = b / pr->edict_size;

	if (b < 0 || b >= pr->num_edicts)
		fatal ("NUM_FOR_EDICT: bad pointer");
	return b;
}
