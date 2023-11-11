#include "quakedef.h"

/*

A server can allways be started, even if the system started out as a client
to a remote system.

A client can NOT be started if the system started as a dedicated server.

Memory is cleared / released when a server or client begins, not when they end.

*/
int dedicated;

bool	host_initialized;		// true if into command execution

double		host_frametime;
double		host_time;
double		realtime;				// without any filtering or bounding
int			host_framecount;

static double oldrealtime;			// last frame run

static void *host_hunklevel;

client_t	*host_client;			// current client

jmp_buf 	host_abortserver;

byte		*host_basepal;
byte		*host_colormap;

static cvar_t	host_framerate = {"host_framerate","0"};	// set for slow motion
static cvar_t	host_speeds = {"host_speeds","0"};			// set for running times
static cvar_t	serverprofile = {"serverprofile","0"};
static cvar_t	samelevel = {"samelevel","0"};
static cvar_t	noexit = {"noexit","0",false,true};
static cvar_t	temp1 = {"temp1","0"};

cvar_t	sys_ticrate = {"sys_ticrate","0.05"};

cvar_t	fraglimit = {"fraglimit","0",false,true};
cvar_t	timelimit = {"timelimit","0",false,true};
cvar_t	teamplay = {"teamplay","0",false,true};

cvar_t	skill = {"skill","1"};						// 0 - 3
cvar_t	deathmatch = {"deathmatch","0"};			// 0, 1, or 2
cvar_t	coop = {"coop","0"};			// 0 or 1

cvar_t	pausable = {"pausable","1"};

/*
================
Host_EndGame
================
*/
void Host_EndGame (char *fmt, ...)
{
	va_list arg;
	char s[1024];

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	Con_DPrintf("Host_EndGame: %s\n", s);

	if(sv.active)
		Host_ShutdownServer(false);
	if(cls.state == ca_dedicated)
		fatal("Host_EndGame: %s\n", s);	// dedicated servers exit
	if(cls.demonum != -1)
		CL_NextDemo();
	else
		CL_Disconnect();

	longjmp(host_abortserver, 1);
}

/*
================
Host_Error

This shuts down both the client and server
================
*/
void Host_Error (char *fmt, ...)
{
	va_list arg;
	char s[1024];
	static bool inerror = false;

	if(inerror)
		fatal("Host_Error: recursively entered");
	inerror = true;

	SCR_EndLoadingPlaque();	// reenable screen updates

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	Con_Printf("Host_Error: %s\n", s);

	if(sv.active)
		Host_ShutdownServer(false);
	if(cls.state == ca_dedicated)
		fatal("Host_Error: %s\n", s);	// dedicated servers exit
	CL_Disconnect();
	cls.demonum = -1;

	inerror = false;

	longjmp(host_abortserver, 1);
}

/*
================
Host_FindMaxClients
================
*/
void	Host_FindMaxClients (void)
{
	svs.maxclients = 1;
	cls.state = ca_disconnected;
	if(dedicated){
		cls.state = ca_dedicated;
		svs.maxclients = MAX_SCOREBOARD;
	}
	if (svs.maxclients < 1)
		svs.maxclients = 8;
	else if (svs.maxclients > MAX_SCOREBOARD)
		svs.maxclients = MAX_SCOREBOARD;

	svs.maxclientslimit = svs.maxclients;
	if (svs.maxclientslimit < 4)
		svs.maxclientslimit = 4;
	svs.clients = Hunk_Alloc(svs.maxclientslimit * sizeof *svs.clients);

	if (svs.maxclients > 1)
		setcvarv ("deathmatch", 1.0);
	else
		setcvarv ("deathmatch", 0.0);
}


/*
=======================
Host_InitLocal
======================
*/
void Host_InitLocal (void)
{
	Host_InitCommands ();

	Cvar_RegisterVariable (&host_framerate);
	Cvar_RegisterVariable (&host_speeds);

	Cvar_RegisterVariable (&sys_ticrate);
	Cvar_RegisterVariable (&serverprofile);

	Cvar_RegisterVariable (&fraglimit);
	Cvar_RegisterVariable (&timelimit);
	Cvar_RegisterVariable (&teamplay);
	Cvar_RegisterVariable (&samelevel);
	Cvar_RegisterVariable (&noexit);
	Cvar_RegisterVariable (&skill);
	Cvar_RegisterVariable (&deathmatch);
	Cvar_RegisterVariable (&coop);

	Cvar_RegisterVariable (&pausable);

	Cvar_RegisterVariable (&temp1);

	Host_FindMaxClients ();

	host_time = 1.0;		// so a think at time 0 won't get called
}

/*
=================
SV_ClientPrintf

Sends text across to be displayed
FIXME: make this just a stuffed echo?
=================
*/
void SV_ClientPrintf (char *fmt, ...)
{
	va_list arg;
	char s[1024];

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	MSG_WriteByte(&host_client->message, svc_print);
	MSG_WriteString(&host_client->message, s);
}

