#include "quakedef.h"
#include <ip.h>

//#define DEBUG

// these two macros are to make the code more readable
#define sfunc	landrv[sock->landriver]
#define dfunc	landrv[net_landriverlevel]

static int net_landriverlevel;

/* statistic counters */
static int packetsSent;
static int packetsReSent;
static int packetsReceived;
static int receivedDuplicateCount;
static int shortPacketCount;
static int droppedDatagrams;

static int myDriverLevel;

static byte netbuf[4+4+NET_MAXMESSAGE];

extern int m_return_state;
extern int m_state;
extern bool m_return_onerror;
extern char m_return_reason[32];

static int
netpack(Addr *a, int f, uint seq, byte *buf, int n)
{
	hnputl(netbuf, NET_HEADERSIZE + n | f);
	hnputl(netbuf+4, seq);
	if(buf != nil)
		memcpy(netbuf+8, buf, n);
	return udpwrite(netbuf, NET_HEADERSIZE + n, a);
}

int Datagram_SendMessage (qsocket_t *s, sizebuf_t *data)
{
	unsigned int	n;
	unsigned int	eom;

#ifdef DEBUG
	if (data->cursize == 0)
		fatal("Datagram_SendMessage: zero length message\n");

	if (data->cursize > NET_MAXMESSAGE)
		fatal("Datagram_SendMessage: message too big %ud\n", data->cursize);

	if (s->canSend == false)
		fatal("SendMessage: called with canSend == false\n");
#endif

	memcpy(s->sendMessage, data->data, data->cursize);
	s->sendMessageLength = data->cursize;
	if(data->cursize <= MAX_DATAGRAM){
		n = data->cursize;
		eom = NFEOM;
	}else{
		n = MAX_DATAGRAM;
		eom = 0;
	}
	s->canSend = false;
	if(netpack(&s->addr, NFDAT|eom, s->sendSequence++, s->sendMessage, n) < 0)
		return -1;
	s->lastSendTime = net_time;
	packetsSent++;
	return 1;
}


int SendMessageNext (qsocket_t *s)
{
	unsigned int	n;
	unsigned int	eom;

	if(s->sendMessageLength <= MAX_DATAGRAM){
		n = s->sendMessageLength;
		eom = NFEOM;
	}else{
		n = MAX_DATAGRAM;
		eom = 0;
	}
	s->sendNext = false;
	if(netpack(&s->addr, NFDAT|eom, s->sendSequence++, s->sendMessage, n) < 0)
		return -1;
	s->lastSendTime = net_time;
	packetsSent++;
	return 1;
}


int ReSendMessage (qsocket_t *s)
{
	unsigned int	n;
	unsigned int	eom;

	if (s->sendMessageLength <= MAX_DATAGRAM)
	{
		n = s->sendMessageLength;
		eom = NFEOM;
	}
	else
	{
		n = MAX_DATAGRAM;
		eom = 0;
	}
	s->sendNext = false;
	if(netpack(&s->addr, NFDAT|eom, s->sendSequence-1, s->sendMessage, n) < 0)
		return -1;
	s->lastSendTime = net_time;
	packetsReSent++;
	return 1;
}


bool Datagram_CanSendMessage (qsocket_t *sock)
{
	if (sock->sendNext)
		SendMessageNext (sock);

	return sock->canSend;
}


bool Datagram_CanSendUnreliableMessage (qsocket_t *)
{
	return true;
}


int Datagram_SendUnreliableMessage (qsocket_t *s, sizebuf_t *data)
{
#ifdef DEBUG
	if (data->cursize == 0)
		fatal("Datagram_SendUnreliableMessage: zero length message\n");

	if (data->cursize > MAX_DATAGRAM)
		fatal("Datagram_SendUnreliableMessage: message too big %ud\n", data->cursize);
#endif

	if(netpack(&s->addr, NFUNREL, s->unreliableSendSequence++, data->data, data->cursize) < 0)
		return -1;
	packetsSent++;
	return 1;
}


