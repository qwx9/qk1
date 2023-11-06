#include "quakedef.h"

qsocket_t	*net_activeSockets = nil;
qsocket_t	*net_freeSockets = nil;
int			net_numsockets = 0;

sizebuf_t		net_message;
int				net_activeconnections = 0;

int messagesSent = 0;
int messagesReceived = 0;
int unreliableMessagesSent = 0;
int unreliableMessagesReceived = 0;

static cvar_t	net_messagetimeout = {"net_messagetimeout","300"};
cvar_t	hostname = {"hostname", "UNNAMED"};

Netdrv netdrv[MAX_NET_DRIVERS] = {
	{
	"Loopback",
	false,
	Loop_Init,
	Loop_Connect,
	Loop_CheckNewConnections,
	Loop_GetMessage,
	Loop_SendMessage,
	Loop_SendUnreliableMessage,
	Loop_CanSendMessage,
	Loop_CanSendUnreliableMessage,
	Loop_Close,
	Loop_Shutdown
	}
#ifdef __plan9__
	,
	{
	"Datagram",
	false,
	Datagram_Init,
	Datagram_Connect,
	Datagram_CheckNewConnections,
	Datagram_GetMessage,
	Datagram_SendMessage,
	Datagram_SendUnreliableMessage,
	Datagram_CanSendMessage,
	Datagram_CanSendUnreliableMessage,
	Datagram_Close,
	Datagram_Shutdown
	}
#endif
};
int net_numdrivers = 0;

Landrv landrv[MAX_NET_DRIVERS] = {
#ifdef __plan9__
	{
	"UDP",
	false,
	UDP_Init,
	UDP_Shutdown,
	UDP_Connect,
	udpread,
	udpwrite,
	UDP_AddrToString,
	getip,
	UDP_AddrCompare,
	UDP_GetSocketPort,
	UDP_SetSocketPort
	}
#endif
};
int net_numlandrivers = 0;

// these two macros are to make the code more readable
#define sfunc	netdrv[sock->driver]
#define dfunc	netdrv[net_driverlevel]

int	net_driverlevel;


double			net_time;

double SetNetTime(void)
{
	net_time = dtime();
	return net_time;
}


/*
===================
NET_NewQSocket

Called by drivers when a new communications endpoint is required
The sequence and buffer fields will be filled in properly
===================
*/
qsocket_t *NET_NewQSocket (void)
{
	qsocket_t	*sock;

	if (net_freeSockets == nil)
		return nil;

	if (net_activeconnections >= svs.maxclients)
		return nil;

	// get one from free list
	sock = net_freeSockets;
	net_freeSockets = sock->next;

	// add it to active list
	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = false;
	sock->connecttime = net_time;
	strcpy(sock->address,"UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->driverdata = nil;
	sock->canSend = true;
	sock->sendNext = false;
	sock->lastMessageTime = net_time;
	sock->ackSequence = 0;
	sock->sendSequence = 0;
	sock->unreliableSendSequence = 0;
	sock->sendMessageLength = 0;
	sock->receiveSequence = 0;
	sock->unreliableReceiveSequence = 0;
	sock->receiveMessageLength = 0;
	sock->addr.fd = -1;

	return sock;
}


void NET_FreeQSocket(qsocket_t *sock)
{
	qsocket_t	*s;

	// remove it from active list
	if (sock == net_activeSockets)
		net_activeSockets = net_activeSockets->next;
	else
	{
		for (s = net_activeSockets; s; s = s->next)
			if (s->next == sock)
			{
				s->next = sock->next;
				break;
			}
		if (!s)
			fatal ("NET_FreeQSocket: not active\n");
	}
	if(sock->addr.fd >= 0)
		close(sock->addr.fd);

	// add it to free list
	sock->next = net_freeSockets;
	net_freeSockets = sock;
	sock->disconnected = true;
}

static void MaxPlayers_f (void)
{
	int 	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"maxplayers\" is \"%d\"\n", svs.maxclients);
		return;
	}

	if (sv.active)
	{
		Con_Printf ("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = atoi(Cmd_Argv(1));
	if (n < 1)
		n = 1;
	if (n > svs.maxclientslimit)
	{
		n = svs.maxclientslimit;
		Con_Printf ("\"maxplayers\" set to \"%d\"\n", n);
	}

	svs.maxclients = n;
	if (n == 1)
		setcvar ("deathmatch", "0");
	else
		setcvar ("deathmatch", "1");
}


static void NET_Port_f (void)
{
	int 	n;

	if (Cmd_Argc () != 2)
	{
		Con_Printf ("\"port\" is \"%s\"\n", myip.srv);
		return;
	}
	if(sv.active){
		Con_Printf("port cannot be changed while a server is running.\n");
		return;
	}

	n = atoi(Cmd_Argv(1));
	if (n < 1 || n > 65534)
	{
		Con_Printf ("Bad value, must be between 1 and 65534\n");
		return;
	}
	snprint(myip.srv, sizeof myip.srv, "%hud", n);
}

/*
===================
NET_Connect
===================
*/

qsocket_t *NET_Connect (char *host)
{
	qsocket_t		*ret;
	int				numdrivers = net_numdrivers;

	SetNetTime();

	if (host == nil || *host == 0)
		return nil;
	if(cistrcmp(host, "local") == 0)
		numdrivers = 1;

	for (net_driverlevel=0 ; net_driverlevel<numdrivers; net_driverlevel++)
	{
		if (netdrv[net_driverlevel].initialized == false)
			continue;
		ret = dfunc.Connect (host);
		if (ret)
			return ret;
	}
	
	return nil;
}

qsocket_t *NET_CheckNewConnections (void)
{
	qsocket_t	*ret;

	SetNetTime();

	for (net_driverlevel=0 ; net_driverlevel<net_numdrivers; net_driverlevel++)
	{
		if (netdrv[net_driverlevel].initialized == false)
			continue;
		if (net_driverlevel && !sv.active)
			continue;
		ret = dfunc.CheckNewConnections ();
		if(ret)
			return ret;
	}

	return nil;
}

/*
===================
NET_Close
===================
*/
void NET_Close (qsocket_t *sock)
{
	if(sock == nil)
		return;
	if(sock->disconnected)
		return;

	SetNetTime();

	// call the driver_Close function
	sfunc.Close (sock);

	NET_FreeQSocket(sock);
}


/*
=================
NET_GetMessage

If there is a complete message, return it in net_message

returns 0 if no data is waiting
returns 1 if a message was received
returns -1 if connection is invalid
=================
*/

extern void PrintStats(qsocket_t *s);

int	NET_GetMessage (qsocket_t *sock)
{
	int ret;

	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();

	ret = sfunc.QGetMessage(sock);

	// see if this connection has timed out
	if (ret == 0 && sock->driver)
	{
		if (net_time - sock->lastMessageTime > net_messagetimeout.value)
		{
			Con_DPrintf("NET_GetMessage: sock %#p timed out\n", sock);
			NET_Close(sock);
			return -1;
		}
	}


	if (ret > 0)
	{
		if (sock->driver)
		{
			sock->lastMessageTime = net_time;
			if (ret == 1)
				messagesReceived++;
			else if (ret == 2)
				unreliableMessagesReceived++;
		}
	}

	return ret;
}


/*
==================
NET_SendMessage

Try to send a complete length+message unit over the reliable stream.
returns 0 if the message cannot be delivered reliably, but the connection
		is still considered valid
returns 1 if the message was sent properly
returns -1 if the connection died
==================
*/

int NET_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;
	
	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.QSendMessage(sock, data);
	if (r == 1 && sock->driver)
		messagesSent++;

	return r;
}


