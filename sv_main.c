#include "quakedef.h"

enum {
	MAX_SIGNON_SIZE = 32000,
};

server_t		sv;
server_static_t	svs;

cvar_t developer = {"developer", "0"}; // show extra messages

// we can't just change protocol mid-game, so it's saved here first
static protocol_t *sv_protocol = &protos[PROTO_RMQ];

static char	localmodels[MAX_MODELS][8];			// inline model names for precache

static void SV_AllocSignon(void);

static sizebuf_t *
signon_overflow_sb(sizebuf_t *s)
{
	int frame;

	if(s == sv.signon){
		frame = sv.signon_frame;
		SV_AllocSignon();
		SZ_Write(sv.signon, s->data + frame, s->cursize - frame);
		s->cursize = frame;
	}
	return sv.signon;
}

static void
SV_AllocSignon(void)
{
	if(sv.numsignons == 256)
		Host_Error("too many signons");
	sv.signon = sv.signons[sv.numsignons++] = Hunk_Alloc(sizeof(*sv.signon) + MAX_SIGNON_SIZE);
	sv.signon->data = (byte*)(sv.signon + 1);
	sv.signon->maxsize = MAX_SIGNON_SIZE;
	sv.signon->name = "sv.signon";
	sv.signon->overflow_cb = signon_overflow_sb;
	sv.signon_frame = 0;
}

void
SV_SignonFrame(void)
{
	sv.signon_frame = sv.signon->cursize;
}

//============================================================================

static void
SV_Protocol_f(void)
{
	int i, n;

	i = Cmd_Argc();
	if(i == 1)
		Con_Printf("\"sv_protocol\" is \"%d\" (%s)\n", sv_protocol->version, sv_protocol->name);
	else if(i == 2){
		n = atoi(Cmd_Argv(1));
		for(i = 0; i < PROTO_NUM && n != protos[i].version; i++);
		if(i >= PROTO_NUM){
			Con_Printf("sv_protocol must be of values:");
			for(i = 0; i < PROTO_NUM; i++)
				Con_Printf(" %d", protos[i].version);
			Con_Printf("\n");
		}else{
			sv_protocol = &protos[i];
			if(sv.active)
				Con_Printf("changes will take effect on the next game\n");
		}
	}else
		Con_Printf("usage: sv_protocol <protocol>\n");
}

