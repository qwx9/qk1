#include "quakedef.h"

cvar_t  registered = {"registered","0"};

char	com_token[1024];

bool		standard_quake = true, rogue, hipnotic;

void
pal3torgbx(byte *in, pixel_t *out, int n, byte *pal, int palsz)
{
	int x;

	palsz *= 3;
	if(in < (byte*)out || in > (byte*)(out+n) || in+n < (byte*)out){
		while(n-- > 0){
			x = (*in++)*3;
			if(x >= palsz)
				x = 0;
			*out++ = 0xff<<24 | pal[x+0]<<16 | pal[x+1]<<8 | pal[x+2];
		}
	}else{
		while(n-- > 0){
			x = in[n]*3;
			if(x >= palsz)
				x = 0;
			out[n] = 0xff<<24 | pal[x+0]<<16 | pal[x+1]<<8 | pal[x+2];
		}
	}
}

void
paltorgbx(byte *in, pixel_t *out, int n, pixel_t *pal)
{
	if(in < (byte*)out || in > (byte*)(out+n) || in+n < (byte*)out){
		while(n-- > 0)
			*out++ = pal[*in++];
	}else{
		while(n-- > 0)
			out[n] = pal[in[n]];
	}
}

void
torgbx(byte *in, pixel_t *out, int n)
{
	paltorgbx(in, out, n, q1pal);
}

/*


All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

*/

//============================================================================


// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

short   (*BigShort) (short l);
short   (*LittleShort) (short l);
int	 (*BigLong) (int l);
int	 (*LittleLong) (int l);
float   (*BigFloat) (float l);
float   (*LittleFloat) (float l);

/*
==============================================================================

			MESSAGE IO FUNCTIONS

Handles byte ordering and avoids alignment errors
==============================================================================
*/

//
// writing functions
//