int NET_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int		r;
	
	if (!sock)
		return -1;

	if (sock->disconnected)
	{
		Con_Printf("NET_SendUnreliableMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime();
	r = sfunc.SendUnreliableMessage(sock, data);
	if (r == 1 && sock->driver)
		unreliableMessagesSent++;
	
	return r;
}


/*
==================
NET_CanSendMessage

Returns true or false if the given qsocket can currently accept a
message to be transmitted.
==================
*/
qboolean NET_CanSendMessage (qsocket_t *sock)
{
	int		r;
	
	if (!sock)
		return false;

	if (sock->disconnected)
		return false;

	SetNetTime();

	r = sfunc.CanSendMessage(sock);

	return r;
}


int NET_SendToAll(sizebuf_t *data, int blocktime)
{
	double		start;
	int			i;
	int			count = 0;
	qboolean	state1 [MAX_SCOREBOARD];
	qboolean	state2 [MAX_SCOREBOARD];

	for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
	{
		if (!host_client->netconnection)
			continue;
		if (host_client->active)
		{
			if (host_client->netconnection->driver == 0)
			{
				NET_SendMessage(host_client->netconnection, data);
				state1[i] = true;
				state2[i] = true;
				continue;
			}
			count++;
			state1[i] = false;
			state2[i] = false;
		}
		else
		{
			state1[i] = true;
			state2[i] = true;
		}
	}

	start = dtime();
	while (count)
	{
		count = 0;
		for (i=0, host_client = svs.clients ; i<svs.maxclients ; i++, host_client++)
		{
			if (! state1[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					state1[i] = true;
					NET_SendMessage(host_client->netconnection, data);
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}

			if (! state2[i])
			{
				if (NET_CanSendMessage (host_client->netconnection))
				{
					state2[i] = true;
				}
				else
				{
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}
		}
		if ((dtime() - start) > blocktime)
			break;
	}
	return count;
}


//=============================================================================

/*
====================
NET_Init
====================
*/

void NET_Init (void)
{
	int			i;
	qsocket_t	*s;

	net_numsockets = svs.maxclientslimit;
	if (cls.state != ca_dedicated)
		net_numsockets++;

	SetNetTime();

	for (i = 0; i < net_numsockets; i++)
	{
		s = Hunk_Alloc(sizeof *s);
		s->next = net_freeSockets;
		net_freeSockets = s;
		s->disconnected = true;
	}

	// allocate space for network message buffer
	SZ_Alloc (&net_message, NET_MAXMESSAGE);

	Cvar_RegisterVariable (&net_messagetimeout);
	Cvar_RegisterVariable (&hostname);

	Cmd_AddCommand ("maxplayers", MaxPlayers_f);
	Cmd_AddCommand ("port", NET_Port_f);

	// initialize all the drivers
	for(net_numdrivers=0; netdrv[net_numdrivers].Init; net_numdrivers++){
		if(netdrv[net_numdrivers].Init() < 0)
			continue;
		netdrv[net_numdrivers].initialized = true;
	}
}

/*
====================
NET_Shutdown
====================
*/

void		NET_Shutdown (void)
{
	qsocket_t	*sock;

	SetNetTime();

	for (sock = net_activeSockets; sock; sock = sock->next)
		NET_Close(sock);

//
// shutdown the drivers
//
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers; net_driverlevel++)
	{
		if (netdrv[net_driverlevel].initialized == true)
		{
			netdrv[net_driverlevel].Shutdown ();
			netdrv[net_driverlevel].initialized = false;
		}
	}
}