int	Datagram_GetMessage (qsocket_t *sock)
{
	int n;
	unsigned int	flags;
	int				ret = 0;
	unsigned int	sequence;
	unsigned int	count;

	if (!sock->canSend)
		if ((net_time - sock->lastSendTime) > 1.0)
			ReSendMessage (sock);
	while(1)
	{
		if((n = udpread(netbuf, NET_MAXMESSAGE, &sock->addr)) == 0)
			break;

		if (n == -1)
		{
			Con_Printf("Read error\n");
			return -1;
		}

		if (n < NET_HEADERSIZE)
		{
			shortPacketCount++;
			continue;
		}

		n = nhgetl(netbuf);
		flags = n & (~NFMASK);
		n &= NFMASK;

		if (flags & NFCTL)
			continue;

		sequence = nhgetl(netbuf+4);
		packetsReceived++;

		if (flags & NFUNREL)
		{
			if (sequence < sock->unreliableReceiveSequence)
			{
				Con_DPrintf("Got a stale datagram\n");
				ret = 0;
				break;
			}
			if (sequence != sock->unreliableReceiveSequence)
			{
				count = sequence - sock->unreliableReceiveSequence;
				droppedDatagrams += count;
				Con_DPrintf("Dropped %ud datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;

			n -= NET_HEADERSIZE;

			SZ_Clear (&net_message);
			SZ_Write (&net_message, netbuf+8, n);

			ret = 2;
			break;
		}

		if (flags & NFACK)
		{
			if (sequence != (sock->sendSequence - 1))
			{
				Con_DPrintf("Stale ACK received\n");
				continue;
			}
			if (sequence == sock->ackSequence)
			{
				sock->ackSequence++;
				if (sock->ackSequence != sock->sendSequence)
					Con_DPrintf("ack sequencing error\n");
			}
			else
			{
				Con_DPrintf("Duplicate ACK received\n");
				continue;
			}
			sock->sendMessageLength -= MAX_DATAGRAM;
			if (sock->sendMessageLength > 0)
			{
				memcpy(sock->sendMessage, sock->sendMessage+MAX_DATAGRAM, sock->sendMessageLength);
				sock->sendNext = true;
			}
			else
			{
				sock->sendMessageLength = 0;
				sock->canSend = true;
			}
			continue;
		}

		if (flags & NFDAT)
		{
			netpack(&sock->addr, NFACK, sequence, nil, 0);
			if (sequence != sock->receiveSequence)
			{
				receivedDuplicateCount++;
				continue;
			}
			sock->receiveSequence++;

			n -= NET_HEADERSIZE;

			if (flags & NFEOM)
			{
				SZ_Clear(&net_message);
				SZ_Write(&net_message, sock->receiveMessage, sock->receiveMessageLength);
				SZ_Write(&net_message, netbuf+8, n);
				sock->receiveMessageLength = 0;

				ret = 1;
				break;
			}

			memcpy(sock->receiveMessage + sock->receiveMessageLength, netbuf+8, n);
			sock->receiveMessageLength += n;
			continue;
		}
	}

	if (sock->sendNext)
		SendMessageNext (sock);

	return ret;
}


void PrintStats(qsocket_t *s)
{
	Con_Printf("canSend = %4d   \n", s->canSend);
	Con_Printf("sendSeq = %4d   ", s->sendSequence);
	Con_Printf("recvSeq = %4d   \n", s->receiveSequence);
	Con_Printf("\n");
}

void NET_Stats_f (void)
{
	qsocket_t	*s;

	if (Cmd_Argc () == 1)
	{
		Con_Printf("unreliable messages sent   = %d\n", unreliableMessagesSent);
		Con_Printf("unreliable messages recv   = %d\n", unreliableMessagesReceived);
		Con_Printf("reliable messages sent     = %d\n", messagesSent);
		Con_Printf("reliable messages received = %d\n", messagesReceived);
		Con_Printf("packetsSent                = %d\n", packetsSent);
		Con_Printf("packetsReSent              = %d\n", packetsReSent);
		Con_Printf("packetsReceived            = %d\n", packetsReceived);
		Con_Printf("receivedDuplicateCount     = %d\n", receivedDuplicateCount);
		Con_Printf("shortPacketCount           = %d\n", shortPacketCount);
		Con_Printf("droppedDatagrams           = %d\n", droppedDatagrams);
	}
	else if(strcmp(Cmd_Argv(1), "*") == 0)
	{
		for (s = net_activeSockets; s; s = s->next)
			PrintStats(s);
		for (s = net_freeSockets; s; s = s->next)
			PrintStats(s);
	}
	else
	{
		for (s = net_activeSockets; s; s = s->next)
			if(cistrcmp(Cmd_Argv(1), s->address) == 0)
				break;
		if (s == nil)
			for (s = net_freeSockets; s; s = s->next)
				if(cistrcmp(Cmd_Argv(1), s->address) == 0)
					break;
		if (s == nil)
			return;
		PrintStats(s);
	}
}

int
Datagram_Init(void)
{
	int i;

	myDriverLevel = net_driverlevel;
	Cmd_AddCommand("net_stats", NET_Stats_f);

	for(i=0; i<net_numlandrivers; i++){
		if(landrv[i].Init() < 0)
			continue;
		landrv[i].initialized = true;
	}

	return 0;
}


void Datagram_Shutdown (void)
{
	int i;

	// shutdown the lan drivers
	for (i = 0; i < net_numlandrivers; i++)
	{
		if (landrv[i].initialized)
		{
			landrv[i].Shutdown ();
			landrv[i].initialized = false;
		}
	}
}

static qsocket_t *_Datagram_CheckNewConnections (void)
{
	Addr clientaddr;
	int			newsock;
	qsocket_t	*sock;
	qsocket_t	*s;
	int			len;
	int			command;
	int			control;

	memset(&clientaddr, 0, sizeof clientaddr);
	if(getnewcon(&clientaddr) == 0)
		return nil;
	SZ_Clear(&net_message);
	len = udpread(net_message.data, net_message.maxsize, &clientaddr);
	if (len < sizeof(s32int))
		goto done;
	net_message.cursize = len;

	MSG_BeginReading ();
	control = BigLong(*((int *)net_message.data));
	MSG_ReadLong();
	if (control == -1)
		goto done;
	if ((control & (~NFMASK)) !=  NFCTL)
		goto done;
	if ((control & NFMASK) != len)
		goto done;

	command = MSG_ReadByte();
	if (command == CQSVINFO)
	{
		if(strcmp(MSG_ReadString(), "QUAKE") != 0)
			goto done;
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CPSVINFO);
		MSG_WriteString(&net_message, dfunc.AddrToString(&myip));
		MSG_WriteString(&net_message, hostname.string);
		MSG_WriteString(&net_message, sv.name);
		MSG_WriteByte(&net_message, net_activeconnections);
		MSG_WriteByte(&net_message, svs.maxclients);
		MSG_WriteByte(&net_message, NETVERSION);
		*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
		dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		goto done;
	}

	if (command == CQPLINFO)
	{
		int			playerNumber;
		int			activeNumber;
		int			clientNumber;
		client_t	*client;

		playerNumber = MSG_ReadByte();
		activeNumber = -1;
		for (clientNumber = 0, client = svs.clients; clientNumber < svs.maxclients; clientNumber++, client++)
		{
			if (client->active)
			{
				activeNumber++;
				if (activeNumber == playerNumber)
					break;
			}
		}
		if (clientNumber == svs.maxclients)
			goto done;

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CPPLINFO);
		MSG_WriteByte(&net_message, playerNumber);
		MSG_WriteString(&net_message, client->name);
		MSG_WriteLong(&net_message, client->colors);
		MSG_WriteLong(&net_message, (int)client->edict->v.frags);
		MSG_WriteLong(&net_message, (int)(net_time - client->netconnection->connecttime));
		MSG_WriteString(&net_message, client->netconnection->address);
		*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
		dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);

		goto done;
	}

	if (command == CQRUINFO)
	{
		char	*prevCvarName;
		cvar_t	*var;

		// find the search start location
		prevCvarName = MSG_ReadString();
		if (*prevCvarName)
		{
			var = Cvar_FindVar (prevCvarName);
			if (!var)
				goto done;
			var = var->next;
		}
		else
			var = cvar_vars;

		// search for the next server cvar
		while (var)
		{
			if (var->server)
				break;
			var = var->next;
		}

		// send the response

		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CPRUINFO);
		if (var)
		{
			MSG_WriteString(&net_message, var->name);
			MSG_WriteString(&net_message, var->string);
		}
		*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
		dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);

		goto done;
	}

	if (command != CQCONNECT)
		goto done;

	if(strcmp(MSG_ReadString(), "QUAKE") != 0)
		goto done;

	if (MSG_ReadByte() != NETVERSION)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CPREJECT);
		MSG_WriteString(&net_message, "Incompatible version.\n");
		*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
		dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		goto done;
	}

	// see if this guy is already connected
	for (s = net_activeSockets; s; s = s->next)
	{
		if (s->driver != net_driverlevel)
			continue;
		if(strcmp(clientaddr.ip, s->addr.ip) != 0 || strcmp(clientaddr.srv, s->addr.srv) != 0)
			continue;
		// is this a duplicate connection reqeust?
		if(strcmp(clientaddr.srv, s->addr.srv) == 0
		&& net_time - s->connecttime < 2.0)
		{
			// yes, so send a duplicate reply
			SZ_Clear(&net_message);
			// save space for the header, filled in later
			MSG_WriteLong(&net_message, 0);
			MSG_WriteByte(&net_message, CPACCEPT);
			MSG_WriteLong(&net_message, dfunc.GetSocketPort(&myip));
			*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
			dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
			SZ_Clear(&net_message);
			goto done;
		}
		// it's somebody coming back in from a crash/disconnect
		// so close the old qsocket and let their retry get them back in
		NET_Close(s);
		goto done;
	}

	// allocate a QSocket
	sock = NET_NewQSocket ();
	if (sock == nil)
	{
		// no room; try to let him know
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CPREJECT);
		MSG_WriteString(&net_message, "Server is full.\n");
		*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
		dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
		SZ_Clear(&net_message);
		goto done;
	}

	// allocate a network socket
	newsock = 1;

	// everything is allocated, just fill in the details
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;
	memcpy(&sock->addr, &clientaddr, sizeof clientaddr);
	strcpy(sock->address, UDP_AddrToString(&clientaddr));

	// send him back the info about the server connection he has been allocated
	SZ_Clear(&net_message);
	// save space for the header, filled in later
	MSG_WriteLong(&net_message, 0);
	MSG_WriteByte(&net_message, CPACCEPT);
	MSG_WriteLong(&net_message, dfunc.GetSocketPort(&myip));
	*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
	dfunc.Write(net_message.data, net_message.cursize, &clientaddr);
	SZ_Clear(&net_message);

	return sock;
