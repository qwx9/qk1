#include "quakedef.h"

Addr myip;

int
UDP_Init(void)
{
	return -1;
}

void
UDP_Shutdown(void)
{
}

void
UDP_Listen(bool on)
{
	USED(on);
}

int
UDP_Connect(Addr *a)
{
	USED(a);
	return -1;
}

void
UDP_Close(Addr *a)
{
	USED(a);
}

int
UDP_GetNewConn(Addr *a)
{
	USED(a);
	return 0;
}

int
UDP_Read(byte *buf, int len, Addr *a)
{
	USED(buf);
	USED(len);
	USED(a);
	return -1;
}

int
UDP_Write(byte *buf, int len, Addr *a)
{
	USED(buf);
	USED(len);
	USED(a);
	return -1;
}

char *
UDP_AddrToString(Addr *a)
{
	USED(a);
	return "";
}

int
UDP_Broadcast(byte *buf, int len)
{
	USED(buf);
	USED(len);
	return -1;
}

int
UDP_GetIP(char *s, Addr *a)
{
	USED(s);
	USED(a);
	return -1;
}

int
UDP_AddrCompare(Addr *a1, Addr *a2)
{
	USED(a1);
	USED(a2);
	return 0;
}

u16int
UDP_GetSocketPort(Addr *a)
{
	USED(a);
	return 0;
}

void
UDP_SetSocketPort(Addr *a, u16int port)
{
	USED(a);
	USED(port);
}
