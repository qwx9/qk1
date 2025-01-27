#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "quakedef.h"

#define	PACKET_HEADER	8

/*

packet header
-------------
31	sequence
1	does this message contain a reliable payload
31	acknowledge sequence
1	acknowledge receipt of even/odd message
16  qport

The remote connection never knows if it missed a reliable message, the
local side detects that it has been dropped by seeing a sequence acknowledge
higher thatn the last reliable sequence, but without the correct evon/odd
bit for the reliable set.

If the sender notices that a reliable message has been dropped, it will be
retransmitted.  It will not be retransmitted again until a message after
the retransmit has been acknowledged and the reliable still failed to get there.

if the sequence number is -1, the packet should be handled without a netcon

The reliable message can be added to at any time by doing
MSG_Write* (&netchan->message, <data>).

If the message buffer is overflowed, either by a single message, or by
multiple frames worth piling up while the last reliable transmit goes
unacknowledged, the netchan signals a fatal error.

Reliable messages are allways placed first in a packet, then the unreliable
message is included if there is sufficient room.

To the receiver, there is no distinction between the reliable and unreliable
parts of the message, they are just processed out as a single larger message.

Illogical packet sequence numbers cause the packet to be dropped, but do
not kill the connection.  This, combined with the tight window of valid
reliable acknowledgement numbers provides protection against malicious
address spoofing.

The qport field is a workaround for bad address translating routers that
sometimes remap the client's source port on a packet during gameplay.

If the base part of the net address matches and the qport matches, then the
channel matches even if the IP port differs.  The IP port should be updated
to the new value before sending out any replies.


*/

int		net_drop;
cvar_t	showpackets = {"showpackets", "0"};
cvar_t	showdrop = {"showdrop", "0"};
cvar_t	qport = {"qport", "0"};