/*
===============
SV_Init
===============
*/
void SV_Init (void)
{
	int		i;
	extern	cvar_t	sv_maxvelocity;
	extern	cvar_t	sv_gravity;
	extern	cvar_t	sv_nostep;
	extern	cvar_t	sv_friction;
	extern	cvar_t	sv_edgefriction;
	extern	cvar_t	sv_stopspeed;
	extern	cvar_t	sv_maxspeed;
	extern	cvar_t	sv_accelerate;
	extern	cvar_t	sv_idealpitchscale;
	extern	cvar_t	sv_aim;

	Cvar_RegisterVariable (&sv_maxvelocity);
	Cvar_RegisterVariable (&sv_gravity);
	Cvar_RegisterVariable (&sv_friction);
	Cvar_RegisterVariable (&sv_edgefriction);
	Cvar_RegisterVariable (&sv_stopspeed);
	Cvar_RegisterVariable (&sv_maxspeed);
	Cvar_RegisterVariable (&sv_accelerate);
	Cvar_RegisterVariable (&sv_idealpitchscale);
	Cvar_RegisterVariable (&sv_aim);
	Cvar_RegisterVariable (&sv_nostep);
	Cvar_RegisterVariable (&developer);

	Cmd_AddCommand("sv_protocol", SV_Protocol_f);

	for (i=0 ; i<MAX_MODELS ; i++)
		sprint (localmodels[i], "*%d", i);
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count)
{
	int		i, v;

	if (sv.datagram.cursize > MAX_DATAGRAM-16)
		return;
	MSG_WriteByte(&sv.datagram, svc_particle);
	MSG_WriteVec(*sv.protocol, &sv.datagram, org);
	for (i=0 ; i<3 ; i++)
	{
		v = dir[i]*16;
		if (v > 127)
			v = 127;
		else if (v < -128)
			v = -128;
		MSG_WriteChar (&sv.datagram, v);
	}
	MSG_WriteByte (&sv.datagram, count);
	MSG_WriteByte (&sv.datagram, color);
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume,
    float attenuation)
{
    int         sound_num;
    int field_mask;
    int			i;
	int			ent;

	if (sv.datagram.cursize > MAX_DATAGRAM-21)
		return;

	// find precache number for sound
    for (sound_num=1 ; sound_num<MAX_SOUNDS
        && sv.sound_precache[sound_num] ; sound_num++)
        if (!strcmp(sample, sv.sound_precache[sound_num]))
            break;

    if ( sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num] )
    {
        Con_Printf ("SV_StartSound: %s not precacheed\n", sample);
        return;
    }

	volume = clamp(volume, 0, 255);
	attenuation = clamp(attenuation, 0, 4);
	channel = max(channel, 0);
	ent = NUM_FOR_EDICT(sv.pr, entity);
	if(ent >= sv.protocol->limit_entity || channel >= sv.protocol->limit_channel || sound_num >= sv.protocol->limit_sound)
		return;

	field_mask = 0;
	if(ent >= sv.protocol->large_entity || channel >= sv.protocol->large_channel)
		field_mask |= sv.protocol->fl_large_entity | sv.protocol->fl_large_channel;
	if(sound_num >= sv.protocol->large_sound)
		field_mask |= sv.protocol->fl_large_sound;
	if(volume != Spktvol)
		field_mask |= SND_VOLUME;
	if(attenuation != Spktatt)
		field_mask |= SND_ATTENUATION;

	// directed messages go only to the entity the are targeted on
	MSG_WriteByte(&sv.datagram, svc_sound);
	MSG_WriteByte(&sv.datagram, field_mask);
	if(field_mask & SND_VOLUME)
		MSG_WriteByte(&sv.datagram, volume);
	if(field_mask & SND_ATTENUATION)
		MSG_WriteByte(&sv.datagram, attenuation*64);
	if(field_mask & (sv.protocol->fl_large_entity | sv.protocol->fl_large_channel)){
		MSG_WriteShort(&sv.datagram, ent);
		MSG_WriteByte(&sv.datagram, channel);
	}else{
		MSG_WriteShort(&sv.datagram, ent<<3 | channel);
	}
	if(field_mask & sv.protocol->fl_large_sound)
		MSG_WriteShort(&sv.datagram, sound_num);
	else
		MSG_WriteByte(&sv.datagram, sound_num);
	for (i=0 ; i<3 ; i++)
		sv.protocol->MSG_WriteCoord (&sv.datagram, entity->v.origin[i]+0.5*(entity->v.mins[i]+entity->v.maxs[i]));
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo (client_t *client)
{
	char **s;
	int n;
	char tmp[64];

	MSG_WriteByte (&client->message, svc_print);
	snprint(tmp, sizeof(tmp), "%c\nNeinQuake %4.2f SERVER (%ud CRC)\n", 2, VERSION, crcn);
	MSG_WriteString (&client->message, tmp);

	MSG_WriteByte (&client->message, svc_serverinfo);
	MSG_WriteLong (&client->message, sv.protocol->version);
	sv.protocol->MSG_WriteProtocolInfo(&client->message, sv.protocol);
	MSG_WriteByte (&client->message, svs.maxclients);

	MSG_WriteByte (&client->message, (!coop.value && deathmatch.value) ? GAME_DEATHMATCH : GAME_COOP);

	MSG_WriteString (&client->message, PR_Str(sv.pr, sv.pr->edicts->v.message));

	for (n = 1, s = sv.model_precache+1 ; *s && n < sv.protocol->limit_model ; s++)
		MSG_WriteString(&client->message, *s);
	MSG_WriteByte(&client->message, 0);

	for (n = 1, s = sv.sound_precache+1 ; *s && n < sv.protocol->limit_sound ; s++)
		MSG_WriteString(&client->message, *s);
	MSG_WriteByte(&client->message, 0);

	// send music
	MSG_WriteByte (&client->message, svc_cdtrack);
	MSG_WriteByte (&client->message, sv.pr->edicts->v.sounds);
	MSG_WriteByte (&client->message, sv.pr->edicts->v.sounds);

	// set view
	MSG_WriteByte (&client->message, svc_setview);
	MSG_WriteShort (&client->message, NUM_FOR_EDICT(sv.pr, client->edict));

	MSG_WriteByte (&client->message, svc_signonnum);
	MSG_WriteByte (&client->message, 1);

	client->signon.state = SIGNON_KICK;
	client->spawned = false;		// need prespawn, spawn, etc
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient (int clientnum)
{
	int i, edictnum;
	float spawn_parms[Nparms];
	edict_t *ent;
	client_t *client;
	struct qsocket_s *netconnection;

	client = svs.clients + clientnum;

	Con_DPrintf("client %s connected\n", client->netconnection->address);

	edictnum = clientnum+1;

	ent = EDICT_NUM(sv.pr, edictnum);

	// set up the client_t
	netconnection = client->netconnection;

	if(sv.loadgame)
		memmove(spawn_parms, client->spawn_parms, sizeof spawn_parms);
	memset(client, 0, sizeof *client);
	client->netconnection = netconnection;

	strcpy(client->name, "unconnected");
	client->active = true;
	client->spawned = false;
	client->edict = ent;
	client->message.data = client->msgbuf;
	client->message.maxsize = sizeof client->msgbuf;
	client->message.allowoverflow = true;		// we can catch it

	if(sv.loadgame)
		memmove(client->spawn_parms, spawn_parms, sizeof spawn_parms);
	else{
		// call the progs to get default spawn parms for the new client
		PR_ExecuteProgram(sv.pr, sv.pr->global_struct->SetNewParms);
		for(i=0; i<Nparms; i++)
			client->spawn_parms[i] = (&sv.pr->global_struct->parm1)[i];
	}

	SV_SendServerinfo(client);
}


/*
===================
SV_CheckForNewClients

===================
*/
void SV_CheckForNewClients (void)
{
	struct qsocket_s	*ret;
	int				i;

	// check for new connections
	while (1)
	{
		ret = NET_CheckNewConnections ();
		if (!ret)
			break;

		// init a new client structure
		for (i=0 ; i<svs.maxclients ; i++)
			if (!svs.clients[i].active)
				break;
		if (i == svs.maxclients)
			fatal ("Host_CheckForNewClients: no free clients");

		svs.clients[i].netconnection = ret;
		SV_ConnectClient (i);

		net_activeconnections++;
	}
}



/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram

==================
*/
void SV_ClearDatagram (void)
{
	SZ_Clear (&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static int fatbytes;
static byte	*fatpvs;
static int fatpvs_size;

void SV_AddToFatPVS (vec3_t org, mnode_t *node, model_t *m)
{
	int		i, sz;
	byte	*pvs;
	mplane_t	*plane;
	float	d;

	while (1)
	{
		// if this is a leaf, accumulate the pvs bits
		if (node->contents < 0)
		{
			if (node->contents != CONTENTS_SOLID)
			{
				pvs = Mod_LeafPVS ( (mleaf_t *)node, m, &sz);
				for (i=0 ; i<fatbytes && i<sz ; i++)
					fatpvs[i] |= pvs[i];
			}
			return;
		}

		plane = node->plane;
		d = DotProduct (org, plane->normal) - plane->dist;
		if (d > 8)
			node = node->children[0];
		else if (d < -8)
			node = node->children[1];
		else
		{	// go down both
			SV_AddToFatPVS (org, node->children[0], m);
			node = node->children[1];
		}
	}
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
byte *SV_FatPVS (vec3_t org, model_t *m)
{
	fatbytes = (m->numleafs+31)>>3;
	if(fatpvs == nil || fatbytes > fatpvs_size){
		fatpvs = realloc(fatpvs, fatbytes);
		fatpvs_size = fatbytes;
	}
	memset(fatpvs, 0, fatbytes);
	SV_AddToFatPVS(org, m->nodes, m);
	return fatpvs;
}

//=============================================================================


/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient (edict_t	*clent, sizebuf_t *msg)
{
	u32int	bits;
	int		e, i, model, alpha;
	eval_t	*v;
	byte	*pvs;
	vec3_t	org;
	float	miss;
	edict_t	*ent;

	// find the client's PVS
	VectorAdd (clent->v.origin, clent->v.view_ofs, org);
	pvs = SV_FatPVS (org, sv.worldmodel);

	// send over all entities (excpet the client) that touch the pvs
	ent = NEXT_EDICT(sv.pr, sv.pr->edicts);
	for (e=1 ; e<sv.pr->num_edicts ; e++, ent = NEXT_EDICT(sv.pr, ent))
	{
		// ignore if not touching a PV leaf
		model = ent->v.modelindex;

		v = GetEdictFieldValue(sv.pr, ent, "alpha");
		alpha = v ? f2alpha(v->_float) : DEFAULT_ALPHA;

		if (ent != clent)	// clent is ALLWAYS sent
		{
			if((int)ent->v.effects == EF_NODRAW)
				continue;
			// ignore ents without visible models
			if(!model || !*PR_Str(sv.pr, ent->v.model))
				continue;
			if(model >= sv.protocol->limit_model)
				continue;
			if(!SV_FindTouchedLeafs(ent, sv.worldmodel->nodes, pvs))
				continue;
			if((int)ent->v.effects == 0 && zeroalpha(alpha))
				continue;
		}

		if (msg->cursize + 18 > msg->maxsize)
		{
			Con_Printf ("packet overflow\n");
			return;
		}

		// send an update
		bits = 0;

		for (i=0 ; i<3 ; i++)
		{
			miss = ent->v.origin[i] - ent->baseline.origin[i];
			if ( miss < -0.1 || miss > 0.1 )
				bits |= U_ORIGIN1<<i;
		}
		if (ent->v.angles[0] != ent->baseline.angles[0])
			bits |= U_ANGLE1;
		if (ent->v.angles[1] != ent->baseline.angles[1])
			bits |= U_ANGLE2;
		if (ent->v.angles[2] != ent->baseline.angles[2])
			bits |= U_ANGLE3;
		if (ent->v.movetype == MOVETYPE_STEP)
			bits |= U_NOLERP;	// don't mess up the step animation
		if (ent->v.colormap != ent->baseline.colormap)
			bits |= U_COLORMAP;
		if (ent->v.skin != ent->baseline.skin)
			bits |= U_SKIN;
		if (ent->v.frame != ent->baseline.frame)
			bits |= U_FRAME;
		if (ent->v.effects != ent->baseline.effects)
			bits |= U_EFFECTS;
		if (model != ent->baseline.modelindex)
			bits |= U_MODEL;
		if (alpha != ent->baseline.alpha)
			bits |= sv.protocol->fl_alpha;
		if (e >= 256)
			bits |= U_LONGENTITY;
		if(bits & U_FRAME){
			if(ent->v.frame >= sv.protocol->limit_frame)
				bits ^= U_FRAME;
			else if(ent->v.frame >= sv.protocol->large_frame)
				bits |= sv.protocol->fl_large_frame;
		}
		if(bits & U_MODEL){
			if(model >= sv.protocol->limit_model)
				bits ^= U_MODEL;
			else if(model >= sv.protocol->large_model)
				bits |= sv.protocol->fl_large_model;
		}
		if(bits >= (1<<8)){
			bits |= U_MOREBITS;
			if(bits >= (1<<16)){
				bits |= U_MOREBITS2;
				if(bits >= (1<<24))
					bits |= U_MOREBITS3;
			}
		}

		// write the message
		MSG_WriteByte (msg, bits | U_SIGNAL);

		if (bits & U_MOREBITS){
			MSG_WriteByte(msg, bits>>8);
			if (bits & U_MOREBITS2){
				MSG_WriteByte(msg, bits>>16);
				if(bits & U_MOREBITS3)
					MSG_WriteByte(msg, bits>>24);
			}
		}
		((bits & U_LONGENTITY) ? MSG_WriteShort : MSG_WriteByte)(msg, e);
		if (bits & U_MODEL)
			MSG_WriteByte(msg, model);
		if (bits & U_FRAME)
			MSG_WriteByte(msg, ent->v.frame);
		if (bits & U_COLORMAP)
			MSG_WriteByte(msg, ent->v.colormap);
		if (bits & U_SKIN)
			MSG_WriteByte(msg, ent->v.skin);
		if (bits & U_EFFECTS)
			MSG_WriteByte(msg, ent->v.effects);
		if (bits & U_ORIGIN1)
			sv.protocol->MSG_WriteCoord(msg, ent->v.origin[0]);
		if (bits & U_ANGLE1)
			sv.protocol->MSG_WriteAngle(msg, ent->v.angles[0]);
		if (bits & U_ORIGIN2)
			sv.protocol->MSG_WriteCoord (msg, ent->v.origin[1]);
		if (bits & U_ANGLE2)
			sv.protocol->MSG_WriteAngle(msg, ent->v.angles[1]);
		if (bits & U_ORIGIN3)
			sv.protocol->MSG_WriteCoord(msg, ent->v.origin[2]);
		if (bits & U_ANGLE3)
			sv.protocol->MSG_WriteAngle(msg, ent->v.angles[2]);
		if (bits & sv.protocol->fl_alpha)
			MSG_WriteByte(msg, alpha);
		if (bits & sv.protocol->fl_large_frame)
			MSG_WriteByte(msg, (int)ent->v.frame>>8);
		if (bits & sv.protocol->fl_large_model)
			MSG_WriteByte(msg, model>>8);
	}
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts (void)
{
	int		e;
	edict_t	*ent;

	ent = NEXT_EDICT(sv.pr, sv.pr->edicts);
	for (e=1 ; e<sv.pr->num_edicts ; e++, ent = NEXT_EDICT(sv.pr, ent))
	{
		ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
	}

}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg)
{
	u32int	bits;
	int		i;
	edict_t	*other;
	int		items, weaponmodel;
	eval_t	*val;

	// send a damage message
	if (ent->v.dmg_take || ent->v.dmg_save)
	{
		other = PROG_TO_EDICT(sv.pr, ent->v.dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v.dmg_save);
		MSG_WriteByte (msg, ent->v.dmg_take);
		for (i=0 ; i<3 ; i++)
			sv.protocol->MSG_WriteCoord (msg, other->v.origin[i] + 0.5*(other->v.mins[i] + other->v.maxs[i]));

		ent->v.dmg_take = 0;
		ent->v.dmg_save = 0;
	}

	// send the current viewpos offset from the view entity
	SV_SetIdealPitch ();		// how much to look up / down ideally

	// a fixangle might get lost in a dropped packet.  Oh well.
	if ( ent->v.fixangle )
	{
		MSG_WriteByte (msg, svc_setangle);
		for (i=0 ; i < 3 ; i++)
			sv.protocol->MSG_WriteAngle (msg, ent->v.angles[i]);
		ent->v.fixangle = 0;
	}

	bits = SU_ITEMS | SU_WEAPON;
	if (ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
		bits |= SU_VIEWHEIGHT;
	if (ent->v.idealpitch)
		bits |= SU_IDEALPITCH;

	// stuff the sigil bits into the high bits of items for sbar, or else
	// mix in items2
	val = GetEdictFieldValue(sv.pr, ent, "items2");
	items = (int)ent->v.items | (val ? ((int)val->_float << 23) : ((int)sv.pr->global_struct->serverflags << 28));
	weaponmodel = SV_ModelIndex(PR_Str(sv.pr, ent->v.weaponmodel));

	if ((int)ent->v.flags & FL_ONGROUND)
		bits |= SU_ONGROUND;
	if (ent->v.waterlevel >= 2)
		bits |= SU_INWATER;

	for (i=0 ; i<3 ; i++)
	{
		if (ent->v.punchangle[i])
			bits |= SU_PUNCH1<<i;
		if (ent->v.velocity[i])
			bits |= SU_VELOCITY1<<i;
	}

	if (bits & SU_WEAPON){
		if(weaponmodel >= sv.protocol->limit_model)
			bits ^= SU_WEAPON; // yikes.
		else if(weaponmodel >= sv.protocol->large_model)
			bits |= sv.protocol->fl_large_weaponmodel;
		if(!defalpha(ent->alpha))
			bits |= sv.protocol->fl_weapon_alpha;
	}
	if (ent->v.weaponframe){
		bits |= SU_WEAPONFRAME;
		if((int)ent->v.weaponframe >= sv.protocol->limit_frame)
			bits ^= SU_WEAPONFRAME;
		else if((int)ent->v.weaponframe >= sv.protocol->large_frame)
			bits |= sv.protocol->fl_large_weaponframe;
	}
	if (ent->v.armorvalue)
		bits |= SU_ARMOR;

	if(bits >= (1<<16)){
		bits |= SU_MOREBITS;
		if(bits >= (1<<24))
			bits |= SU_MOREBITS2;
	}

	// send the data

	MSG_WriteByte (msg, svc_clientdata);
	MSG_WriteShort (msg, bits);
	if(bits & SU_MOREBITS){
		MSG_WriteByte(msg, bits>>16);
		if(bits & SU_MOREBITS2)
			MSG_WriteByte(msg, bits>>24);
	}

	if (bits & SU_VIEWHEIGHT)
		MSG_WriteChar (msg, ent->v.view_ofs[2]);
	if (bits & SU_IDEALPITCH)
		MSG_WriteChar (msg, ent->v.idealpitch);

	for (i=0 ; i<3 ; i++)
	{
		if (bits & (SU_PUNCH1<<i))
			MSG_WriteChar (msg, ent->v.punchangle[i]);
		if (bits & (SU_VELOCITY1<<i))
			MSG_WriteChar (msg, ent->v.velocity[i]/16);
	}

	// [always sent]	if (bits & SU_ITEMS)
	MSG_WriteLong (msg, items);

	if (bits & SU_WEAPONFRAME)
		MSG_WriteByte (msg, ent->v.weaponframe);
	if (bits & SU_ARMOR)
		MSG_WriteByte (msg, ent->v.armorvalue);
	if (bits & SU_WEAPON)
		MSG_WriteByte (msg, weaponmodel);

	MSG_WriteShort (msg, ent->v.health);
	MSG_WriteByte (msg, ent->v.currentammo);
	MSG_WriteByte (msg, ent->v.ammo_shells);
	MSG_WriteByte (msg, ent->v.ammo_nails);
	MSG_WriteByte (msg, ent->v.ammo_rockets);
	MSG_WriteByte (msg, ent->v.ammo_cells);

	if (standard_quake)
	{
		MSG_WriteByte (msg, ent->v.weapon);
	}
	else
	{
		for(i=0;i<32;i++)
		{
			if ( ((int)ent->v.weapon) & (1<<i) )
			{
				MSG_WriteByte (msg, i);
				break;
			}
		}
	}

	if(bits & sv.protocol->fl_large_weaponmodel)
		MSG_WriteByte(msg, weaponmodel>>8);
	if(bits & sv.protocol->fl_large_weaponframe)
		MSG_WriteByte(msg, (int)ent->v.weaponframe>>8);
	if(bits & sv.protocol->fl_weapon_alpha)
		MSG_WriteByte(msg, ent->alpha);
}

/*
=======================
SV_SendClientDatagram
=======================
*/
bool SV_SendClientDatagram (client_t *client)
{
	static byte		buf[MAX_DATAGRAM_LOCAL];
	sizebuf_t	msg;

	msg.data = buf;
	// allow big messages locally, but otherwise (real world) we're forced to use 1400 at most
	msg.maxsize = client->netconnection->local ? MAX_DATAGRAM_LOCAL : MAX_DATAGRAM;
	msg.cursize = 0;

	MSG_WriteByte (&msg, svc_time);
	MSG_WriteFloat (&msg, sv.time);

	// add the client specific data to the datagram
	SV_WriteClientdataToMessage (client->edict, &msg);

	SV_WriteEntitiesToClient (client->edict, &msg);

	// copy the server datagram if there is space
	if (msg.cursize + sv.datagram.cursize < msg.maxsize)
		SZ_Write (&msg, sv.datagram.data, sv.datagram.cursize);

	// send the datagram
	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
	{
		SV_DropClient (true);// if the message couldn't send, kick off
		return false;
	}

	return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages (void)
{
	int			i, j;
	client_t *client;

	// check for changes to be sent over the reliable streams
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (host_client->old_frags != host_client->edict->v.frags)
		{
			for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
			{
				if (!client->active)
					continue;
				MSG_WriteByte (&client->message, svc_updatefrags);
				MSG_WriteByte (&client->message, i);
				MSG_WriteShort (&client->message, host_client->edict->v.frags);
			}

			host_client->old_frags = host_client->edict->v.frags;
		}
	}

	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
	{
		if (!client->active)
			continue;
		SZ_Write (&client->message, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	SZ_Clear (&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated client
message buffer
=======================
*/
void SV_SendNop (client_t *client)
{
	sizebuf_t	msg;
	byte		buf[4];

	msg.data = buf;
	msg.maxsize = sizeof buf;
	msg.cursize = 0;

	MSG_WriteChar (&msg, svc_nop);

	if (NET_SendUnreliableMessage (client->netconnection, &msg) == -1)
		SV_DropClient (true);	// if the message couldn't send, kick off
	client->last_message = realtime;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int i;
	sizebuf_t *s;

	// update frags, names, etc
	SV_UpdateToReliableMessages ();

	// build individual updates
	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		if (host_client->spawned)
		{
			if (!SV_SendClientDatagram (host_client))
				continue;
		}
		else
		{
		// the player isn't totally in the game yet
		// send small keepalive messages if too much time has passed
		// send a full message when the next signon stage has been requested
		// some other message data (name changes, etc) may accumulate
		// between signon stages
			if(host_client->signon.state == SIGNON_INIT){
				if(realtime - host_client->last_message > 5)
					SV_SendNop (host_client);
				continue;	// don't send out non-signon messages
			}
			if(host_client->signon.state == SIGNON_SENDING){
				if(host_client->signon.id < sv.numsignons){
					s = sv.signons[host_client->signon.id];
					if(host_client->message.cursize+s->cursize <= host_client->message.maxsize){
						SZ_Write(&host_client->message, s->data, s->cursize);
						host_client->signon.id++;
					}
				}
				if(host_client->signon.id == sv.numsignons)
					host_client->signon.state = SIGNON_DONE;
			}
			if(host_client->signon.state == SIGNON_DONE && host_client->message.cursize+2 < host_client->message.maxsize){
					MSG_WriteByte(&host_client->message, svc_signonnum);
					MSG_WriteByte(&host_client->message, 2);
					host_client->signon.state = SIGNON_KICK;
			}
		}

		// check for an overflowed message.  Should only happen
		// on a very fucked up connection that backs up a lot, then
		// changes level
		if (host_client->message.overflowed)
		{
			SV_DropClient (true);
			host_client->message.overflowed = false;
			continue;
		}

		if (host_client->message.cursize || host_client->dropasap)
		{
			if (!NET_CanSendMessage (host_client->netconnection))
			{
				//I_Printf ("can't write\n");
				continue;
			}

			if (host_client->dropasap)
				SV_DropClient (false);	// went to another level
			else
			{
				if(NET_SendMessage(host_client->netconnection, &host_client->message) < 0)
					SV_DropClient(true);	// if the message couldn't send, kick off
				SZ_Clear(&host_client->message);
				host_client->last_message = realtime;
				if(host_client->signon.state == SIGNON_KICK)
					host_client->signon.state = SIGNON_INIT;
			}
		}
	}


	// clear muzzle flashes
	SV_CleanupEnts ();
}


/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex (char *name)
{
	int		i;

	if (!name || !name[0])
		return 0;

	for (i=0 ; i<MAX_MODELS && sv.model_precache[i] ; i++)
		if (!strcmp(sv.model_precache[i], name))
			return i;
	if (i==MAX_MODELS || !sv.model_precache[i])
		fatal ("SV_ModelIndex: model %s not precached", name);
	return i;
}

/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
	u32int bits;
	int			i;
	edict_t			*svent;
	int				entnum;

	for (entnum = 0; entnum < sv.pr->num_edicts ; entnum++)
	{
		// get the current server version
		svent = EDICT_NUM(sv.pr, entnum);
		if (svent->free)
			continue;
		if (entnum > svs.maxclients && !svent->v.modelindex)
			continue;

		// create entity baseline
		VectorCopy (svent->v.origin, svent->baseline.origin);
		VectorCopy (svent->v.angles, svent->baseline.angles);
		svent->baseline.frame = svent->v.frame;
		svent->baseline.skin = svent->v.skin;
		if (entnum > 0 && entnum <= svs.maxclients)
		{
			svent->baseline.colormap = entnum;
			svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
			svent->baseline.alpha = DEFAULT_ALPHA;
		}
		else
		{
			svent->baseline.colormap = 0;
			svent->baseline.modelindex = SV_ModelIndex(PR_Str(sv.pr, svent->v.model));
			svent->baseline.alpha = svent->alpha;
		}

		bits = 0;
		if(svent->baseline.modelindex >= sv.protocol->limit_model)
			svent->baseline.modelindex = 0; // yikes.
		else if(svent->baseline.modelindex >= sv.protocol->large_model)
			bits |= sv.protocol->fl_large_baseline_model;
		if(svent->baseline.frame >= sv.protocol->limit_frame)
			svent->baseline.frame = 0; // yikes.
		else if(svent->baseline.frame >= sv.protocol->large_frame)
			bits |= sv.protocol->fl_large_baseline_frame;
		if(!defalpha(svent->baseline.alpha))
			bits |= sv.protocol->fl_baseline_alpha;

		// add to the message
		SV_SignonFrame();
		MSG_WriteByte(sv.signon, bits ? svc_spawnbaseline2 : svc_spawnbaseline);
		MSG_WriteShort(sv.signon, entnum);
		if(bits != 0)
			MSG_WriteByte(sv.signon, bits);

		((bits & sv.protocol->fl_large_baseline_model) ? MSG_WriteShort : MSG_WriteByte)
			(sv.signon, svent->baseline.modelindex);
		((bits & sv.protocol->fl_large_baseline_frame) ? MSG_WriteShort : MSG_WriteByte)
			(sv.signon, svent->baseline.frame);
		MSG_WriteByte(sv.signon, svent->baseline.colormap);
		MSG_WriteByte(sv.signon, svent->baseline.skin);
		for(i = 0; i < 3; i++){
			sv.protocol->MSG_WriteCoord(sv.signon, svent->baseline.origin[i]);
			sv.protocol->MSG_WriteAngle(sv.signon, svent->baseline.angles[i]);
		}
		if(bits & sv.protocol->fl_baseline_alpha)
			MSG_WriteByte(sv.signon, svent->baseline.alpha);
	}
}


/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect (void)
{
	uchar	data[128];
	sizebuf_t	msg;

	msg.data = data;
	msg.cursize = 0;
	msg.maxsize = sizeof data;

	MSG_WriteChar (&msg, svc_stufftext);
	MSG_WriteString (&msg, "reconnect\n");
	NET_SendToAll (&msg, 5);

	if (cls.state != ca_dedicated)
		Cmd_ExecuteString ("reconnect\n", src_command);
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms (void)
{
	int		i, j;

	svs.serverflags = sv.pr->global_struct->serverflags;

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->active)
			continue;

		// call the progs to get default spawn parms for the new client
		sv.pr->global_struct->self = EDICT_TO_PROG(sv.pr, host_client->edict);
		PR_ExecuteProgram(sv.pr, sv.pr->global_struct->SetChangeParms);
		for (j=0 ; j<Nparms ; j++)
			host_client->spawn_parms[j] = (&sv.pr->global_struct->parm1)[j];
	}
}


/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float		scr_centertime_off;

void SV_SpawnServer (char *server)
{
	edict_t		*ent;
	static char dummy[8] = {0};
	int			i;

	// let's not have any servers with no name
	if (hostname.string[0] == 0)
		setcvar ("hostname", "UNNAMED");
	scr_centertime_off = 0;

	Con_DPrintf("SV_SpawnServer: %s\n", server);
	svs.changelevel_issued = false;		// now safe to issue another

	// tell all connected clients that we are going to a new level
	if (sv.active)
	{
		SV_SendReconnect ();
	}

	// make cvars consistant
	if (coop.value)
		setcvarv ("deathmatch", 0);
	current_skill = (int)(skill.value + 0.5);
	if (current_skill < 0)
		current_skill = 0;
	if (current_skill > 3)
		current_skill = 3;

	setcvarv ("skill", (float)current_skill);

	// set up the new server
	Host_ClearMemory ();

	memset(&sv, 0, sizeof sv);

	strcpy(sv.name, server);
	sv.protocol = sv_protocol;

	// load progs to get entity field count
	sv.pr = PR_LoadProgs("progs.dat");

	// allocate server memory

	sv.datagram.maxsize = sizeof sv.datagram_buf;
	sv.datagram.cursize = 0;
	sv.datagram.data = sv.datagram_buf;
	sv.datagram.name = "sv.datagram";

	sv.reliable_datagram.maxsize = sizeof sv.reliable_datagram_buf;
	sv.reliable_datagram.cursize = 0;
	sv.reliable_datagram.data = sv.reliable_datagram_buf;
	sv.reliable_datagram.name = "sv.reliable_datagram";

	SV_AllocSignon();

	// leave slots at start for clients only
	sv.pr->num_edicts = svs.maxclients+1;
	for (i=0 ; i<svs.maxclients ; i++)
	{
		ent = EDICT_NUM(sv.pr, i+1);
		svs.clients[i].edict = ent;
	}

	sv.state = ss_loading;
	sv.paused = false;

	sv.time = 1.0;

	strcpy (sv.name, server);
	sprint (sv.modelname,"maps/%s.bsp", server);
	sv.worldmodel = Mod_ForName (sv.modelname, false);
	if (!sv.worldmodel)
	{
		Con_Printf ("Couldn't spawn server %s\n", sv.modelname);
		sv.active = false;
		return;
	}
	sv.models[1] = sv.worldmodel;

	// clear world interaction links
	SV_ClearWorld ();

	sv.sound_precache[0] = dummy;
	sv.model_precache[0] = dummy;
	sv.model_precache[1] = sv.modelname;
	for (i=1 ; i<sv.worldmodel->numsubmodels ; i++)
	{
		sv.model_precache[1+i] = localmodels[i];
		sv.models[i+1] = Mod_ForName (localmodels[i], false);
	}

	// load the rest of the entities
	ent = EDICT_NUM(sv.pr, 0);
	memset(&ent->v, 0, sv.pr->entityfields * 4);
	ent->free = false;
	ent->v.model = PR_SetStr(sv.pr, sv.worldmodel->name);
	ent->v.modelindex = 1;		// world model
	ent->v.solid = SOLID_BSP;
	ent->v.movetype = MOVETYPE_PUSH;

	if (coop.value)
		sv.pr->global_struct->coop = coop.value;
	else
		sv.pr->global_struct->deathmatch = deathmatch.value;

	sv.pr->global_struct->mapname = PR_SetStr(sv.pr, sv.name);

	// serverflags are for cross level information (sigils)
	sv.pr->global_struct->serverflags = svs.serverflags;

	ED_LoadFromFile(sv.pr, sv.worldmodel->entities);

	sv.active = true;

	// all setup is completed, any further precache statements are errors
	sv.state = ss_active;

	// run two frames to allow everything to settle
	host_frametime = 0.1;
	SV_Physics ();
	SV_Physics ();

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	UDP_Listen(svs.maxclients > 1);

	// send serverinfo to all connected clients
	for (i=0,host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		if (host_client->active)
			SV_SendServerinfo (host_client);

	IN_Grabm(1);
	Con_DPrintf("server spawned\n");
}