/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
void SV_BroadcastPrintf (char *fmt, ...)
{
	va_list arg;
	char s[1024];
	int i;

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	for(i=0; i<svs.maxclients; i++)
		if(svs.clients[i].active && svs.clients[i].spawned){
			MSG_WriteByte(&svs.clients[i].message, svc_print);
			MSG_WriteString(&svs.clients[i].message, s);
		}
}

/*
=================
Host_ClientCommands

Send text over to the client to be executed
=================
*/
void Host_ClientCommands (char *fmt, ...)
{
	va_list arg;
	char s[1024];

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	MSG_WriteByte(&host_client->message, svc_stufftext);
	MSG_WriteString(&host_client->message, s);
}

/*
=====================
SV_DropClient

Called when the player is getting totally kicked off the host
if (crash = true), don't bother sending signofs
=====================
*/
void SV_DropClient (bool crash)
{
	int		saveSelf;
	int		i;
	client_t *client;

	if (!crash)
	{
		// send any final messages (don't check for errors)
		if (NET_CanSendMessage (host_client->netconnection))
		{
			MSG_WriteByte (&host_client->message, svc_disconnect);
			NET_SendMessage (host_client->netconnection, &host_client->message);
		}

		if (host_client->edict && host_client->spawned)
		{
			// call the prog function for removing a client
			// this will set the body to a dead frame, among other things
			saveSelf = pr_global_struct->self;
			pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
			PR_ExecuteProgram (pr_global_struct->ClientDisconnect);
			pr_global_struct->self = saveSelf;
		}

		Con_DPrintf("client %s removed\n", host_client->name);
	}

	// break the net connection
	NET_Close (host_client->netconnection);
	host_client->netconnection = nil;

	// free the client (the body stays around)
	host_client->active = false;
	host_client->name[0] = 0;
	host_client->old_frags = Q_MININT;
	net_activeconnections--;

	// send notification to all clients
	for (i=0, client = svs.clients ; i<svs.maxclients ; i++, client++)
	{
		if (!client->active)
			continue;
		MSG_WriteByte (&client->message, svc_updatename);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteString (&client->message, "");
		MSG_WriteByte (&client->message, svc_updatefrags);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteShort (&client->message, 0);
		MSG_WriteByte (&client->message, svc_updatecolors);
		MSG_WriteByte (&client->message, host_client - svs.clients);
		MSG_WriteByte (&client->message, 0);
	}
}

/*
==================
Host_ShutdownServer

This only happens at the end of a game, not between levels
==================
*/
void Host_ShutdownServer(bool crash)
{
	int		i;
	int		count;
	sizebuf_t	buf;
	char		message[4];
	double	start;

	if (!sv.active)
		return;
	UDP_Listen(0);
	sv.active = false;

	// stop all client sounds immediately
	if (cls.state == ca_connected)
		CL_Disconnect ();

	// flush any pending messages - like the score!!!
	start = dtime();
	do
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (host_client->active && host_client->message.cursize)
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					NET_SendMessage(host_client->netconnection, &host_client->message);
					SZ_Clear (&host_client->message);
				}
				else
				{
					NET_GetMessage(host_client->netconnection);
					count++;
				}
			}
		}
		if ((dtime() - start) > 3.0f)
			break;
	}
	while (count);

	// make sure all the clients know we're disconnecting
	buf.data = (uchar *)message;
	buf.maxsize = 4;
	buf.cursize = 0;
	MSG_WriteByte(&buf, svc_disconnect);
	count = NET_SendToAll(&buf, 5);
	if (count)
		Con_Printf("Host_ShutdownServer: NET_SendToAll failed for %d clients\n", count);

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_DropClient(crash);

	// clear structures
	memset(&sv, 0, sizeof sv);
	memset(svs.clients, 0, svs.maxclientslimit * sizeof *svs.clients);
}


/*
================
Host_ClearMemory

This clears all the memory used by both the client and server, but does
not reinitialize anything.
================
*/
void Host_ClearMemory (void)
{
	Con_DPrintf("Clearing memory\n");
	D_FlushCaches ();
	Mod_ClearAll ();
	if (host_hunklevel)
		Hunk_FreeToMark (host_hunklevel);

	cls.signon = 0;
	memset(&sv, 0, sizeof sv);
	memset(&cl, 0, sizeof cl);
}

/*
==================
Host_ServerFrame

==================
*/
void Host_ServerFrame (void)
{
	// run the world state
	pr_global_struct->frametime = host_frametime;

	// set the time and clear the general datagram
	SV_ClearDatagram ();

	// check for new clients
	SV_CheckForNewClients ();

	// read client messages
	SV_RunClients ();

	// move things around and think
	// always pause in single player if in console or menus
	if (!sv.paused && (svs.maxclients > 1 || key_dest == key_game) )
		SV_Physics ();

	// send all messages to the clients
	SV_SendClientMessages ();
}