/*
===============
Netchan_Init

===============
*/
void Netchan_Init (void)
{
	int		port;

	// pick a port value that should be nice and random
	port = ((int)(getpid()+nrand(4096)*1000) * time(nil)) & 0xffff;

	Cvar_RegisterVariable (&showpackets);
	Cvar_RegisterVariable (&showdrop);
	Cvar_RegisterVariable (&qport);
	Cvar_SetValue("qport", port);
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand (netadr_t *adr, int length, byte *data)
{
	sizebuf_t	send;
	byte		send_buf[MAX_MSGLEN + PACKET_HEADER];

// write the packet header
	send.data = send_buf;
	send.maxsize = sizeof(send_buf);
	send.cursize = 0;
	
	MSG_WriteLong (&send, -1);	// -1 sequence means out of band
	SZ_Write (&send, data, length);

// send the datagram
	//zoid, no input in demo playback mode
	if(svonly || !cls.demoplayback)
		NET_SendPacket (send.cursize, send.data, adr);
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint (netadr_t *adr, char *format, ...)
{
	va_list		argptr;
	static char		string[8192];		// ??? why static?
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);


	Netchan_OutOfBand (adr, strlen(string), (byte *)string);
}


/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup (netchan_t *chan, netadr_t *adr, int qport)
{
	memset (chan, 0, sizeof(*chan));

	if(adr != nil)
		memmove(&chan->remote_address, adr, sizeof *adr);
	chan->last_received = realtime;
	
	chan->message.data = chan->message_buf;
	chan->message.allowoverflow = true;
	chan->message.maxsize = sizeof(chan->message_buf);

	chan->qport = qport;
	
	chan->rate = 1.0/2500;
}


/*
===============
Netchan_CanPacket

Returns true if the bandwidth choke isn't active
================
*/
#define	MAX_BACKUP	200
bool Netchan_CanPacket (netchan_t *chan)
{
	if (chan->cleartime < realtime + MAX_BACKUP*chan->rate)
		return true;
	return false;
}


/*
===============
Netchan_CanReliable

Returns true if the bandwidth choke isn't 
================
*/
bool Netchan_CanReliable (netchan_t *chan)
{
	if (chan->reliable_length)
		return false;			// waiting for ack
	return Netchan_CanPacket (chan);
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit (netchan_t *chan, int length, byte *data)
{
	sizebuf_t	send;
	byte		send_buf[MAX_MSGLEN + PACKET_HEADER];
	bool	send_reliable;
	unsigned	w1, w2;
	int			i;

// check for message overflow
	if (chan->message.overflowed)
	{
		chan->fatal_error = true;
		Con_Printf ("%s:Outgoing message overflow\n"
			, chan->remote_address.addr);
		return;
	}

// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
	&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_length && chan->message.cursize)
	{
		memmove (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
		send_reliable = true;
	}

// write the packet header
	send.data = send_buf;
	send.maxsize = sizeof(send_buf);
	send.cursize = 0;

	w1 = chan->outgoing_sequence | (send_reliable<<31);
	w2 = chan->incoming_sequence | (chan->incoming_reliable_sequence<<31);

	chan->outgoing_sequence++;

	MSG_WriteLong (&send, w1);
	MSG_WriteLong (&send, w2);

	// send the qport if we are a client
	if(!svonly)
		MSG_WriteShort (&send, cls.qport);

// copy the reliable message to the packet first
	if (send_reliable)
	{
		SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}
	
// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length)
		SZ_Write (&send, data, length);

// send the datagram
	i = chan->outgoing_sequence & (MAX_LATENT-1);
	chan->outgoing_size[i] = send.cursize;
	chan->outgoing_time[i] = realtime;

	//zoid, no input in demo playback mode
	if(svonly || !cls.demoplayback)
		NET_SendPacket (send.cursize, send.data, &chan->remote_address);

	if (chan->cleartime < realtime)
		chan->cleartime = realtime + send.cursize*chan->rate;
	else
		chan->cleartime += send.cursize*chan->rate;
	if(ServerPaused())
		chan->cleartime = realtime;

	if (showpackets.value)
		Con_Printf ("--> s=%d(%d) a=%d(%d) %d\n"
			, chan->outgoing_sequence
			, send_reliable
			, chan->incoming_sequence
			, chan->incoming_reliable_sequence
			, send.cursize);

}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
bool Netchan_Process (netchan_t *chan)
{
	unsigned		sequence, sequence_ack;
	unsigned		reliable_ack, reliable_message;
	int			qport;

	if((svonly || !cls.demoplayback) && !NET_CompareAdr(net_from, &chan->remote_address))
		return false;

// get sequence numbers		
	MSG_BeginReading ();
	sequence = MSG_ReadLong ();
	sequence_ack = MSG_ReadLong ();

	// read the qport if we are a server
	if(svonly){
		qport = MSG_ReadShort ();
		USED(qport);
	}

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1<<31);	
	sequence_ack &= ~(1<<31);	

	if (showpackets.value)
		Con_Printf ("<-- s=%d(%d) a=%d(%d) %d\n"
			, sequence
			, reliable_message
			, sequence_ack
			, reliable_ack
			, net_message.cursize);

//
// discard stale or duplicated packets
//
	if (sequence <= (unsigned)chan->incoming_sequence)
	{
		if (showdrop.value)
			Con_Printf ("%s:Out of order packet %d at %d\n"
				, chan->remote_address.addr
				,  sequence
				, chan->incoming_sequence);
		return false;
	}

//
// dropped packets don't keep the message from being used
//
	net_drop = sequence - (chan->incoming_sequence+1);
	if (net_drop > 0)
	{
		chan->drop_count += 1;

		if (showdrop.value)
			Con_Printf ("%s:Dropped %d packets at %d\n"
			, chan->remote_address.addr
			, sequence-(chan->incoming_sequence+1)
			, sequence);
	}

//
// if the current outgoing reliable message has been acknowledged
// clear the buffer to make way for the next
//
	if (reliable_ack == (unsigned)chan->reliable_sequence)
		chan->reliable_length = 0;	// it has been received
	
//
// if this message contains a reliable message, bump incoming_reliable_sequence 
//
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
		chan->incoming_reliable_sequence ^= 1;

//
// the message can now be read from the current message pointer
// update statistics counters
//
	chan->frame_latency = chan->frame_latency*OLD_AVG
		+ (chan->outgoing_sequence-sequence_ack)*(1.0-OLD_AVG);
	chan->frame_rate = chan->frame_rate*OLD_AVG
		+ (realtime-chan->last_received)*(1.0-OLD_AVG);		
	chan->good_count += 1;

	chan->last_received = realtime;

	return true;
}
