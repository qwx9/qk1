#include "quakedef.h"
#include "qp.h"

static int con_linewidth;

static float con_cursorspeed = 4;

#define		CON_TEXTSIZE	16384
#define		MAXPRINTMSG		4096

bool 	con_forcedup;		// because no entities to refresh

int			con_totallines;		// total lines in console scrollback
int			con_backscroll;		// lines up from bottom to display
static int		con_current;		// where next message will be printed
static int		con_x;				// offset in current line for next print
static char		*con_text;

static cvar_t con_notifytime = {"con_notifytime","3"};		//seconds

#define	NUM_CON_TIMES 4
static float con_times[NUM_CON_TIMES];	// realtime time the line was generated
								// for transparent notify lines

static int con_vislines;

#define		MAXCMDLINE	256
extern	char	key_lines[32][MAXCMDLINE];
extern	int		edit_line;
extern	int		key_linepos;


bool	con_initialized;

int			con_notifylines;		// scan lines to clear for notify lines

extern void M_Menu_Main_f (cmd_t *c);

static Trie *conobj;

void
Con_AddObject(char *name, void *obj)
{
	conobj = qpset(conobj, name, 0, obj);
}

void *
Con_FindObject(char *name)
{
	char *k;
	void *v;

	if(qpget(conobj, name, 0, &k, &v) != 0)
		v = nil;
	return v;
}

int
Con_SearchObject(char *prefix, int len0, void (*f)(char *name, void *obj, void *aux), void *aux)
{
	char *k;
	void *v;
	int n, len, c;

	if(conobj == nil)
		return 0;

	k = nil;
	v = nil;
	len = 0;
	for(n = 0;;){
		if(qpnext(conobj, &k, &len, &v) < 0)
			break;
		c = -1;
		if(len0 == 0 || (len >= len0 && (c = strncmp(k, prefix, len0)) == 0)){
			f(k, v, aux);
			n++;
		}else if(c > 0)
			break;
	}

	return n;
}

void
Con_ToggleConsole_f(cmd_t *c)
{
	USED(c);
	if(key_dest == key_console){
		if(cls.state == ca_connected){
			key_dest = key_game;
			key_lines[edit_line][1] = 0;	// clear any typing
			key_linepos = 1;
			IN_Grabm(1);
		}else
			M_Menu_Main_f(c);
	}else{
		key_dest = key_console;
		IN_Grabm(0);
	}

	SCR_EndLoadingPlaque();
	memset(con_times, 0, sizeof con_times);
}

void
Con_Clear_f(cmd_t *c)
{
	USED(c);
	if(con_text)
		memset(con_text, ' ', CON_TEXTSIZE);
}

/*
================
Con_ClearNotify
================
*/
void
Con_ClearNotify(void)
{
	memset(con_times, 0, sizeof(con_times));
}


/*
================
Con_MessageMode_f
================
*/
extern bool team_message;

static void
Con_MessageMode_f(cmd_t *c)
{
	USED(c);
	key_dest = key_message;
	team_message = false;
}


/*
================
Con_MessageMode2_f
================
*/
static void
Con_MessageMode2_f(cmd_t *c)
{
	USED(c);
	key_dest = key_message;
	team_message = true;
}


/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void
Con_CheckResize(void)
{
	int		i, j, width, oldwidth, oldtotallines, numlines, numchars;
	char	tbuf[CON_TEXTSIZE];

	width = (vid.width >> 3) - 2;

	if (width == con_linewidth)
		return;

	if (width < 1)			// video hasn't been initialized yet
	{
		width = 38;
		con_linewidth = width;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		memset(con_text, ' ', CON_TEXTSIZE);
	}
	else
	{
		oldwidth = con_linewidth;
		con_linewidth = width;
		oldtotallines = con_totallines;
		con_totallines = CON_TEXTSIZE / con_linewidth;
		numlines = oldtotallines;

		if (con_totallines < numlines)
			numlines = con_totallines;

		numchars = oldwidth;

		if (con_linewidth < numchars)
			numchars = con_linewidth;

		memmove(tbuf, con_text, CON_TEXTSIZE);
		memset(con_text, ' ', CON_TEXTSIZE);

		for (i=0 ; i<numlines ; i++)
		{
			for (j=0 ; j<numchars ; j++)
			{
				con_text[(con_totallines - 1 - i) * con_linewidth + j] =
						tbuf[((con_current - i + oldtotallines) %
							  oldtotallines) * oldwidth + j];
			}
		}

		Con_ClearNotify ();
	}

	con_backscroll = 0;
	con_current = con_totallines - 1;
}


