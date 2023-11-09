typedef struct Addr Addr;

struct Addr{
	int fd;
	char ip[46];
	char srv[6];
	char sys[52];
};
extern Addr myip;
extern char *netmtpt;

enum{
	NET_NAMELEN = 64,
	NET_MAXMESSAGE = 65000,
	NET_HEADERSIZE = 4 + 4,
	NET_DATAGRAMSIZE = MAX_DATAGRAM + NET_HEADERSIZE,

	/* netheader flags */
	NFMASK	= 0xffff,
	NFDAT	= 1<<16,
	NFACK	= 1<<17,
	NFNAK	= 1<<18,
	NFEOM	= 1<<19,
	NFUNREL	= 1<<20,
	NFCTL	= 1<<31,

	/* network info/connection protocol: used to find servers, get info
	 * about them and connect to them. once connected, the game protocol is
	 * used instead.
	 * game[s]: always "QUAKE" for qk1. was supposed to allow this protocol
	 * 	to be used in other games (hint: it never was)
	 * v[1]: NETVERSION
	 * addresses have two forms: "addr:port" and "port". the longer form is
	 * used to return the address of a server which is not running locally.
	 * the short form uses the address from which the request was made.
	 */
	NETVERSION = 3,
	CQCONNECT = 1,		/* game[s] v[1] */
	CQSVINFO  = 2,		/* game[s] v[1] */
	CQPLINFO  = 3,		/* plnum[1] */
	CQRUINFO  = 4,		/* rule[s] */
	CPACCEPT  = 1<<7 | 1,	/* port[4] */
	CPREJECT  = 1<<7 | 2,	/* reason[s] */
	CPSVINFO  = 1<<7 | 3,
	/* addr[s] host[s] map[s] curpl[1] maxpl[1] v[1] */
	CPPLINFO  = 1<<7 | 4,
	/* plnum[1] name[s] colors[4] frags[4] contime[4] addr[s] */
	CPRUINFO  = 1<<7 | 5,	/* rule[s] val[s] */

	MAX_NET_DRIVERS = 8,
};

typedef struct qsocket_s
{
	struct qsocket_s	*next;
	double			connecttime;
	double			lastMessageTime;
	double			lastSendTime;

	qboolean		disconnected;
	qboolean		canSend;
	qboolean		sendNext;
	qboolean		local;
	
	int				driver;
	int				landriver;
	int				socket;
	void			*driverdata;

	unsigned int	ackSequence;
	unsigned int	sendSequence;
	unsigned int	unreliableSendSequence;
	int				sendMessageLength;
	byte			sendMessage [NET_MAXMESSAGE];

	unsigned int	receiveSequence;
	unsigned int	unreliableReceiveSequence;
	int				receiveMessageLength;
	byte			receiveMessage [NET_MAXMESSAGE];

	Addr addr;
	int uready;
	char				address[NET_NAMELEN];
} qsocket_t;

extern qsocket_t	*net_activeSockets;
extern qsocket_t	*net_freeSockets;
extern int			net_numsockets;

typedef struct Landrv Landrv;
typedef struct Netdrv Netdrv;

struct Landrv{
	char *name;
	qboolean initialized;
	int	(*Init)(void);
	void	(*Shutdown)(void);
	int	(*Connect)(Addr *);
	int	(*Read)(uchar *, int, Addr *);
	int	(*Write)(uchar *, int, Addr *);
	char*	(*AddrToString)(Addr *);
	int	(*getip)(char *, Addr *);
	int	(*AddrCompare)(Addr *, Addr *);
	ushort	(*GetSocketPort)(Addr *);
	void	(*SetSocketPort)(Addr *, ushort);
};
extern int net_numlandrivers;
extern Landrv landrv[MAX_NET_DRIVERS];

