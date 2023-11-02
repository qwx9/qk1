#include <u.h>
#include <libc.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

static char *svc_strings[] =
{
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",		// [long] server version
	"svc_setview",		// [short] entity number
	"svc_sound",			// <see code>
	"svc_time",			// [float] server time
	"svc_print",			// [string] null terminated string
	"svc_stufftext",		// [string] stuffed into client's console buffer
						// the string should be \n terminated
	"svc_setangle",		// [vec3] set the view angle to this absolute value
	
	"svc_serverinfo",		// [long] version
						// [string] signon string
						// [string]..[0]model cache [string]...[0]sounds cache
						// [string]..[0]item cache
	"svc_lightstyle",		// [byte] [string]
	"svc_updatename",		// [byte] [string]
	"svc_updatefrags",	// [byte] [short]
	"svc_clientdata",		// <shortbits + data>
	"svc_stopsound",		// <see code>
	"svc_updatecolors",	// [byte] [byte]
	"svc_particle",		// [vec3] <variable>
	"svc_damage",			// [byte] impact [byte] blood [vec3] from
	
	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",
	
	"svc_temp_entity",		// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",			// [string] music [string] text
	"svc_cdtrack",			// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene",

	"svc_spawnbaseline2",
	"svc_spawnstatic2",
	"svc_spawnstaticsound2",
};

//=============================================================================

/*
===============
CL_EntityNum

This error checks and tracks the total number of entities
===============
*/
static entity_t	*CL_EntityNum(int num)
{
	if (num >= cl.num_entities)
	{
		if (num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: %d is an invalid number",num);
		while (cl.num_entities<=num)
		{
			cl_entities[cl.num_entities].colormap = vid.colormap;
			cl.num_entities++;
		}
	}
		
	return &cl_entities[num];
}


/*
==================
CL_ParseStartSoundPacket
==================
*/
static void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos;
    int 	channel, ent;
    int 	sound_num;
    int 	volume;
    int 	field_mask;
    float 	attenuation;  
	           
    field_mask = MSG_ReadByte(); 
    volume = (field_mask & SND_VOLUME) ? MSG_ReadByte() : Spktvol;
	attenuation = (field_mask & SND_ATTENUATION) ? MSG_ReadByte()/64.0 : Spktatt;
	if(field_mask & (cl.protocol.fl_large_entity | cl.protocol.fl_large_channel)){
		ent = MSG_ReadShort();
		channel = MSG_ReadByte();
	}else{
		channel = MSG_ReadShort ();
		ent = channel >> 3;
		channel &= 7;
	}
	sound_num = (field_mask & cl.protocol.fl_large_sound) ? MSG_ReadShort() : MSG_ReadByte();

	if(ent > MAX_EDICTS)
		Host_Error("CL_ParseStartSoundPacket: ent = %d", ent);

	MSG_ReadVec(cl.protocol, pos);
    startsfx (ent, channel, cl.sound_precache[sound_num], pos, volume/255.0, attenuation);
}       