void MSG_WriteChar (sizebuf_t *sb, int c)
{
	byte	*buf;

#ifdef PARANOID
	if (c < -128 || c > 127)
		fatal ("MSG_WriteChar: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteByte (sizebuf_t *sb, int c)
{
	byte	*buf;

#ifdef PARANOID
	if (c < 0 || c > 255)
		fatal ("MSG_WriteByte: range error");
#endif

	buf = SZ_GetSpace (sb, 1);
	buf[0] = c;
}

void MSG_WriteShort (sizebuf_t *sb, int c)
{
	byte	*buf;

#ifdef PARANOID
	if (c < ((short)0x8000) || c > (short)0x7fff)
		fatal ("MSG_WriteShort: range error");
#endif

	buf = SZ_GetSpace (sb, 2);
	buf[0] = c&0xff;
	buf[1] = c>>8;
}

void MSG_WriteLong (sizebuf_t *sb, int c)
{
	byte	*buf;

	buf = SZ_GetSpace (sb, 4);
	buf[0] = c&0xff;
	buf[1] = (c>>8)&0xff;
	buf[2] = (c>>16)&0xff;
	buf[3] = c>>24;
}

void MSG_WriteFloat (sizebuf_t *sb, float f)
{
	union
	{
		float   f;
		int	 l;
	} dat;


	dat.f = f;
	dat.l = LittleLong (dat.l);

	SZ_Write (sb, &dat.l, 4);
}

void MSG_WriteString (sizebuf_t *sb, char *s)
{
	if (!s)
		SZ_Write (sb, "", 1);
	else
		SZ_Write (sb, s, strlen(s)+1);
}

void MSG_WriteCoord (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, (int)(f*8));
}

void MSG_WriteCoordInt32 (sizebuf_t *sb, float f)
{
	MSG_WriteLong (sb, Qrint(f*16));
}

void MSG_WriteAngle (sizebuf_t *sb, float f)
{
	MSG_WriteByte (sb, ((int)f*256/360) & 255);
}

void MSG_WriteAngleInt16 (sizebuf_t *sb, float f)
{
	MSG_WriteShort (sb, Qrint(f*65536.0/360.0) & 65535);
}

//
// reading functions
//
int					 msg_readcount;
bool		msg_badread;

void MSG_BeginReading (void)
{
	msg_readcount = 0;
	msg_badread = false;
}

// returns -1 and sets msg_badread if no more characters are available
int MSG_ReadChar (void)
{
	int	 c;

	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (signed char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadByte (void)
{
	int	 c;

	if (msg_readcount+1 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (unsigned char)net_message.data[msg_readcount];
	msg_readcount++;

	return c;
}

int MSG_ReadShort (void)
{
	int	 c;

	if (msg_readcount+2 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = (short)(net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8));

	msg_readcount += 2;

	return c;
}

int MSG_ReadLong (void)
{
	int	 c;

	if (msg_readcount+4 > net_message.cursize)
	{
		msg_badread = true;
		return -1;
	}

	c = net_message.data[msg_readcount]
	+ (net_message.data[msg_readcount+1]<<8)
	+ (net_message.data[msg_readcount+2]<<16)
	+ (net_message.data[msg_readcount+3]<<24);

	msg_readcount += 4;

	return c;
}

float MSG_ReadFloat (void)
{
	union
	{
		byte	b[4];
		float   f;
		int	 l;
	} dat;

	dat.b[0] =	  net_message.data[msg_readcount];
	dat.b[1] =	  net_message.data[msg_readcount+1];
	dat.b[2] =	  net_message.data[msg_readcount+2];
	dat.b[3] =	  net_message.data[msg_readcount+3];
	msg_readcount += 4;

	dat.l = LittleLong (dat.l);

	return dat.f;
}

char *MSG_ReadString (void)
{
	static char	 string[2048];
	int			 l,c;

	for(l = 0; l < (int)sizeof(string)-1; l++){
		if((c = MSG_ReadChar()) == -1 || c == 0)
			break;
		string[l] = c;
	}
	string[l] = 0;

	return string;
}

float MSG_ReadCoord (void)
{
	return MSG_ReadShort() * 1.0 / 8.0;
}

float MSG_ReadCoordInt32 (void)
{
	return MSG_ReadLong() * 1.0 / 16.0;
}

float MSG_ReadAngle (void)
{
	return MSG_ReadChar() * 360.0 / 256.0;
}

float MSG_ReadAngleInt16 (void)
{
	return MSG_ReadShort() * 360.0 / 65536.0;
}

//===========================================================================

void *
Arr_AllocExtra(void *arr, int *nel, int needextra)
{
	while(needextra > 0){
		arr = Hunk_Double(arr);
		needextra -= *nel;
		*nel *= 2;
	}
	return arr;
}

void
SZ_Alloc(sizebuf_t *buf, int startsize)
{
	if(startsize < 256)
		startsize = 256;
	buf->data = Hunk_Alloc(startsize);
	buf->maxsize = startsize;
	buf->cursize = 0;
	setmalloctag(buf->data, getcallerpc(&buf));
}

void
SZ_Clear(sizebuf_t *buf)
{
	buf->cursize = 0;
}

void *
SZ_GetSpace(sizebuf_t *buf, int length)
{
	void *data;
	sizebuf_t *next;

again:
	if(buf->cursize + length > buf->maxsize){
		if(buf->overflow_cb != nil){
			next = buf->overflow_cb(buf);
			if(next != buf){
				buf = next;
				goto again;
			}
		}
		if(!buf->allowoverflow)
			Host_Error("SZ_GetSpace: %s: overflow without allowoverflow set", buf->name);
		if(length > buf->maxsize)
			Host_Error("SZ_GetSpace: %s: %d is > full buffer size", buf->name, length);
		buf->overflowed = true;
		Con_Printf("SZ_GetSpace: %s: overflow", buf->name);
		SZ_Clear(buf);
	}
	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void
SZ_Write(sizebuf_t *buf, void *data, int length)
{
	memcpy(SZ_GetSpace(buf, length), data, length);
}

void
SZ_Print(sizebuf_t *buf, char *data)
{
	int len;

	len = strlen(data)+1;

	if(buf->data[buf->cursize-1])
		memcpy(SZ_GetSpace(buf, len), data, len);	// no trailing 0
	else
		memcpy((uchar *)SZ_GetSpace(buf, len-1)-1, data, len);	// write over trailing 0
}

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int			 c;
	int			 len;

	len = 0;
	com_token[0] = 0;

	if (data == nil)
		return nil;

	// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return nil;					// end of file;
		data++;
	}

	// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

	// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

	// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c==':')
			break;
	} while (c>32);

	com_token[len] = 0;
	return data;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
char *
va(char *fmt, ...)
{
	va_list arg;
	static char s[1024];

	va_start(arg, fmt);
	vsnprint(s, sizeof s, fmt, arg);
	va_end(arg);

	return s;
}

float
f32le(u32int u)
{
	union {
		float f;
		u32int u;
	}x;
	x.u = u;
	return x.f;
}