static int
boundfps(float t)
{
	realtime += t;
	if(!cls.timedemo && realtime - oldrealtime < 1.0 / Fpsmax)
		return -1;
	host_frametime = realtime - oldrealtime;
	oldrealtime = realtime;
	if(host_framerate.value > 0)
		host_frametime = host_framerate.value;
	else if(host_frametime > 1.0 / Fpsmin)
		host_frametime = 1.0 / Fpsmin;
	return 0;
}

void _Host_Frame (float time)
{
	static double		time1 = 0;
	static double		time2 = 0;
	static double		time3 = 0;
	int			pass1, pass2, pass3;

	if (setjmp (host_abortserver) )
		return;			// something bad happened, or the server disconnected

	// keep the random time dependent
	rand ();

	if(boundfps(time) < 0)
		return;

	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// if running the server locally, make intentions now
	if (sv.active)
		CL_SendCmd ();

	//-------------------
	//
	// server operations
	//
	//-------------------

	// check for commands typed to the host
	conscmd();

	if (sv.active)
		Host_ServerFrame ();

	//-------------------
	//
	// client operations
	//
	//-------------------

	// if running the server remotely, send intentions now after
	// the incoming messages have been read
	if (!sv.active)
		CL_SendCmd ();

	host_time += host_frametime;

	// fetch results from server
	if (cls.state == ca_connected)
	{
		CL_ReadFromServer ();
	}

	// update video
	if (host_speeds.value)
		time1 = dtime ();

	SCR_UpdateScreen (false);

	if (host_speeds.value)
		time2 = dtime ();

	// update audio
	if (cls.signon == SIGNONS)
	{
		stepsnd (r_origin, vpn, vright, vup);
		CL_DecayLights ();
	}
	else
		stepsnd (vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	stepcd();

	if (host_speeds.value)
	{
		pass1 = (time1 - time3)*1000;
		time3 = dtime ();
		pass2 = (time2 - time1)*1000;
		pass3 = (time3 - time2)*1000;
		Con_Printf ("%3d tot %3d server %3d gfx %3d snd\n",
					pass1+pass2+pass3, pass1, pass2, pass3);
	}

	host_framecount++;
}

void Host_Frame (float time)
{
	double	time1, time2;
	static double	timetotal;
	static int		timecount;
	int		i, c, m;

	if (!serverprofile.value)
	{
		_Host_Frame (time);
		return;
	}

	time1 = dtime ();
	_Host_Frame (time);
	time2 = dtime ();

	timetotal += time2 - time1;
	timecount++;

	if (timecount < 1000)
		return;

	m = timetotal*1000/timecount;
	timecount = 0;
	timetotal = 0;
	c = 0;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		if (svs.clients[i].active)
			c++;
	}

	Con_Printf ("serverprofile: %2d clients %2d msec\n",  c,  m);
}

/*
====================
Host_Init
====================
*/
void Host_Init (int argc, char **argv, char **paths)
{
	int i;

	Memory_Init();
	Cbuf_Init ();
	Cmd_Init ();
	V_Init ();
	Chase_Init ();
	initfs(paths);
	Host_InitLocal ();
	W_LoadWadFile ("gfx.wad");
	Key_Init ();
	Con_Init ();
	M_Init ();
	PR_Init ();
	Mod_Init ();
	NET_Init ();
	SV_Init ();
	R_InitTextures ();		// needed even for dedicated servers

	if (cls.state != ca_dedicated)
	{
		host_basepal = loadhunklmp("gfx/palette.lmp", nil);
		if(host_basepal == nil)
			fatal("Host_Init: %s", lerr());
		host_colormap = loadhunklmp("gfx/colormap.lmp", nil);
		if(host_colormap == nil)
			fatal("Host_Init: %s", lerr());

		initfb();

		Draw_Init ();
		SCR_Init ();
		R_Init ();
		if(initsnd() < 0)
			Con_DPrintf("initsnd: %s\n", lerr());
		if(initcd() < 0)
			Con_DPrintf("initcd: %s\n", lerr());
		Sbar_Init ();
		CL_Init ();
	}
	IN_Init ();

	Cbuf_InsertText ("+mlook\n");
	Cbuf_InsertText ("exec quake.rc\n");

	host_hunklevel = Hunk_Mark ();

	host_initialized = true;

	if(argc < 1 || **argv != '+')
		return;
	for(i = 0; i < argc;){
		Cbuf_AddText(argv[i++]+1);
		while(i < argc && argv[i][0] != '+'){
			Cbuf_AddText(" ");
			Cbuf_AddText(argv[i++]);
		}
		Cbuf_AddText("\n");
	}
}


/*
===============
Host_Shutdown

FIXME: this is a callback from shutdown and fatal.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void Host_Shutdown(void)
{
	static bool isdown = false;

	if (isdown)
	{
		Con_DPrintf("Host_Shutdown: recursive shutdown\n");
		return;
	}
	isdown = true;

	// keep Con_Printf from trying to update the screen
	scr_disabled_for_loading = true;

	dumpcfg();

	NET_Shutdown ();
	shutcd();
	sndclose();
	IN_Shutdown ();
}

