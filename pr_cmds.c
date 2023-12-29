#include "quakedef.h"

#define	RETURN_EDICT(pr, e) (((int *)pr->globals)[OFS_RETURN] = EDICT_TO_PROG(pr, e))

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/

static char *
PF_VarString(pr_t *pr, int first)
{
	int		i;
	static char out[256];

	out[0] = 0;
	for (i=first ; i<pr->argc ; i++)
	{
		strcat (out, G_STRING(pr, (OFS_PARM0+i*3)));
	}
	return out;
}


/*
=================
PF_errror

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
static void
PF_error(pr_t *pr)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(pr, 0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n", PR_Str(pr, pr->xfunction->s_name),s);
	ed = PROG_TO_EDICT(pr, pr->global_struct->self);
	ED_Print (pr, ed);

	Host_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
static void
PF_objerror(pr_t *pr)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(pr, 0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_Str(pr, pr->xfunction->s_name),s);
	ed = PROG_TO_EDICT(pr, pr->global_struct->self);
	ED_Print (pr, ed);
	ED_Free (ed);

	Host_Error ("Program error");
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
static void
PF_makevectors(pr_t *pr)
{
	AngleVectors (G_VECTOR(pr, OFS_PARM0), pr->global_struct->v_forward, pr->global_struct->v_right, pr->global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
static void
PF_setorigin(pr_t *pr)
{
	edict_t	*e;
	float	*org;

	e = G_EDICT(pr, OFS_PARM0);
	org = G_VECTOR(pr, OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}

static void
SetMinMaxSize(pr_t *pr, edict_t *e, float *min, float *max, bool rotate)
{
	float	*angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;

	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PR_RunError (pr, "backwards mins/maxs");

	rotate = false;		// FIXME: implement rotation properly again

	if (!rotate)
	{
		VectorCopy (min, rmin);
		VectorCopy (max, rmax);
	}
	else
	{
		// find min / max for rotations
		angles = e->v.angles;

		a = angles[1]/180 * M_PI;

		xvector[0] = cosf(a);
		xvector[1] = sinf(a);
		yvector[0] = -sinf(a);
		yvector[1] = cosf(a);

		VectorCopy (min, bounds[0]);
		VectorCopy (max, bounds[1]);

		rmin[0] = rmin[1] = rmin[2] = Q_MAXFLOAT;
		rmax[0] = rmax[1] = rmax[2] = -Q_MAXFLOAT;

		for (i=0 ; i<= 1 ; i++)
		{
			base[0] = bounds[i][0];
			for (j=0 ; j<= 1 ; j++)
			{
				base[1] = bounds[j][1];
				for (k=0 ; k<= 1 ; k++)
				{
					base[2] = bounds[k][2];

					// transform the point
					transformed[0] = xvector[0]*base[0] + yvector[0]*base[1];
					transformed[1] = xvector[1]*base[0] + yvector[1]*base[1];
					transformed[2] = base[2];

					for (l=0 ; l<3 ; l++)
					{
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}

	// set derived values
	VectorCopy (rmin, e->v.mins);
	VectorCopy (rmax, e->v.maxs);
	VectorSubtract (max, min, e->v.size);

	SV_LinkEdict (e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
static void
PF_setsize(pr_t *pr)
{
	edict_t	*e;
	float	*min, *max;

	e = G_EDICT(pr, OFS_PARM0);
	min = G_VECTOR(pr, OFS_PARM1);
	max = G_VECTOR(pr, OFS_PARM2);
	SetMinMaxSize (pr, e, min, max, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
static void
PF_setmodel(pr_t *pr)
{
	edict_t	*e;
	char	*m, **check;
	model_t	*mod;
	int		i;

	e = G_EDICT(pr, OFS_PARM0);
	m = G_STRING(pr, OFS_PARM1);

	// check to see if model was properly precached
	for (i=0, check = sv.model_precache ; *check ; i++, check++)
		if (!strcmp(*check, m))
			break;
	if (!*check)
		PR_RunError (pr, "no precache: %s\n", m);
	e->v.model = PR_SetStr(pr, m);
	e->v.modelindex = i; //SV_ModelIndex (m);

	mod = sv.models[ (int)e->v.modelindex];  // Mod_ForName (m, true);

	if(!mod)
		SetMinMaxSize (pr, e, vec3_origin, vec3_origin, true);
	else
		SetMinMaxSize (pr, e, mod->mins, mod->maxs, true);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
static void
PF_bprint(pr_t *pr)
{
	char		*s;

	s = PF_VarString(pr, 0);
	SV_BroadcastPrintf ("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
static void
PF_sprint(pr_t *pr)
{
	char		*s;
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(pr, OFS_PARM0);
	s = PF_VarString(pr, 1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteChar (&client->message,svc_print);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
static void
PF_centerprint(pr_t *pr)
{
	char		*s;
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(pr, OFS_PARM0);
	s = PF_VarString(pr, 1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteChar (&client->message,svc_centerprint);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
static void
PF_normalize(pr_t *pr)
{
	float	*v;
	vec3_t	newvalue;
	double	ln, a, b, c;

	v = G_VECTOR(pr, OFS_PARM0);
	a = v[0];
	b = v[1];
	c = v[2];
	ln = sqrtf(a*a + b*b + c*c);

	if(ln == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else{
		newvalue[0] = a / ln;
		newvalue[1] = b / ln;
		newvalue[2] = c / ln;
	}

	VectorCopy(newvalue, G_VECTOR(pr, OFS_RETURN));
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
static void
PF_vlen(pr_t *pr)
{
	float	*v;
	double	a, b, c;

	v = G_VECTOR(pr, OFS_PARM0);
	a = v[0];
	b = v[1];
	c = v[2];
	G_FLOAT(pr, OFS_RETURN) = sqrtf(a*a + b*b + c*c);
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
static void
PF_vectoyaw(pr_t *pr)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(pr, OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = atan2f(value1[1], value1[0]) * 180.0 / M_PI;
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(pr, OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
static void
PF_vectoangles(pr_t *pr)
{
	float	*value1;
	double	forward;
	double	yaw, pitch;

	value1 = G_VECTOR(pr, OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = atan2f(value1[1], value1[0]) * 180.0 / M_PI;
		if (yaw < 0)
			yaw += 360;

		forward = sqrtf(value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = atan2f(value1[2], forward) * 180 / M_PI;
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(pr, OFS_RETURN+0) = pitch;
	G_FLOAT(pr, OFS_RETURN+1) = yaw;
	G_FLOAT(pr, OFS_RETURN+2) = 0;
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
static void
PF_random(pr_t *pr)
{
	static double xmax;
	static long xand;

	if(xand == 0){
		if((RAND_MAX & ((ulong)RAND_MAX+1)) == 0){
			xmax = (ulong)RAND_MAX+1;
			xand = RAND_MAX;
		}else{
			xmax = 0x8000;
			xand = 0x7fff;
		}
	}

	G_FLOAT(pr, OFS_RETURN) = ((double)(rand() & xand) + 0.5) / xmax;
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
static void
PF_particle(pr_t *pr)
{
	float		*org, *dir;
	float		color;
	float		count;

	org = G_VECTOR(pr, OFS_PARM0);
	dir = G_VECTOR(pr, OFS_PARM1);
	color = G_FLOAT(pr, OFS_PARM2);
	count = G_FLOAT(pr, OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
static void
PF_ambientsound(pr_t *pr)
{
	char		**check;
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			i, soundnum;

	pos = G_VECTOR(pr, OFS_PARM0);
	samp = G_STRING(pr, OFS_PARM1);
	vol = G_FLOAT(pr, OFS_PARM2);
	attenuation = G_FLOAT(pr, OFS_PARM3);

	// check to see if samp was properly precached
	for (soundnum=0, check = sv.sound_precache ; *check ; check++, soundnum++)
		if (!strcmp(*check,samp))
			break;

	if (!*check)
	{
		Con_Printf ("no precache: %s\n", samp);
		return;
	}

	// add an svc_spawnambient command to the level signon packet

	if(soundnum >= sv.protocol->limit_sound)
		return;

	SV_SignonFrame();

	MSG_WriteByte(
		sv.signon,
		soundnum < sv.protocol->large_sound ?
			svc_spawnstaticsound :
			svc_spawnstaticsound2
	);
	for (i=0 ; i<3 ; i++)
		sv.protocol->MSG_WriteCoord(sv.signon, pos[i]);

	(soundnum < sv.protocol->large_sound ? MSG_WriteByte : MSG_WriteShort)(sv.signon, soundnum);
	MSG_WriteByte(sv.signon, vol*255);
	MSG_WriteByte(sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
static void
PF_sound(pr_t *pr)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = G_EDICT(pr, OFS_PARM0);
	channel = G_FLOAT(pr, OFS_PARM1);
	sample = G_STRING(pr, OFS_PARM2);
	volume = G_FLOAT(pr, OFS_PARM3) * 255;
	attenuation = G_FLOAT(pr, OFS_PARM4);

	if (volume < 0 || volume > 255)
		fatal ("SV_StartSound: volume = %d", volume);

	if (attenuation < 0 || attenuation > 4)
		fatal ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		fatal ("SV_StartSound: channel = %d", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
static void
PF_break(pr_t *pr)
{
	USED(pr);
	Con_Printf("break statement\n");
	assert(nil);
	//	PR_RunError (pr, "break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
static void
PF_traceline(pr_t *pr)
{
	float	*v1, *v2;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(pr, OFS_PARM0);
	v2 = G_VECTOR(pr, OFS_PARM1);
	nomonsters = G_FLOAT(pr, OFS_PARM2);
	ent = G_EDICT(pr, OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr->global_struct->trace_allsolid = trace.allsolid;
	pr->global_struct->trace_startsolid = trace.startsolid;
	pr->global_struct->trace_fraction = trace.fraction;
	pr->global_struct->trace_inwater = trace.inwater;
	pr->global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr->global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr->global_struct->trace_plane_normal);
	pr->global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr->global_struct->trace_ent = EDICT_TO_PROG(pr, trace.ent);
	else
		pr->global_struct->trace_ent = EDICT_TO_PROG(pr, pr->edicts);
}

//============================================================================

static byte *checkpvs;
static int checkpvs_size;

static int
PF_newcheckclient(pr_t *pr, int check)
{
	int		i, size;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;

	// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if (i == svs.maxclients+1)
			i = 1;

		ent = EDICT_NUM(pr, i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

	// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel, &size);
	if(checkpvs == nil || size > checkpvs_size){
		checkpvs = realloc(checkpvs, size);
		checkpvs_size = size;
	}
	memcpy (checkpvs, pvs, size);

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
static void
PF_checkclient(pr_t *pr)
{
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;

	// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (pr, sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	// return check if it might be visible
	ent = EDICT_NUM(pr, sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
	{
		RETURN_EDICT(pr, pr->edicts);
		return;
	}

	// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr, pr->global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
		RETURN_EDICT(pr, pr->edicts);
		return;
	}

	// might be able to see it
	RETURN_EDICT(pr, ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
static void
PF_stuffcmd(pr_t *pr)
{
	int		entnum;
	char	*str;
	client_t	*old;

	entnum = G_EDICTNUM(pr, OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError (pr, "Parm 0 not a client");
	str = G_STRING(pr, OFS_PARM1);

	old = host_client;
	host_client = &svs.clients[entnum-1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
static void
PF_localcmd(pr_t *pr)
{
	char	*str;

	str = G_STRING(pr, OFS_PARM0);
	Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
static void
PF_cvar(pr_t *pr)
{
	char	*str;

	str = G_STRING(pr, OFS_PARM0);

	G_FLOAT(pr, OFS_RETURN) = Cvar_VariableValue (str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
static void
PF_cvar_set(pr_t *pr)
{
	char	*var, *val;

	var = G_STRING(pr, OFS_PARM0);
	val = G_STRING(pr, OFS_PARM1);

	setcvar (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
static void
PF_findradius(pr_t *pr)
{
	edict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (edict_t *)pr->edicts;

	org = G_VECTOR(pr, OFS_PARM0);
	rad = G_FLOAT(pr, OFS_PARM1);

	ent = NEXT_EDICT(pr, pr->edicts);
	for (i=1 ; i<pr->num_edicts ; i++, ent = NEXT_EDICT(pr, ent))
	{
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5);
		if (Length(eorg) > rad)
			continue;

		ent->v.chain = EDICT_TO_PROG(pr, chain);
		chain = ent;
	}

	RETURN_EDICT(pr, chain);
}

static void
PF_dprint(pr_t *pr)
{
	Con_DPrintf("%s", PF_VarString(pr, 0));
}

static void
PF_ftos(pr_t *pr)
{
	float	v;
	char *s;

	v = G_FLOAT(pr, OFS_PARM0);
	s = PR_StrTmp(pr);
	if (v == (int)v)
		sprint (s, "%d",(int)v);
	else
		sprint (s, "%f",v);
	G_INT(pr, OFS_RETURN) = PR_SetStr(pr, s);
}

static void
PF_fabs(pr_t *pr)
{
	float	v;
	v = G_FLOAT(pr, OFS_PARM0);
	G_FLOAT(pr, OFS_RETURN) = fabs(v);
}

static void
PF_vtos(pr_t *pr)
{
	char *s;
	s = PR_StrTmp(pr);
	sprint (s, "'%5.1f %5.1f %5.1f'", G_VECTOR(pr, OFS_PARM0)[0], G_VECTOR(pr, OFS_PARM0)[1], G_VECTOR(pr, OFS_PARM0)[2]);
	G_INT(pr, OFS_RETURN) = PR_SetStr(pr, s);
}

static void
PF_Spawn(pr_t *pr)
{
	edict_t	*ed;
	ed = ED_Alloc(pr);
	RETURN_EDICT(pr, ed);
}

static void
PF_Remove(pr_t *pr)
{
	edict_t	*ed;

	ed = G_EDICT(pr, OFS_PARM0);
	ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
static void
PF_find(pr_t *pr)
{
	int		e;
	int		f;
	char	*s, *t;
	edict_t	*ed;
	ddef_t *def;

	e = G_EDICTNUM(pr, OFS_PARM0);
	f = G_INT(pr, OFS_PARM1);
	s = G_STRING(pr, OFS_PARM2);
	if (!s)
		PR_RunError(pr, "PF_find: bad search string");
	if((def = ED_FieldAtOfs(pr, f)) == nil)
		PR_RunError(pr, "PF_find: invalid field offset %d", f);
	USED(def);
	// FIXME(sigrid): apparently this is common
	//if(def->type != ev_string)
	//	Con_DPrintf("PF_find: not a string field: %s", PR_Str(pr, def->s_name));

	for (e++ ; e < pr->num_edicts ; e++)
	{
		ed = EDICT_NUM(pr, e);
		if (ed->free)
			continue;
		t = E_STRING(pr, ed, f);
		if (t == nil)
			t = "";
		if(!strcmp(t, s)){
			RETURN_EDICT(pr, ed);
			return;
		}
	}

	RETURN_EDICT(pr, pr->edicts);
}

static void
PR_CheckEmptyString(pr_t *pr, char *s)
{
	if (s[0] <= ' ')
		PR_RunError (pr, "Bad string");
}

static bool
CloseEnough(edict_t *ent, edict_t *goal, float dist)
{
	int		i;

	for (i=0 ; i<3 ; i++)
	{
		if (goal->v.absmin[i] > ent->v.absmax[i] + dist)
			return false;
		if (goal->v.absmax[i] < ent->v.absmin[i] - dist)
			return false;
	}
	return true;
}

bool SV_StepDirection (edict_t *ent, float yaw, float dist);
void SV_NewChaseDir (edict_t *actor, edict_t *enemy, float dist);

static void
PF_MoveToGoal(pr_t *pr)
{
	edict_t		*ent, *goal;
	float		dist;

	ent = PROG_TO_EDICT(pr, pr->global_struct->self);
	goal = PROG_TO_EDICT(pr, ent->v.goalentity);
	dist = G_FLOAT(pr, OFS_PARM0);

	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(pr, OFS_RETURN) = 0;
		return;
	}

	// if the next step hits the enemy, return immediately
	if ( PROG_TO_EDICT(pr, ent->v.enemy) != pr->edicts &&  CloseEnough (ent, goal, dist) )
		return;

	// bump around...
	if ( (rand()&3)==1 ||
	!SV_StepDirection (ent, ent->v.ideal_yaw, dist))
	{
		SV_NewChaseDir (ent, goal, dist);
	}
}

static void
PF_precache_file(pr_t *pr)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(pr, OFS_RETURN) = G_INT(pr, OFS_PARM0);
}

static void
PF_precache_sound(pr_t *pr)
{
	char	*s;
	int		i;

	if (sv.state != ss_loading)
		PR_RunError (pr, "PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(pr, OFS_PARM0);
	G_INT(pr, OFS_RETURN) = G_INT(pr, OFS_PARM0);
	PR_CheckEmptyString(pr, s);

	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError(pr, "PF_precache_sound: overflow");
}

static void
PF_precache_model(pr_t *pr)
{
	char	*s;
	int		i;

	if (sv.state != ss_loading)
		PR_RunError (pr, "PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(pr, OFS_PARM0);
	G_INT(pr, OFS_RETURN) = G_INT(pr, OFS_PARM0);
	PR_CheckEmptyString (pr, s);

	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName (s, true);
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError (pr,"PF_precache_model: overflow");
}

static void
PF_coredump(pr_t *pr)
{
	// FIXME(sigrid): needs to be pr-specific
	USED(pr);
	ED_PrintEdicts();
}

static void
PF_traceon(pr_t *pr)
{
	pr->trace = true;
}

static void
PF_traceoff(pr_t *pr)
{
	pr->trace = false;
}

static void
PF_eprint(pr_t *pr)
{
	ED_PrintNum(pr, G_EDICTNUM(pr, OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
static void
PF_walkmove(pr_t *pr)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	dfunction_t	*oldf;
	int 	oldself;

	ent = PROG_TO_EDICT(pr, pr->global_struct->self);
	yaw = G_FLOAT(pr, OFS_PARM0);
	dist = G_FLOAT(pr, OFS_PARM1);

	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(pr, OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;

	move[0] = cosf(yaw)*dist;
	move[1] = sinf(yaw)*dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	oldf = pr->xfunction;
	oldself = pr->global_struct->self;

	G_FLOAT(pr, OFS_RETURN) = SV_movestep(ent, move, true);


	// restore program state
	pr->xfunction = oldf;
	pr->global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
static void
PF_droptofloor(pr_t *pr)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	ent = PROG_TO_EDICT(pr, pr->global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(pr, OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(pr, trace.ent);
		G_FLOAT(pr, OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
static void
PF_lightstyle(pr_t *pr)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;

	style = G_FLOAT(pr, OFS_PARM0);
	val = G_STRING(pr, OFS_PARM1);

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
		if (client->active || client->spawned)
		{
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message,style);
			MSG_WriteString (&client->message, val);
		}
}

static void
PF_rint(pr_t *pr)
{
	float	f;
	f = G_FLOAT(pr, OFS_PARM0);
	G_FLOAT(pr, OFS_RETURN) = Qrint(f);
}

static void
PF_floor(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = floorf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_ceil(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = ceilf(G_FLOAT(pr, OFS_PARM0));
}

/*
=============
PF_checkbottom
=============
*/
static void
PF_checkbottom(pr_t *pr)
{
	edict_t	*ent;

	ent = G_EDICT(pr, OFS_PARM0);

	G_FLOAT(pr, OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
static void
PF_pointcontents(pr_t *pr)
{
	float	*v;

	v = G_VECTOR(pr, OFS_PARM0);

	G_FLOAT(pr, OFS_RETURN) = SV_PointContents (v);
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
static void
PF_nextent(pr_t *pr)
{
	int		i;
	edict_t	*ent;

	i = G_EDICTNUM(pr, OFS_PARM0);
	while (1)
	{
		i++;
		if (i == pr->num_edicts)
		{
			RETURN_EDICT(pr, pr->edicts);
			return;
		}
		ent = EDICT_NUM(pr, i);
		if (!ent->free)
		{
			RETURN_EDICT(pr, ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
cvar_t sv_aim = {"sv_aim", "0.93"};
static void
PF_aim(pr_t *pr)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;

	ent = G_EDICT(pr, OFS_PARM0);

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

	// try sending a trace straight
	VectorCopy (pr->global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
	&& (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team) )
	{
		VectorCopy (pr->global_struct->v_forward, G_VECTOR(pr, OFS_RETURN));
		return;
	}


	// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = nil;

	check = NEXT_EDICT(pr, pr->edicts);
	for (i=1 ; i<pr->num_edicts ; i++, check = NEXT_EDICT(pr, check) )
	{
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v.origin[j]
			+ 0.5*(check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr->global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, pr->global_struct->v_forward);
		VectorScale (pr->global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(pr, OFS_RETURN));
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(pr, OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void
PF_changeyaw(pr_t *pr)
{
	edict_t		*ent;
	float		ideal, current, move, speed;

	ent = PROG_TO_EDICT(pr, pr->global_struct->self);
	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[1] = anglemod (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

static sizebuf_t *
WriteDest(pr_t *pr)
{
	int		entnum;
	int		dest;
	edict_t	*ent;

	dest = G_FLOAT(pr, OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		ent = PROG_TO_EDICT(pr, pr->global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(pr, ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError (pr, "WriteDest: not a client");
		return &svs.clients[entnum-1].message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return sv.signon;

	default:
		PR_RunError (pr, "WriteDest: bad destination");
		break;
	}

	return nil;
}

static void
PF_WriteByte(pr_t *pr)
{
	MSG_WriteByte (WriteDest(pr), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_WriteChar(pr_t *pr)
{
	MSG_WriteChar (WriteDest(pr), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_WriteShort(pr_t *pr)
{
	MSG_WriteShort (WriteDest(pr), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_WriteLong(pr_t *pr)
{
	MSG_WriteLong (WriteDest(pr), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_WriteAngle(pr_t *pr)
{
	sv.protocol->MSG_WriteAngle (WriteDest(pr), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_WriteCoord(pr_t *pr)
{
	sv.protocol->MSG_WriteCoord (WriteDest(pr), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_WriteString(pr_t *pr)
{
	MSG_WriteString (WriteDest(pr), G_STRING(pr, OFS_PARM1));
}

static void
PF_WriteEntity(pr_t *pr)
{
	MSG_WriteShort (WriteDest(pr), G_EDICTNUM(pr, OFS_PARM1));
}

//=============================================================================

int SV_ModelIndex (char *name);

static void
PF_makestatic(pr_t *pr)
{
	edict_t	*ent;
	int		i, model, frame, bits;

	ent = G_EDICT(pr, OFS_PARM0);
	model = SV_ModelIndex(PR_Str(pr, ent->v.model));
	frame = ent->v.frame;
	bits = 0;
	if(model >= sv.protocol->limit_model)
		return; // yikes.
	else if(model >= sv.protocol->large_model)
		bits |= sv.protocol->fl_large_baseline_model;
	if(frame >= sv.protocol->limit_frame)
		frame = 0; // yikes.
	else if(frame >= sv.protocol->large_frame)
		bits |= sv.protocol->fl_large_baseline_frame;
	if(zeroalpha(ent->alpha)){
		ED_Free(ent);
		return;
	}
	if(!defalpha(ent->alpha))
		bits |= sv.protocol->fl_baseline_alpha;
	if(((int)ent->v.effects & 0xff) != 0)
		bits |= sv.protocol->fl_baseline_effects;

	SV_SignonFrame();

	MSG_WriteByte(sv.signon, bits ? svc_spawnstatic2 : svc_spawnstatic);
	if(bits)
		MSG_WriteByte(sv.signon, bits);

	((bits & sv.protocol->fl_large_baseline_model) ? MSG_WriteShort : MSG_WriteByte)
		(sv.signon, model);
	((bits & sv.protocol->fl_large_baseline_frame) ? MSG_WriteShort : MSG_WriteByte)
		(sv.signon, frame);
	MSG_WriteByte(sv.signon, ent->v.colormap);
	MSG_WriteByte(sv.signon, ent->v.skin);
	for (i=0 ; i<3 ; i++)
	{
		sv.protocol->MSG_WriteCoord(sv.signon, ent->v.origin[i]);
		sv.protocol->MSG_WriteAngle(sv.signon, ent->v.angles[i]);
	}
	if(bits & sv.protocol->fl_baseline_alpha)
		MSG_WriteByte(sv.signon, ent->alpha);
	if(bits & sv.protocol->fl_baseline_effects)
		MSG_WriteByte(sv.signon, ent->v.effects);

	// throw the entity away now
	ED_Free(ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
static void
PF_setspawnparms(pr_t *pr)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(pr, OFS_PARM0);
	i = NUM_FOR_EDICT(pr, ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError (pr, "Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< Nparms ; i++)
		(&pr->global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
static void
PF_changelevel(pr_t *pr)
{
	char	*s;

	// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = G_STRING(pr, OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
}

static void
PF_Fixme(pr_t *pr)
{
	PR_RunError (pr, "unimplemented builtin");
}

static const char *exts[] = {
	"DP_EF_ADDITIVE",
	"DP_EF_NODRAW",
	"DP_QC_ASINACOSATANATAN2TAN",
	"DP_QC_MINMAXBOUND",
	"DP_QC_SINCOSSQRTPOW",
	"DP_QC_TOKENIZE_CONSOLE", /* FIXME(sigrid): not really; see somewhere below */
	"KRIMZON_SV_PARSECLIENTCOMMAND",
};

static void
PF_sinf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = sinf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_cosf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = cosf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_sqrtf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = sqrtf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_min(pr_t *pr)
{
	float v = Q_MAXFLOAT, n;
	int i;

	for(i = 0; i < pr->argc; i++){
		if(v > (n = G_FLOAT(pr, OFS_PARM0 + i*3)))
			v = n;
	}
	G_FLOAT(pr, OFS_RETURN) = v;
}

static void
PF_max(pr_t *pr)
{
	float v = Q_MINFLOAT, n;
	int i;

	for(i = 0; i < pr->argc; i++){
		if(v < (n = G_FLOAT(pr, OFS_PARM0 + i*3)))
			v = n;
	}
	G_FLOAT(pr, OFS_RETURN) = v;
}

static void
PF_bound(pr_t *pr)
{
	float l = G_FLOAT(pr, OFS_PARM0);
	float v = G_FLOAT(pr, OFS_PARM1);
	float r = G_FLOAT(pr, OFS_PARM2);
	G_FLOAT(pr, OFS_RETURN) = clamp(v, l, r);
}

static void
PF_pow(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = pow(G_FLOAT(pr, OFS_PARM0), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_checkextension(pr_t *pr)
{
	const char *ext = G_STRING(pr, OFS_PARM0);
	int i;

	G_FLOAT(pr, OFS_RETURN) = false;
	Con_DPrintf("checking extension %s\n", ext);
	for(i = 0; i < nelem(exts); i++){
		if(strcmp(ext, exts[i]) == 0){
			G_FLOAT(pr, OFS_RETURN) = true;
			break;
		}
	}
}

static void
PF_clientstat(pr_t *pr)
{
	// FIXME
	// Arcane Dimensions will fall off if this one isn't defined
	// even though it does *not* check for the extension
	USED(pr);
}

static void
PF_clientcommand(pr_t *pr)
{
	client_t *tmp;
	edict_t *e;
	char *cmd;
	int n;

	if(!pr->parse_cl_command.in_callback){
		Con_DPrintf("clientcommand(...) outside of SV_ParseClientCommand\n");
		return;
	}
	e = G_EDICT(pr, OFS_PARM0);
	cmd = G_STRING(pr, OFS_PARM1);
	if((n = NUM_FOR_EDICT(pr, e)-1) >= svs.maxclients || !svs.clients[n].active){
		Con_DPrintf("clientcommand(...) on a non-client entity\n");
		return;
	}
	if(!*cmd)
		return;
	tmp = host_client;
	host_client = &svs.clients[n];
	Cmd_ExecuteString(cmd, src_client);
	host_client = tmp;
}

static void
PF_asinf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = asinf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_acosf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = acosf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_atanf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = atanf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_atan2f(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = atan2f(G_FLOAT(pr, OFS_PARM0), G_FLOAT(pr, OFS_PARM1));
}

static void
PF_tanf(pr_t *pr)
{
	G_FLOAT(pr, OFS_RETURN) = tanf(G_FLOAT(pr, OFS_PARM0));
}

static void
PF_tokenize(pr_t *pr)
{
	char *s;
	int n;

	s = G_STRING(pr, OFS_PARM0);
	for(n = 0; s != nil && n < nelem(pr->parse_cl_command.argv); n++){
		while(*s && isspace(*s))
			s++;
		if(*s == 0 || (s = COM_Parse(s)) == nil)
			break;
		pr->parse_cl_command.argv[n] = PR_CopyStrTmp(pr, com_token);
	}
	pr->parse_cl_command.argc = n;
	G_FLOAT(pr, OFS_RETURN) = n;
}

static void
PF_argv(pr_t *pr)
{
	int i;

	if((i = G_FLOAT(pr, OFS_PARM0)) < 0)
		i += pr->parse_cl_command.argc;
	G_INT(pr, OFS_RETURN) = i >= pr->parse_cl_command.argc ? 0 : pr->parse_cl_command.argv[i];
}

static const builtin_t pr_sv_builtins[] =
{
PF_Fixme,
PF_makevectors,	// void(entity e)	makevectors 		= #1;
PF_setorigin,	// void(entity e, vector o) setorigin	= #2;
PF_setmodel,	// void(entity e, string m) setmodel	= #3;
PF_setsize,	// void(entity e, vector min, vector max) setsize = #4;
PF_Fixme,	// void(entity e, vector min, vector max) setabssize = #5;
PF_break,	// void() break						= #6;
PF_random,	// float() random						= #7;
PF_sound,	// void(entity e, float chan, string samp) sound = #8;
PF_normalize,	// vector(vector v) normalize			= #9;
PF_error,	// void(string e) error				= #10;
PF_objerror,	// void(string e) objerror				= #11;
PF_vlen,	// float(vector v) vlen				= #12;
PF_vectoyaw,	// float(vector v) vectoyaw		= #13;
PF_Spawn,	// entity() spawn						= #14;
PF_Remove,	// void(entity e) remove				= #15;
PF_traceline,	// float(vector v1, vector v2, float tryents) traceline = #16;
PF_checkclient,	// entity() clientlist					= #17;
PF_find,	// entity(entity start, .string fld, string match) find = #18;
PF_precache_sound,	// void(string s) precache_sound		= #19;
PF_precache_model,	// void(string s) precache_model		= #20;
PF_stuffcmd,	// void(entity client, string s)stuffcmd = #21;
PF_findradius,	// entity(vector org, float rad) findradius = #22;
PF_bprint,	// void(string s) bprint				= #23;
PF_sprint,	// void(entity client, string s) sprint = #24;
PF_dprint,	// void(string s) dprint				= #25;
PF_ftos,	// void(string s) ftos				= #26;
PF_vtos,	// void(string s) vtos				= #27;
PF_coredump,
PF_traceon,
PF_traceoff,
PF_eprint,	// void(entity e) debug print an entire entity
PF_walkmove, // float(float yaw, float dist) walkmove
PF_Fixme, // float(float yaw, float dist) walkmove
PF_droptofloor,
PF_lightstyle,
PF_rint,
PF_floor,
PF_ceil,
PF_Fixme,
PF_checkbottom,
PF_pointcontents,
PF_Fixme,
PF_fabs,
PF_aim,
PF_cvar,
PF_localcmd,
PF_nextent,
PF_particle,
PF_changeyaw,
PF_Fixme,
PF_vectoangles,

PF_WriteByte,
PF_WriteChar,
PF_WriteShort,
PF_WriteLong,
PF_WriteCoord,
PF_WriteAngle,
PF_WriteString,
PF_WriteEntity,

[60] = PF_sinf,
[61] = PF_cosf,
[62] = PF_sqrtf,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

PF_MoveToGoal,
PF_precache_file,
PF_makestatic,

PF_changelevel,
PF_Fixme,

PF_cvar_set,
PF_centerprint,

PF_ambientsound,

PF_precache_model,
PF_precache_sound,		// precache_sound2 is different only for qcc
PF_precache_file,

PF_setspawnparms, // #78

[94] = PF_min,
[95] = PF_max,
[96] = PF_bound,
[97] = PF_pow,
[99] = PF_checkextension,
[232] = PF_clientstat,
[440] = PF_clientcommand,
[441] = PF_tokenize,
[442] = PF_argv,
[471] = PF_asinf,
[472] = PF_acosf,
[473] = PF_atanf,
[474] = PF_atan2f,
[475] = PF_tanf,
[514] = PF_tokenize, /* FIXME(sigrid): strictly speaking, this is supposed to be "tokenize_console" */
};

void
PR_InitSV(pr_t *pr)
{
	pr->builtins = pr_sv_builtins;
	pr->numbuiltins = nelem(pr_sv_builtins);

	pr->parse_cl_command.func = ED_FindFunction(pr, "SV_ParseClientCommand");
}
