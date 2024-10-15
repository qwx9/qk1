#include "quakedef.h"

static bool	cmd_wait;

//=============================================================================

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
============
*/
static void Cmd_Wait_f (cmd_t *c)
{
	USED(c);
	cmd_wait = true;
}

/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

static sizebuf_t	cmd_text;

/*
============
Cbuf_Init
============
*/
void Cbuf_Init (void)
{
	SZ_Alloc (&cmd_text, 65536);		// space for commands and script files
}


/*
============
Cbuf_AddText

Adds command text at the end of the buffer
============
*/
void Cbuf_AddText (char *text)
{
	int		l;

	l = strlen(text);

	if (cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Con_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	SZ_Write (&cmd_text, text, strlen(text));
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
FIXME: actually change the command buffer to do less copying
============
*/
void Cbuf_InsertText (char *text)
{
	char	*temp = nil;
	int		templen;

	// copy off any commands still remaining in the exec buffer
	templen = cmd_text.cursize;
	if (templen)
	{
		temp = Z_Malloc (templen);
		memmove(temp, cmd_text.data, templen);
		SZ_Clear (&cmd_text);
	}

	// add the entire text of the file
	Cbuf_AddText (text);

	// add the copied off data
	if (templen)
	{
		SZ_Write (&cmd_text, temp, templen);
		Z_Free (temp);
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute (void)
{
	int		i;
	char	*text;
	char	line[1024];
	int		quotes;

	while (cmd_text.cursize)
	{
		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for (i=0 ; i< cmd_text.cursize ; i++)
		{
			if (text[i] == '"')
				quotes++;
			if ( !(quotes&1) &&  text[i] == ';')
				break;	// don't break if inside a quoted string
			if (text[i] == '\n')
				break;
		}


		memmove(line, text, i);
		line[i] = 0;

		// delete the text from the command buffer and move remaining commands down
		// this is necessary because commands (exec, alias) can insert data at the
		// beginning of the text buffer

		if (i == cmd_text.cursize)
			cmd_text.cursize = 0;
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove(text, text+i, cmd_text.cursize);
		}

		// execute the command line
		Cmd_ExecuteString (line, src_command);

		if (cmd_wait)
		{	// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait = false;
			break;
		}
	}
}

/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/

/*
===============
Cmd_Exec_f
===============
*/
static void
Cmd_Exec_f(cmd_t *c)
{
	char	*f;
	void	*mark;

	USED(c);
	if (Cmd_Argc () != 2)
	{
		Con_Printf ("exec <filename> : execute a script file\n");
		return;
	}

	mark = Hunk_Mark ();
	f = loadhunklmp(Cmd_Argv(1), nil);
	if(f == nil){
		Con_Printf(va("exec: %s\n", lerr()));
		return;
	}
	Con_Printf ("execing %s\n",Cmd_Argv(1));

	Cbuf_InsertText (f);
	Hunk_FreeToMark (mark);
}


/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
static void
Cmd_Echo_f(cmd_t *c)
{
	int		i;

	USED(c);
	for (i=1 ; i<Cmd_Argc() ; i++)
		Con_Printf ("%s ",Cmd_Argv(i));
	Con_Printf ("\n");
}

static char *
CopyString(char *in)
{
	char	*out;

	out = Z_Malloc (strlen(in)+1);
	strcpy (out, in);
	return out;
}

static void
Cmd_AliasList(char *name, void *v, void *aux)
{
	cmd_t *a;

	USED(aux);
	a = v;
	if(iscmd(a) && a->alias != nil)
		Con_Printf("  %s : %s\n", name, a->alias);
}

static void
Cmd_ExecAlias_f(cmd_t *a)
{
	Cbuf_InsertText(a->alias);
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
static void
Cmd_Alias_f(cmd_t *t)
{
	char *s, cmd[1024];
	cmd_t *a;
	int i, c;

	USED(t);
	if(Cmd_Argc() == 1){
		Con_Printf ("Current alias commands:\n");
		Con_SearchObject("", 0, Cmd_AliasList, nil);
		return;
	}

	s = Cmd_Argv(1);
	if(strlen(Cmd_Args()) - strlen(s) >= sizeof(cmd)){
		Con_Printf("Alias commands are too long\n");
		return;
	}

	if((a = Con_FindObject(s)) != nil){
		if(!iscmd(a) || a->alias == nil){
			Con_Printf("Can't register alias %s, already defined\n", s);
			return;
		}
		Z_Free(a->alias);
	}

	if(a == nil)
		a = Cmd_AddCommand(CopyString(s), Cmd_ExecAlias_f);

	// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = Cmd_Argc();
	for(i = 2; i < c; i++){
		strcat(cmd, Cmd_Argv(i));
		if(i != c)
			strcat(cmd, " ");
	}
	strcat(cmd, "\n");

	a->alias = CopyString(cmd);
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

#define	MAX_ARGS		80

static	int			cmd_argc;
static	char		*cmd_argv[MAX_ARGS];
static	char		*cmd_null_string = "";
static	char		*cmd_args = nil;

cmd_source_t	cmd_source;

/*
============
Cmd_Init
============
*/
void Cmd_Init (void)
{
	Cmd_AddCommand ("exec", Cmd_Exec_f);
	Cmd_AddCommand ("echo", Cmd_Echo_f);
	Cmd_AddCommand ("alias", Cmd_Alias_f);
	Cmd_AddCommand ("cmd", Cmd_ForwardToServer);
	Cmd_AddCommand ("wait", Cmd_Wait_f);
}

/*
============
Cmd_Argc
============
*/
int		Cmd_Argc (void)
{
	return cmd_argc;
}

/*
============
Cmd_Argv
============
*/
char	*Cmd_Argv (int arg)
{
	if (arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

/*
============
Cmd_Args
============
*/
char		*Cmd_Args (void)
{
	return cmd_args;
}


/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
============
*/
static void
Cmd_TokenizeString(char *text)
{
	int		i;

	// clear the args from the last string
	for(i = 0; i < cmd_argc; i++){
		Z_Free(cmd_argv[i]);
		cmd_argv[i] = nil;
	}

	cmd_argc = 0;
	cmd_args = nil;

	while (1)
	{
		// skip whitespace up to a /n
		while (*text && *text <= ' ' && *text != '\n')
		{
			text++;
		}

		if (*text == '\n')	// a newline seperates commands in the buffer
			return;

		if (!*text)
			return;

		if (cmd_argc == 1)
			 cmd_args = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			cmd_argv[cmd_argc] = Z_Malloc(strlen(com_token)+1);
			strcpy(cmd_argv[cmd_argc], com_token);
			cmd_argc++;
		}
	}
}


/*
============
Cmd_AddCommand
============
*/
cmd_t *
Cmd_AddCommand(char *name, cmdfun_t f)
{
	cmd_t	*cmd;

	if(Con_FindObject(name) != nil){
		Con_Printf("Cmd_AddCommand: %s already defined\n", name);
		return nil;
	}

	cmd = Z_Malloc(sizeof *cmd);
	cmd->name = name;
	cmd->f = f;
	Con_AddObject(name, cmd);
	return cmd;
}

/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
FIXME: lookupnoadd the token to speed search?
============
*/
void
Cmd_ExecuteString (char *text, cmd_source_t src)
{
	cmd_t *cmd;

	cmd_source = src;
	Cmd_TokenizeString (text);

	// execute the command line
	if(!Cmd_Argc())
		return;		// no tokens

	cmd = Con_FindObject(cmd_argv[0]);
	if(cmd != nil && iscmd(cmd)){
		cmd->f(cmd);
		return;
	}

	// check cvars
	if (!Cvar_Command())
		Con_Printf ("Unknown command \"%s\": %s\n", Cmd_Argv(0), text);

}


/*
===================
Cmd_ForwardToServer

Sends the entire command line over to the server
===================
*/
void Cmd_ForwardToServer (cmd_t *c)
{
	USED(c);
	if (cls.state != ca_connected)
	{
		Con_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	MSG_WriteByte (&cls.message, clc_stringcmd);
	if(cistrcmp(Cmd_Argv(0), "cmd") != 0)
	{
		SZ_Print (&cls.message, Cmd_Argv(0));
		SZ_Print (&cls.message, " ");
	}
	if (Cmd_Argc() > 1)
		SZ_Print (&cls.message, Cmd_Args());
	else
		SZ_Print (&cls.message, "\n");
}