done:
	close(clientaddr.fd);
	return nil;
}

qsocket_t *Datagram_CheckNewConnections (void)
{
	qsocket_t *ret = nil;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
		if (landrv[net_landriverlevel].initialized)
			if ((ret = _Datagram_CheckNewConnections ()) != nil)
				break;
	return ret;
}

static qsocket_t *_Datagram_Connect (char *host)
{
	Addr sendaddr;
	Addr readaddr;
	qsocket_t	*sock;
	int			ret = 0;
	int			reps;
	double		start_time;
	int			control;
	char		*reason;

	memset(&sendaddr, 0, sizeof sendaddr);
	memset(&readaddr, 0, sizeof readaddr);

	// see if we can resolve the host name
	if (dfunc.getip(host, &sendaddr) == -1){
		return nil;
	}

	sock = NET_NewQSocket ();
	if (sock == nil)
		goto ErrorReturn2;
	sock->socket = 1;
	sock->landriver = net_landriverlevel;

	// connect to the host
	if (dfunc.Connect(&sendaddr) == -1)
		goto ErrorReturn;
	memcpy(&readaddr, &sendaddr, sizeof readaddr);

	// send the connection request
	Con_Printf("trying...\n"); SCR_UpdateScreen (false);
	start_time = net_time;

	UDP_Listen(1);
	for (reps = 0; reps < 3; reps++)
	{
		SZ_Clear(&net_message);
		// save space for the header, filled in later
		MSG_WriteLong(&net_message, 0);
		MSG_WriteByte(&net_message, CQCONNECT);
		MSG_WriteString(&net_message, "QUAKE");
		MSG_WriteByte(&net_message, NETVERSION);
		*((int *)net_message.data) = BigLong(NFCTL | (net_message.cursize & NFMASK));
		dfunc.Write(net_message.data, net_message.cursize, &sendaddr);
		SZ_Clear(&net_message);
		do
		{
			ret = dfunc.Read(net_message.data, net_message.maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0)
			{
				if (ret < sizeof(int))
				{
					ret = 0;
					continue;
				}

				net_message.cursize = ret;
				MSG_BeginReading ();

				control = BigLong(*((int *)net_message.data));
				MSG_ReadLong();
				if (control == -1)
				{
					ret = 0;
					continue;
				}
				if ((control & (~NFMASK)) !=  NFCTL)
				{
					ret = 0;
					continue;
				}
				if ((control & NFMASK) != ret)
				{
					ret = 0;
					continue;
				}
			}
		}
		while (ret == 0 && (SetNetTime() - start_time) < 2.5);
		if (ret)
			break;
		Con_Printf("still trying...\n"); SCR_UpdateScreen (false);
		start_time = SetNetTime();
	}
	/* bullshit workaround for non-plan9 servers replying from different
	 * ports.  because of this workaround, multiple instances on the same
	 * host all require different ports prior to connection.  if someone
	 * has a better solution, i'm all ears. */
	start_time = SetNetTime();
	do{
		if(getnewcon(&sendaddr) > 0){
			close(readaddr.fd);
			memcpy(&readaddr, &sendaddr, sizeof readaddr);
			break;
		}
		sleep(1);
	}while(SetNetTime() - start_time < 2.5);
	UDP_Listen(0);

	if (ret == 0)
	{
		reason = "No Response";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	if (ret == -1)
	{
		reason = "Network Error";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	ret = MSG_ReadByte();
	if (ret == CPREJECT)
	{
		reason = MSG_ReadString();
		Con_Printf(reason);
		strncpy(m_return_reason, reason, 31);
		goto ErrorReturn;
	}

	if (ret == CPACCEPT)
	{
		memcpy(&sock->addr, &readaddr, sizeof readaddr);
		dfunc.SetSocketPort (&sock->addr, MSG_ReadLong());
	}
	else
	{
		reason = "Bad Response";
		Con_Printf("%s\n", reason);
		strcpy(m_return_reason, reason);
		goto ErrorReturn;
	}

	strcpy(sock->address, dfunc.AddrToString(&sendaddr));

	Con_Printf ("Connection accepted\n");
	sock->lastMessageTime = SetNetTime();

	m_return_onerror = false;
	return sock;

ErrorReturn:
	close(readaddr.fd);
	NET_FreeQSocket(sock);
ErrorReturn2:
	if (m_return_onerror)
	{
		key_dest = key_menu;
		m_state = m_return_state;
		m_return_onerror = false;
	}
	return nil;
}

qsocket_t *Datagram_Connect (char *host)
{
	qsocket_t *ret = nil;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers; net_landriverlevel++)
		if (landrv[net_landriverlevel].initialized)
			if ((ret = _Datagram_Connect (host)) != nil)
				break;
	return ret;
}

void
Datagram_Close(qsocket_t *s)
{
	close(s->addr.fd);
}