struct Netdrv{
	char *name;
	qboolean initialized;
	int	(*Init)(void);
	qsocket_t*	(*Connect)(char *);
	qsocket_t*	(*CheckNewConnections)(void);
	int	(*QGetMessage)(qsocket_t *);
	int	(*QSendMessage)(qsocket_t *, sizebuf_t *);
	int	(*SendUnreliableMessage)(qsocket_t *, sizebuf_t *);
	qboolean	(*CanSendMessage)(qsocket_t *);
	qboolean	(*CanSendUnreliableMessage)(qsocket_t *);
	void	(*Close)(qsocket_t *);
	void	(*Shutdown)(void);
};
extern int net_numdrivers;
extern Netdrv netdrv[MAX_NET_DRIVERS];

extern int net_driverlevel;
extern cvar_t		hostname;
extern char			playername[];
extern int			playercolor;

extern int		messagesSent;
extern int		messagesReceived;
extern int		unreliableMessagesSent;
extern int		unreliableMessagesReceived;

qsocket_t *NET_NewQSocket (void);
void NET_FreeQSocket(qsocket_t *);
double SetNetTime(void);

//============================================================================
//
// public network functions
//
//============================================================================

extern	double		net_time;
extern	sizebuf_t	net_message;
extern	int			net_activeconnections;

void		NET_Init (void);
void		NET_Shutdown (void);

struct qsocket_s	*NET_CheckNewConnections (void);
// returns a new connection number if there is one pending, else -1

struct qsocket_s	*NET_Connect (char *host);
// called by client to connect to a host.  Returns -1 if not able to

qboolean NET_CanSendMessage (qsocket_t *sock);
// Returns true or false if the given qsocket can currently accept a
// message to be transmitted.

int			NET_GetMessage (struct qsocket_s *sock);
// returns data in net_message sizebuf
// returns 0 if no data is waiting
// returns 1 if a message was received
// returns 2 if an unreliable message was received
// returns -1 if the connection died

int			NET_SendMessage (struct qsocket_s *sock, sizebuf_t *data);
int			NET_SendUnreliableMessage (struct qsocket_s *sock, sizebuf_t *data);
// returns 0 if the message connot be delivered reliably, but the connection
//		is still considered valid
// returns 1 if the message was sent properly
// returns -1 if the connection died

int			NET_SendToAll(sizebuf_t *data, int blocktime);
// This is a reliable *blocking* send to all attached clients.


void		NET_Close (struct qsocket_s *sock);
// if a dead connection is returned by a get or send function, this function
// should be called when it is convenient

// Server calls when a client is kicked off for a game related misbehavior
// like an illegal protocal conversation.  Client calls when disconnecting
// from a server.
// A netcon_t number will not be reused until this function is called for it

int			Datagram_Init (void);
void		Datagram_Listen (qboolean state);
qsocket_t	*Datagram_Connect (char *host);
qsocket_t 	*Datagram_CheckNewConnections (void);
int			Datagram_GetMessage (qsocket_t *sock);
int			Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data);
int			Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data);
qboolean	Datagram_CanSendMessage (qsocket_t *sock);
qboolean	Datagram_CanSendUnreliableMessage (qsocket_t *sock);
void	Datagram_Close(qsocket_t *);
void		Datagram_Shutdown (void);

int			Loop_Init (void);
void		Loop_Listen (qboolean state);
qsocket_t 	*Loop_Connect (char *host);
qsocket_t 	*Loop_CheckNewConnections (void);
int			Loop_GetMessage (qsocket_t *sock);
int			Loop_SendMessage (qsocket_t *sock, sizebuf_t *data);
int			Loop_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data);
qboolean	Loop_CanSendMessage (qsocket_t *sock);
qboolean	Loop_CanSendUnreliableMessage (qsocket_t *sock);
void		Loop_Close (qsocket_t *sock);
void		Loop_Shutdown (void);

int	UDP_Init(void);
void	UDP_Shutdown(void);
void	UDP_Listen(qboolean);
int	UDP_Connect(Addr *);
int	udpread(uchar *, int, Addr *);
int	udpwrite(uchar *, int, Addr *);
void	udpinfo(Addr*);
int	getnewcon(Addr*);
char*	UDP_AddrToString(Addr *);
int	getip(char *, Addr *);
int	UDP_AddrCompare(Addr *, Addr *);
ushort	UDP_GetSocketPort(Addr *);
void	UDP_SetSocketPort(Addr *, ushort);