/*
================
Con_Init
================
*/
void
Con_Init(void)
{
	con_text = Hunk_Alloc(CON_TEXTSIZE);
	memset(con_text, ' ', CON_TEXTSIZE);
	con_linewidth = -1;
	Con_CheckResize ();

	Con_Printf ("Console initialized.\n");

	Cvar_RegisterVariable (&con_notifytime);

	Cmd_AddCommand ("toggleconsole", Con_ToggleConsole_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
	Cmd_AddCommand ("clear", Con_Clear_f);
	con_initialized = true;
}


/*
===============
Con_Linefeed
===============
*/
static void
Con_Linefeed(void)
{
	con_x = 0;
	con_current++;
	memset(&con_text[(con_current%con_totallines)*con_linewidth], ' ', con_linewidth);
}

/* handles cursor positioning, line wrapping, etc. all console printing must go
 * through this in order to be logged to disk. if no console is visible, the
 * notify window will pop up. */
static void
Con_Print(char *txt)
{
	int		y;
	int		c, l;
	static int	cr;
	int		mask;

	con_backscroll = 0;

	if (txt[0] == 1)
	{
		mask = 128;		// go to colored text
		localsfx ("misc/talk.wav");
		// play talk wav
		txt++;
	}
	else if (txt[0] == 2)
	{
		mask = 128;		// go to colored text
		txt++;
	}
	else
		mask = 0;


	while ( (c = *txt) )
	{
		// count word length
		for (l=0 ; l< con_linewidth ; l++)
			if ( txt[l] <= ' ')
				break;

		// word wrap
		if (l != con_linewidth && (con_x + l > con_linewidth) )
			con_x = 0;

		txt++;

		if (cr)
		{
			con_current--;
			cr = false;
		}


		if (!con_x)
		{
			Con_Linefeed ();
			// mark time for transparent overlay
			if (con_current >= 0)
				con_times[con_current % NUM_CON_TIMES] = realtime;
		}

		switch (c)
		{
		case '\n':
			con_x = 0;
			break;

		case '\r':
			con_x = 0;
			cr = 1;
			break;

		default:	// display character and advance
			y = con_current % con_totallines;
			con_text[y*con_linewidth+con_x] = c | mask;
			con_x++;
			if (con_x >= con_linewidth)
				con_x = 0;
			break;
		}

	}
}

/* Handles cursor positioning, line wrapping, etc */
void
Con_Printf(char *fmt, ...)
{
	va_list arg;
	char msg[MAXPRINTMSG];
	static bool	inupdate;

	va_start(arg, fmt);
	vsnprintf(msg, sizeof msg, fmt, arg);
	va_end(arg);

	if(!con_initialized)
		return;

	if(cls.state == ca_dedicated)
		return;		// no graphics mode

	// write it to the scrollable buffer
	Con_Print(msg);

	// update the screen if the console is displayed
	if(cls.signon != SIGNONS && !scr_disabled_for_loading){
		/* protect against infinite loop if something in
		 * SCR_UpdateScreen(...) calls Con_Printf() */
		if(!inupdate){
			inupdate = true;
			SCR_UpdateScreen(false);
			inupdate = false;
		}
	}
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void
Con_DPrintf(char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	int			n;

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	n = vsprintf (msg,fmt,argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
	if(developer.value > 1 && write(2, msg, n) < 0)
		setcvar("developer", "0");
}

/*
==============================================================================

DRAWING

==============================================================================
*/


/*
================
Con_DrawInput

The input line scrolls horizontally if typing goes beyond the right edge
================
*/
static void
Con_DrawInput(void)
{
	int		y;
	int		i;
	char	*text, c;

	if (key_dest != key_console && !con_forcedup)
		return;		// don't draw anything

	text = key_lines[edit_line];

	// add the cursor frame
	c = text[key_linepos];
	if((int)(realtime*con_cursorspeed)&1)
		text[key_linepos] = 11;

	//	prestep if horizontally scrolling
	if (key_linepos >= con_linewidth)
		text += 1 + key_linepos - con_linewidth;

	// draw it
	y = con_vislines-16;
	for(i=0 ; (c != 0 || i <= key_linepos) && i < con_linewidth && text[i] != 0 ; i++)
		Draw_Character ( (i+1)<<3, y, text[i]);

	// remove cursor
	key_lines[edit_line][key_linepos] = c;
}


/*
================
Con_DrawNotify

Draws the last few lines of output transparently over the game top
================
*/
void Con_DrawNotify (void)
{
	int		x, v;
	char	*text;
	int		i;
	float	time;
	extern char chat_buffer[];

	v = 0;
	for (i= con_current-NUM_CON_TIMES+1 ; i<=con_current ; i++)
	{
		if (i < 0)
			continue;
		time = con_times[i % NUM_CON_TIMES];
		if (time == 0)
			continue;
		time = realtime - time;
		if (time > con_notifytime.value)
			continue;
		text = con_text + (i % con_totallines)*con_linewidth;

		clearnotify = 0;

		for (x = 0 ; x < con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, v, text[x]);

		v += 8;
	}


	if (key_dest == key_message)
	{
		clearnotify = 0;

		x = 0;

		Draw_String (8, v, "say:");
		while(chat_buffer[x])
		{
			Draw_Character ( (x+5)<<3, v, chat_buffer[x]);
			x++;
		}
		Draw_Character ( (x+5)<<3, v, 10+((int)(realtime*con_cursorspeed)&1));
		v += 8;
	}

	if (v > con_notifylines)
		con_notifylines = v;
}

/*
================
Con_DrawConsole

Draws the console with the solid background
The typing input line at the bottom should only be drawn if typing is allowed
================
*/
void Con_DrawConsole (int lines, bool drawinput)
{
	int				i, x, y;
	int				rows;
	char			*text;
	int				j;

	if (lines <= 0)
		return;

	// draw the background
	Draw_ConsoleBackground (lines);

	// draw the text
	con_vislines = lines;

	rows = (lines-16)>>3;		// rows of text to draw
	y = lines - 16 - (rows<<3);	// may start slightly negative

	for (i= con_current - rows + 1 ; i<=con_current ; i++, y+=8 )
	{
		j = i - con_backscroll;
		if (j<0)
			j = 0;
		text = con_text + (j % con_totallines)*con_linewidth;

		for (x=0 ; x<con_linewidth ; x++)
			Draw_Character ( (x+1)<<3, y, text[x]);
	}

	// draw the input prompt, user text, and cursor if desired
	if (drawinput)
		Con_DrawInput ();
}