/*
==================
CL_KeepaliveMessage

When the client is taking a long time to load stuff, send keepalive messages
so the server doesn't disconnect.
==================
*/
static void CL_KeepaliveMessage (void)
{
	float	time;
	static float lastmsg;
	int		ret;
	sizebuf_t	old;
	static byte		olddata[NET_MAXMESSAGE];
	
	if (sv.active)
		return;		// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = net_message;
	memcpy (olddata, net_message.data, net_message.cursize);
	
	do
	{
		ret = readcl ();
		switch (ret)
		{
		default:
			Host_Error ("CL_KeepaliveMessage: readcl failed");		
		case 0:
			break;	// nothing waiting
		case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
		case 2:
			if (MSG_ReadByte() != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	net_message = old;
	memcpy (net_message.data, olddata, net_message.cursize);

// check time
	time = dtime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

/*
==================
CL_ParseServerInfo
==================
*/
static void CL_ParseServerInfo (void)
{
	char	*str;
	void	*p;
	int		i, n;
	int		nummodels, numsounds;
	static char	model_precache[MAX_MODELS][Npath];
	static char	sound_precache[MAX_SOUNDS][Npath];

	Con_DPrintf("CL_ParseServerInfo: parsing serverinfo pkt...\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();

// parse protocol version number
	i = MSG_ReadLong ();
	p = nil;
	for(n = 0; n < nelem(protos); n++){
		if(protos[n].version == i){
			p = &protos[n];
			break;
		}
	}
	if(p == nil){
		Con_Printf ("Server returned unknown protocol version %d", i);
		return;
	}
	memmove(&cl.protocol, p, sizeof(cl.protocol));
	cl.protocol.MSG_ReadProtocolInfo(&cl.protocol);

// parse maxclients
	cl.maxclients = MSG_ReadByte ();
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD)
	{
		Con_Printf("Bad maxclients (%d) from server\n", cl.maxclients);
		return;
	}
	cl.scores = Hunk_Alloc(cl.maxclients * sizeof *cl.scores);

// parse gametype
	cl.gametype = MSG_ReadByte ();

// parse signon message
	str = MSG_ReadString ();
	strncpy (cl.levelname, str, sizeof(cl.levelname)-1);

// seperate the printfs so the server message can have a color
	Con_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_Printf ("%c%s\n", 2, str);

//
// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it
//

// precache models
	memset(cl.model_precache, 0, sizeof cl.model_precache);
	for (nummodels=1 ; ; nummodels++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (nummodels==MAX_MODELS)
		{
			Con_Printf ("Server sent too many model precaches\n");
			return;
		}
		strcpy (model_precache[nummodels], str);
		Mod_TouchModel (str);
	}

// precache sounds
	memset(cl.sound_precache, 0, sizeof cl.sound_precache);
	sfxbegin();
	for (numsounds=1 ; ; numsounds++)
	{
		str = MSG_ReadString ();
		if (!str[0])
			break;
		if (numsounds==MAX_SOUNDS)
		{
			Con_Printf ("Server sent too many sound precaches\n");
			return;
		}
		strcpy (sound_precache[numsounds], str);
		touchsfx (str);
	}

//
// now we try to load everything else until a cache allocation fails
//

	for (i=1 ; i<nummodels ; i++)
	{
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (cl.model_precache[i] == nil)
		{
			Con_Printf("Model %s not found\n", model_precache[i]);
			return;
		}
		CL_KeepaliveMessage ();
	}

	for (i=1 ; i<numsounds ; i++)
	{
		cl.sound_precache[i] = precachesfx (sound_precache[i]);
		CL_KeepaliveMessage ();
	}


// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];
	
	R_NewMap ();

	noclip_anglehack = false;		// noclip is turned off at start	
}


/*
==================
CL_ParseUpdate

Parse an entity update message from the server
If an entities model or origin changes from frame to frame, it must be
relinked.  Other attributes can change without relinking.
==================
*/
static int	bitcounts[16];

static void CL_ParseUpdate (int bits)
{
	int			i;
	model_t		*model;
	int			modnum;
	qboolean	forcelink;
	entity_t	*ent;
	int			num, skin, colormap;

	if (cls.signon == SIGNONS - 1)
	{	// first update is the final signon stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS){
		bits |= MSG_ReadByte()<<8;
		if(bits & U_MOREBITS2){
			bits |= MSG_ReadByte()<<16;
			if(bits & U_MOREBITS3)
				bits |= MSG_ReadByte()<<24;
		}
		if(bits == -1)
			Host_Error("CL_ParseUpdate: unexpected EOF while reading extended bits");
	}

	num = (bits & U_LONGENTITY) ? MSG_ReadShort() : MSG_ReadByte ();
	ent = CL_EntityNum (num);

	for (i=0 ; i<16 ; i++)
		if (bits&(1<<i))
			bitcounts[i]++;

	forcelink = ent->msgtime != cl.mtime[1]; // no previous frame to lerp from
	ent->msgtime = cl.mtime[0];

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	modnum = (bits & U_MODEL) ? MSG_ReadByte() : ent->baseline.modelindex;
	ent->frame = (bits & U_FRAME) ? MSG_ReadByte() : ent->baseline.frame;
	colormap = (bits & U_COLORMAP) ? MSG_ReadByte() : ent->baseline.colormap;
	skin = (bits & U_SKIN) ? MSG_ReadByte() : ent->baseline.skin;
	ent->effects = (bits & U_EFFECTS) ? MSG_ReadByte() : ent->baseline.effects;

	if(colormap == 0)
		ent->colormap = vid.colormap;
	else if(colormap > cl.maxclients)
		fatal("colormap > cl.maxclients");
	else
		ent->colormap = cl.scores[colormap-1].translations;

	if(skin != ent->skinnum && num >= 0 && num <= cl.maxclients){
		// FIXME(sigrid): update the skin
		ent->skinnum = skin;
	}

	ent->msg_origins[0][0] = (bits & U_ORIGIN1) ? cl.protocol.MSG_ReadCoord() : ent->baseline.origin[0];
	ent->msg_angles[0][0] = (bits & U_ANGLE1) ? cl.protocol.MSG_ReadAngle() : ent->baseline.angles[0];
	ent->msg_origins[0][1] = (bits & U_ORIGIN2) ? cl.protocol.MSG_ReadCoord() : ent->baseline.origin[1];
	ent->msg_angles[0][1] = (bits & U_ANGLE2) ? cl.protocol.MSG_ReadAngle() : ent->baseline.angles[1];
	ent->msg_origins[0][2] = (bits & U_ORIGIN3) ? cl.protocol.MSG_ReadCoord() : ent->baseline.origin[2];
	ent->msg_angles[0][2] = (bits & U_ANGLE3) ? cl.protocol.MSG_ReadAngle() : ent->baseline.angles[2];

	ent->alpha = (bits & cl.protocol.fl_alpha) ? MSG_ReadByte() : ent->baseline.alpha;

	if(bits & U_NOLERP)
		ent->forcelink = true;

	if(bits & cl.protocol.fl_large_frame)
		ent->frame |= MSG_ReadByte()<<8;
	if(bits & cl.protocol.fl_large_model)
		modnum |= MSG_ReadByte()<<8;

	if(modnum < 0 || modnum >= MAX_MODELS)
		Host_Error("CL_ParseModel: bad modnum: %d (bits 0x%08ux)", modnum, bits);
	model = cl.model_precache[modnum];
	if (model != ent->model)
	{
		ent->model = model;
	// automatic animation (torches, etc) can be either all together
	// or randomized
		if (model)
		{
			if (model->synctype == ST_RAND)
				ent->syncbase = (float)(rand()&0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		}
		else
			forcelink = true;	// hack to make null model players work
	}

	if ( forcelink )
	{	// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
static void CL_ParseBaseline (int withbits, entity_t *ent)
{
	int i, bits;

	bits = withbits ? MSG_ReadByte() : 0;
	ent->baseline.modelindex = (bits & cl.protocol.fl_large_baseline_model) ? MSG_ReadShort() : MSG_ReadByte();
	ent->baseline.frame = (bits & cl.protocol.fl_large_baseline_frame) ? MSG_ReadShort() : MSG_ReadByte();
	ent->baseline.colormap = MSG_ReadByte();
	ent->baseline.skin = MSG_ReadByte();
	for (i=0 ; i<3 ; i++)
	{
		ent->baseline.origin[i] = cl.protocol.MSG_ReadCoord ();
		ent->baseline.angles[i] = cl.protocol.MSG_ReadAngle ();
	}
	ent->baseline.alpha = (bits & cl.protocol.fl_baseline_alpha) ? MSG_ReadByte() : DEFAULT_ALPHA;
}


/*
==================
CL_ParseClientdata

Server information pertaining to this client only
==================
*/
static void CL_ParseClientdata (unsigned int bits)
{
	int		i, j, weaponmodel;

	if(bits & SU_MOREBITS){
		bits |= MSG_ReadByte() << 16;
		if(bits & SU_MOREBITS2)
			bits |= MSG_ReadByte() << 24;
	}

	cl.viewheight = (bits & SU_VIEWHEIGHT) ? MSG_ReadChar() : DEFAULT_VIEWHEIGHT;
	cl.idealpitch = (bits & SU_IDEALPITCH) ? MSG_ReadChar() : 0;

	VectorCopy(cl.mvelocity[0], cl.mvelocity[1]);
	for (i=0 ; i<3 ; i++)
	{
		cl.punchangle[i] = (bits & (SU_PUNCH1<<i)) ? MSG_ReadChar() : 0;
		cl.mvelocity[0][i] = (bits & (SU_VELOCITY1<<i)) ? MSG_ReadChar()*16 : 0;
	}

	i = (bits & SU_ITEMS) ? MSG_ReadLong() : cl.items;
	if(cl.items != i){ // set flash times
		Sbar_Changed ();
		for (j=0 ; j<32 ; j++)
			if ( (i & (1<<j)) && !(cl.items & (1<<j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;
	cl.stats[STAT_WEAPONFRAME] = (bits & SU_WEAPONFRAME) ? MSG_ReadByte() : 0;

	i = (bits & SU_ARMOR) ? MSG_ReadByte() : 0;
	if(cl.stats[STAT_ARMOR] != i){
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	weaponmodel = (bits & SU_WEAPON) ? MSG_ReadByte() : 0;
	
	i = MSG_ReadShort();
	if (cl.stats[STAT_HEALTH] != i){
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed();
	}

	i = MSG_ReadByte();
	if(cl.stats[STAT_AMMO] != i){
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed();
	}

	for (i=0 ; i<4 ; i++){
		j = MSG_ReadByte();
		if(cl.stats[STAT_SHELLS+i] != j){
			cl.stats[STAT_SHELLS+i] = j;
			Sbar_Changed();
		}
	}

	i = MSG_ReadByte();
	if(standard_quake){
		if(cl.stats[STAT_ACTIVEWEAPON] != i){
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed();
		}
	}else if(cl.stats[STAT_ACTIVEWEAPON] != (1<<i)){
		cl.stats[STAT_ACTIVEWEAPON] = (1<<i);
		Sbar_Changed();
	}

	if(bits & cl.protocol.fl_large_weaponmodel)
		weaponmodel |= MSG_ReadByte() << 8;
	if(cl.stats[STAT_WEAPON] != weaponmodel){
		cl.stats[STAT_WEAPON] = weaponmodel;
		Sbar_Changed();
	}

	if(bits & cl.protocol.fl_large_weaponframe)
		cl.stats[STAT_WEAPONFRAME] |= MSG_ReadByte() << 8;
	cl.viewent.alpha = (bits & cl.protocol.fl_weapon_alpha) ? MSG_ReadByte() : DEFAULT_ALPHA;

	if(cl.viewent.model != cl.model_precache[cl.stats[STAT_WEAPON]]){
		// FIXME(sigrid) - reset lerp
	}
}

/*
=====================
CL_NewTranslation
=====================
*/
static void CL_NewTranslation (int slot)
{
	int		i, j;
	int		top, bottom;
	byte	*dest, *source;
	
	if (slot > cl.maxclients)
		fatal ("CL_NewTranslation: slot > cl.maxclients");
	dest = cl.scores[slot].translations;
	source = vid.colormap;
	memcpy(dest, vid.colormap, sizeof cl.scores[slot].translations);
	top = cl.scores[slot].colors & 0xf0;
	bottom = (cl.scores[slot].colors &15)<<4;

	for (i=0 ; i<VID_GRADES ; i++, dest += 256, source+=256)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			memcpy (dest + TOP_RANGE, source + top, 16);
		else
			for (j=0 ; j<16 ; j++)
				dest[TOP_RANGE+j] = source[top+15-j];
				
		if (bottom < 128)
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		else
			for (j=0 ; j<16 ; j++)
				dest[BOTTOM_RANGE+j] = source[bottom+15-j];		
	}
}

/*
=====================
CL_ParseStatic
=====================
*/
static void CL_ParseStatic (int withbits)
{
	entity_t *ent;
	int		i;
		
	i = cl.num_statics;
	if (i >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[i];
	cl.num_statics++;
	CL_ParseBaseline(withbits, ent);

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->alpha = ent->baseline.alpha;
	ent->frame = ent->baseline.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);	
	R_AddEfrags (ent);
}

/*
===================
CL_ParseStaticSound
===================
*/
static void CL_ParseStaticSound (int large_sound)
{
	vec3_t		org;
	int			sound_num, vol, atten;

	MSG_ReadVec(cl.protocol, org);
	sound_num = large_sound ? MSG_ReadShort() : MSG_ReadByte();
	vol = MSG_ReadByte();
	atten = MSG_ReadByte();
	
	staticsfx(cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x) if(cl_shownet.value==2)Con_Printf ("%3d:%s\n", msg_readcount-1, x);

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int cmd, i, n;
	protocol_t *p;
	
//
// if recording demos, copy the message out
//
	if (cl_shownet.value == 1)
		Con_Printf ("%d ",net_message.cursize);
	else if (cl_shownet.value == 2)
		Con_Printf ("------------------\n");
	
	cl.onground = false;	// unless the server says otherwise	
//
// parse the message
//
	MSG_BeginReading ();
	
	while (1)
	{
		if (msg_badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte ();

		if (cmd < 0)
		{
			SHOWNET("END OF MESSAGE");
			return;		// end of message
		}

	// if the high bit of the command byte is set, it is a fast update
		if (cmd & U_SIGNAL)
		{
			SHOWNET("fast update");
			CL_ParseUpdate (cmd);
			continue;
		}

		SHOWNET(svc_strings[cmd]);
	
	// other commands
		switch (cmd)
		{
		default:
			Host_Error ("CL_ParseServerMessage: Illegible server message (cmd %d)\n", cmd);
			break;
			
		case svc_nop:
//			Con_Printf ("svc_nop\n");
			break;
			
		case svc_time:
			cl.mtime[1] = cl.mtime[0];
			cl.mtime[0] = MSG_ReadFloat ();			
			break;
			
		case svc_clientdata:
			CL_ParseClientdata((ushort)MSG_ReadShort());
			break;
		
		case svc_version:
			i = MSG_ReadLong();
			for(p = nil, n = 0; n < nelem(protos); n++){
				if(protos[n].version == i){
					p = &protos[n];
					break;
				}
			}
			if(p == nil)
				Host_Error("CL_ParseServerMessage: server uses unknown protocol %d\n", i);
			if(p->version != cl.protocol.version)
				Host_Error("CL_ParseServerMessage: server decided to switch protocol to %d mid-game?\n", i);
			break;
			
		case svc_disconnect:
			Host_EndGame ("Server disconnected\n");

		case svc_print:
			Con_Printf ("%s", MSG_ReadString ());
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString ());
			break;
			
		case svc_stufftext:
			Cbuf_AddText (MSG_ReadString ());
			break;
			
		case svc_damage:
			V_ParseDamage ();
			break;
			
		case svc_serverinfo:
			CL_ParseServerInfo ();
			vid.recalc_refdef = true;	// leave intermission full screen
			break;
			
		case svc_setangle:
			for (i=0 ; i<3 ; i++)
				cl.viewangles[i] = cl.protocol.MSG_ReadAngle ();
			break;
			
		case svc_setview:
			cl.viewentity = MSG_ReadShort ();
			break;
					
		case svc_lightstyle:
			i = MSG_ReadByte ();
			if (i >= Nlights)
				fatal ("svc_lightstyle > Nlights");
			strcpy(cl_lightstyle[i].map,  MSG_ReadString());
			cl_lightstyle[i].length = strlen(cl_lightstyle[i].map);
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_stopsound:
			i = MSG_ReadShort();
			stopsfx(i>>3, i&7);
			break;
		
		case svc_updatename:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatename > MAX_SCOREBOARD");
			strcpy (cl.scores[i].name, MSG_ReadString ());
			break;
			
		case svc_updatefrags:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
			cl.scores[i].frags = MSG_ReadShort ();
			break;			

		case svc_updatecolors:
			Sbar_Changed ();
			i = MSG_ReadByte ();
			if (i >= cl.maxclients)
				Host_Error ("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
			cl.scores[i].colors = MSG_ReadByte ();
			CL_NewTranslation (i);
			break;
			
		case svc_particle:
			R_ParseParticleEffect ();
			break;

		case svc_spawnbaseline:
		case svc_spawnbaseline2:
			i = MSG_ReadShort();
			// must use CL_EntityNum() to force cl.num_entities up
			CL_ParseBaseline(cmd == svc_spawnbaseline2, CL_EntityNum(i));
			break;
		case svc_spawnstatic:
		case svc_spawnstatic2:
			CL_ParseStatic(cmd == svc_spawnstatic2);
			break;
		case svc_temp_entity:
			CL_ParseTEnt();
			break;

		case svc_setpause:
			{
				cl.paused = MSG_ReadByte ();

				if (cl.paused)
					pausecd();
				else
					resumecd();
			}
			break;
			
		case svc_signonnum:
			i = MSG_ReadByte ();
			if (i <= cls.signon)
				Host_Error ("Received signon %d when at %d", i, cls.signon);
			cls.signon = i;
			CL_SignonReply ();
			break;

		case svc_killedmonster:
			cl.stats[STAT_MONSTERS]++;
			break;

		case svc_foundsecret:
			cl.stats[STAT_SECRETS]++;
			break;

		case svc_updatestat:
			i = MSG_ReadByte ();
			if (i < 0 || i >= MAX_CL_STATS)
				fatal ("svc_updatestat: %d is invalid", i);
			cl.stats[i] = MSG_ReadLong ();;
			break;
			
		case svc_spawnstaticsound:
		case svc_spawnstaticsound2:
			CL_ParseStaticSound(cmd == svc_spawnstaticsound2);
			break;

		case svc_cdtrack:
			cl.cdtrack = MSG_ReadByte ();
			MSG_ReadByte();	/* looptrack */
			if((cls.demoplayback || cls.demorecording) && cls.forcetrack > 0)
				playcd(cls.forcetrack, 1);
			else
				playcd(cl.cdtrack, 1);
			break;

		case svc_intermission:
			cl.intermission = 1;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			break;

		case svc_finale:
			cl.intermission = 2;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString ());			
			break;

		case svc_cutscene:
			cl.intermission = 3;
			cl.completed_time = cl.time;
			vid.recalc_refdef = true;	// go to full screen
			SCR_CenterPrint (MSG_ReadString ());			
			break;

		case svc_sellscreen:
			Cmd_ExecuteString ("help", src_command);
			break;
		}
	}
}

