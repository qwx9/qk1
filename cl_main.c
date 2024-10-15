#include "quakedef.h"

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t	cl_name = {"_cl_name", "player", true};
cvar_t	cl_color = {"_cl_color", "0", true};

cvar_t	cl_shownet = {"cl_shownet","0"};	// can be 0, 1, or 2
static cvar_t cl_nolerp = {"cl_nolerp","0"};

cvar_t	lookspring = {"lookspring","0", true};
cvar_t	lookstrafe = {"lookstrafe","0", true};
cvar_t	sensitivity = {"sensitivity","3", true};

cvar_t	m_pitch = {"m_pitch","0.022", true};
cvar_t	m_yaw = {"m_yaw","0.022", true};
cvar_t	m_forward = {"m_forward","1", true};
cvar_t	m_side = {"m_side","0.8", true};

static efrag_t *cl_efrags;

client_static_t	cls;
client_state_t	cl;
entity_t		**cl_visedicts;
entity_t		*cl_entities;
entity_t		*cl_static_entities;
lightstyle_t	cl_lightstyle[Nlights];
dlight_t		cl_dlights[MAX_DLIGHTS];
int				cl_numvisedicts;

static void
CL_ApplyInterpolation(entity_t *e, float frac)
{
	float d, f;
	vec3_t delta;
	int i;

	if(e->forcelink){
		// the entity was not updated in the last message
		// so move to the final spot
		VectorCopy(e->msg_origins[0], e->origin);
		VectorCopy(e->msg_angles[0], e->angles);
	}else{
		f = frac;
		for(i = 0; i < 3; i++){
			delta[i] = e->msg_origins[0][i] - e->msg_origins[1][i];
			if(delta[i] > 100 || delta[i] < -100)
				f = 1;
		}

		for(i = 0; i < 3; i++){
			e->origin[i] = e->msg_origins[1][i] + f*delta[i];
			d = e->msg_angles[0][i] - e->msg_angles[1][i];
			if(d > 180.0)
				d -= 360.0;
			else if(d < -180.0)
				d += 360.0;
			e->angles[i] = e->msg_angles[1][i] + f*d;
		}
	}
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	int			i;

	if (!sv.active)
		Host_ClearMemory ();

	memset(&cl, 0, sizeof cl);

	if(cls.state == ca_connected)
		stopallsfx(nil);

	SZ_Clear (&cls.message);
	CL_ResetTEnts();
	memset(cl_static_entities, 0, MAX_STATIC_ENTITIES * sizeof(*cl_static_entities));
	memset(cl_entities, 0, MAX_EDICTS*sizeof(*cl_entities));
	memset(cl_lightstyle, 0, sizeof cl_lightstyle);
	memset(cl_dlights, 0, sizeof cl_dlights);
	memset(cl_beams, 0, sizeof cl_beams);
	cl_numvisedicts = 0;

	// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;
	for (i=0 ; i<MAX_EFRAGS-1 ; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i+1];
	cl.free_efrags[i].entnext = nil;
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	stopallsfx(nil);

	// bring the console down and fade the colors back to normal
	SCR_BringDownConsole ();

	// if running a local server, shut it down
	if(cls.demoplayback)
		abortdemo();
	else if (cls.state == ca_connected){
		if (cls.demorecording)
			stopdemo(nil);

		Con_DPrintf("CL_Disconnect: sending clc_disconnect...\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer(false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}

void CL_Disconnect_f(cmd_t *c)
{
	USED(c);
	CL_Disconnect();
	if (sv.active)
		Host_ShutdownServer (false);
}

/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Con_DPrintf("CL_EstablishConnection: connected to %s\n", host);

	cls.demonum = -1;			// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;				// need all the signon messages before playing
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply (void)
{
	char str[8192];

	Con_DPrintf("CL_SignonReply: got %d\n", cls.signon);

	switch (cls.signon)
	{
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

	case 2:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("name \"%s\"\n", cl_name.string));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va("color %d %d\n", ((int)cl_color.value)>>4, ((int)cl_color.value)&15));

		MSG_WriteByte (&cls.message, clc_stringcmd);
		sprint (str, "spawn %s", cls.spawnparms);
		MSG_WriteString (&cls.message, str);
		break;

	case 3:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		break;

	case 4:
		SCR_EndLoadingPlaque ();		// allow normal screen updates
		break;
	}
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
bool
CL_NextDemo (void)
{
	char str[1024];

	if(cls.demonum < 0)
		return false; // don't play demos

	if(!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS){
		cls.demonum = 0;
		if(!cls.demos[cls.demonum][0]){
			Con_Printf("No demos listed with startdemos\n");
			cls.demonum = -1;
			return false;
		}
	}

	SCR_BeginLoadingPlaque();
	sprint(str, "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText(str);
	cls.demonum++;
	return true;
}

/*
==============
CL_PrintEntities_f
==============
*/
static void CL_PrintEntities_f(cmd_t *c)
{
	entity_t	*ent;
	int			i;

	USED(c);
	for (i=0,ent=cl_entities ; i<cl.num_entities ; i++,ent++)
	{
		if (!ent->model)
			continue;
		Con_Printf ("%3d:%s:%2d  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
			i,ent->model->name,ent->frame,
			ent->origin[0], ent->origin[1], ent->origin[2],
			ent->angles[0], ent->angles[1], ent->angles[2]
		);
	}
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int		i;
	dlight_t	*dl;

	// first look for an exact key match
	if (key)
	{
		dl = cl_dlights;
		for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
		{
			if (dl->key == key)
			{
				memset(dl, 0, sizeof *dl);
				dl->key = key;
				return dl;
			}
		}
	}

	// then look for anything else
	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time)
		{
			memset(dl, 0, sizeof *dl);
			dl->key = key;
			return dl;
		}
	}

	dl = &cl_dlights[0];
	memset(dl, 0, sizeof *dl);
	dl->key = key;
	return dl;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	int			i;
	dlight_t	*dl;
	float		time;

	time = cl.time - cl.oldtime;

	dl = cl_dlights;
	for (i=0 ; i<MAX_DLIGHTS ; i++, dl++)
	{
		if (dl->die < cl.time || !dl->radius)
			continue;

		dl->radius -= time*dl->decay;
		if (dl->radius < 0)
			dl->radius = 0;
	}
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
static float	CL_LerpPoint (void)
{
	double f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp.value || cls.timedemo || sv.active)
	{
		cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1)
	{	// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;
	if (frac < 0)
	{
		if (frac < -0.01)
			cl.time = cl.mtime[1];
		frac = 0;
	}
	else if (frac > 1)
	{
		if (frac > 1.01)
			cl.time = cl.mtime[0];
		frac = 1;
	}

	return frac;
}


/*
===============
CL_RelinkEntities
===============
*/
static void CL_RelinkEntities (void)
{
	entity_t	*ent;
	int			i, j;
	float		frac, d;
	float		bobjrotate;
	vec3_t		oldorg;
	dlight_t	*dl;

	// determine partial update time
	frac = CL_LerpPoint();

	cl_numvisedicts = 0;

	// interpolate player info
	for(i = 0; i < 3; i++)
		cl.velocity[i] = cl.mvelocity[1][i] + frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if(cls.demoplayback){
		// interpolate the angles
		for(j = 0; j < 3; j++){
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if(d > 180)
				d -= 360;
			else if(d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac*d;
		}
	}

	bobjrotate = anglemod(100*cl.time);

	// start on the entity after the world
	for (i=1,ent=cl_entities+1 ; i<cl.num_entities ; i++,ent++)
	{
		if (!ent->model)
		{	// empty slot
			if (ent->forcelink)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}

		// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0])
		{
			ent->model = nil;
			continue;
		}

		VectorCopy (ent->origin, oldorg);

		CL_ApplyInterpolation(ent, frac);

		// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
			ent->angles[1] = bobjrotate;

		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);
		if (ent->effects & EF_MUZZLEFLASH)
		{
			vec3_t		fv, rv, uv;

			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, rv, uv);

			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
		}
		if (ent->effects & EF_BRIGHTLIGHT)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->origin[2] += 16;
			dl->radius = 400 + (rand()&31);
			dl->die = cl.time + 0.001;
		}
		if (ent->effects & EF_DIMLIGHT)
		{
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin,  dl->origin);
			dl->radius = 200 + (rand()&31);
			dl->die = cl.time + 0.001;
		}

		if (ent->model->flags & EF_GIB)
			R_RocketTrail (oldorg, ent->origin, 2);
		else if (ent->model->flags & EF_ZOMGIB)
			R_RocketTrail (oldorg, ent->origin, 4);
		else if (ent->model->flags & EF_TRACER)
			R_RocketTrail (oldorg, ent->origin, 3);
		else if (ent->model->flags & EF_TRACER2)
			R_RocketTrail (oldorg, ent->origin, 5);
		else if (ent->model->flags & EF_ROCKET)
		{
			R_RocketTrail (oldorg, ent->origin, 0);
			dl = CL_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.01;
		}
		else if (ent->model->flags & EF_GRENADE)
			R_RocketTrail (oldorg, ent->origin, 1);
		else if (ent->model->flags & EF_TRACER3)
			R_RocketTrail (oldorg, ent->origin, 6);

		ent->forcelink = false;

		if ((ent->effects & EF_NODRAW) != 0 || (i == cl.viewentity && !chase_active.value))
			continue;

		if (cl_numvisedicts < MAX_VISEDICTS)
		{
			cl_visedicts[cl_numvisedicts] = ent;
			cl_numvisedicts++;
		}
	}

}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer (void)
{
	int		ret;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	do
	{
		ret = readcl ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet.value)
		Con_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts ();

	// bring the links up to date
	return 0;
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd (void)
{
	usercmd_t		cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS)
	{
		// get basic movement from keyboard
		CL_BaseMove (&cmd);

		// allow mice or other external controllers to add to the move
		IN_Move (&cmd);

		// send the unreliable message
		CL_SendMove (&cmd);

	}

	if (cls.demoplayback)
	{
		SZ_Clear (&cls.message);
		return;
	}

	// send the reliable message
	if (!cls.message.cursize)
		return;		// no message at all

	if (!NET_CanSendMessage (cls.netcon))
	{
		Con_DPrintf("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

/*
=================
CL_Init
=================
*/
void CL_Init (void)
{
	SZ_Alloc (&cls.message, MAX_DATAGRAM);
	cls.message.name = "cls.message";

	cl_visedicts = Hunk_Alloc(MAX_VISEDICTS * sizeof(*cl_visedicts));
	cl_efrags = Hunk_Alloc(MAX_EFRAGS * sizeof(*cl_efrags));
	cl_entities = Hunk_Alloc(MAX_EDICTS * sizeof(*cl_entities));
	cl_static_entities = Hunk_Alloc(MAX_STATIC_ENTITIES * sizeof(*cl_static_entities));

	CL_InitInput ();
	CL_InitTEnts ();

	// register our commands
	Cvar_RegisterVariable (&cl_name);
	Cvar_RegisterVariable (&cl_color);
	Cvar_RegisterVariable (&cl_upspeed);
	Cvar_RegisterVariable (&cl_forwardspeed);
	Cvar_RegisterVariable (&cl_backspeed);
	Cvar_RegisterVariable (&cl_sidespeed);
	Cvar_RegisterVariable (&cl_movespeedkey);
	Cvar_RegisterVariable (&cl_yawspeed);
	Cvar_RegisterVariable (&cl_pitchspeed);
	Cvar_RegisterVariable (&cl_anglespeedkey);
	Cvar_RegisterVariable (&cl_shownet);
	Cvar_RegisterVariable (&cl_nolerp);
	Cvar_RegisterVariable (&lookspring);
	Cvar_RegisterVariable (&lookstrafe);
	Cvar_RegisterVariable (&sensitivity);

	Cvar_RegisterVariable (&m_pitch);
	Cvar_RegisterVariable (&m_yaw);
	Cvar_RegisterVariable (&m_forward);
	Cvar_RegisterVariable (&m_side);

	Cmd_AddCommand("entities", CL_PrintEntities_f);
	Cmd_AddCommand("disconnect", CL_Disconnect_f);
	Cmd_AddCommand("stop", stopdemo);
	Cmd_AddCommand("record", recdemo);
	Cmd_AddCommand("playdemo", playdemo);
	Cmd_AddCommand("timedemo", timedemo);
}
