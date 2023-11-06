void *Arr_AllocExtra(void *arr, int *nel, int needextra);

typedef struct sizebuf_s
{
	qboolean	allowoverflow;	// if false, do a fatal
	qboolean	overflowed;		// set to true if the buffer size failed
	char	*name;
	byte	*data;
	int		maxsize;
	int		cursize;
} sizebuf_t;

void SZ_Alloc (sizebuf_t *buf, int startsize);
void SZ_Free (sizebuf_t *buf);
void SZ_Clear (sizebuf_t *buf);
void *SZ_GetSpace (sizebuf_t *buf, int length);
void SZ_Write (sizebuf_t *buf, void *data, int length);
void SZ_Print (sizebuf_t *buf, char *data);	// strcats onto the sizebuf

//============================================================================

typedef struct link_s
{
	struct link_s	*prev, *next;
} link_t;


void ClearLink (link_t *l);
void RemoveLink (link_t *l);
void InsertLinkBefore (link_t *l, link_t *before);
void InsertLinkAfter (link_t *l, link_t *after);

//============================================================================

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)<(b)?(a):(b))
#define clamp(x,a,b) (min(max((x),(a)),(b)))

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT	((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)
#define Q_MAXFLOAT 3.40282347e+38

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT 	((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)
#define Q_MINFLOAT 1.17549435e-38

//============================================================================

extern	short	(*BigShort) (short l);
extern	short	(*LittleShort) (short l);
extern	int	(*BigLong) (int l);
extern	int	(*LittleLong) (int l);
extern	float	(*BigFloat) (float l);
extern	float	(*LittleFloat) (float l);

float f32le(u32int u);

#define le32(p) (p += 4, (int)(p[-4]|p[-3]<<8|p[-2]<<16|p[-1]<<24))
#define le32u(p) (u32int)le32(p)
#define le16(p) (p += 2, (short)(p[-2]|p[-1]<<8))
#define le16u(p) (u16int)le16(p)
#define f32(p) f32le(le32(p))

//============================================================================

void MSG_WriteChar (sizebuf_t *sb, int c);
void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, char *s);
void MSG_WriteCoord (sizebuf_t *sb, float f);
void MSG_WriteCoordInt32 (sizebuf_t *sb, float f);
void MSG_WriteAngle (sizebuf_t *sb, float f);
void MSG_WriteAngleInt16 (sizebuf_t *sb, float f);

extern	int			msg_readcount;
extern	qboolean	msg_badread;		// set if a read goes beyond end of message

void MSG_BeginReading (void);
int MSG_ReadChar (void);
int MSG_ReadByte (void);
int MSG_ReadShort (void);
int MSG_ReadLong (void);
float MSG_ReadFloat (void);
char *MSG_ReadString (void);

float MSG_ReadCoord (void);
float MSG_ReadCoordInt32 (void);
float MSG_ReadAngle (void);
float MSG_ReadAngleInt16 (void);

#define MSG_ReadVec(proto, d) do{ \
	(d)[0] = (proto).MSG_ReadCoord(); \
	(d)[1] = (proto).MSG_ReadCoord(); \
	(d)[2] = (proto).MSG_ReadCoord(); \
}while(0)

#define MSG_WriteVec(proto, sb, s) do{ \
	(proto).MSG_WriteCoord(sb, (s)[0]); \
	(proto).MSG_WriteCoord(sb, (s)[1]); \
	(proto).MSG_WriteCoord(sb, (s)[2]); \
}while(0)

//============================================================================

extern	char		com_token[1024];
extern	qboolean	com_eof;

char *COM_Parse (char *data);
extern	int		com_argc;
extern	char	**com_argv;

#pragma varargck	argpos	va	1
char	*va(char *format, ...);
// does a varargs printf into a temp buffer

//============================================================================

struct cache_user_s;

extern cvar_t registered;

extern qboolean		standard_quake, rogue, hipnotic;
