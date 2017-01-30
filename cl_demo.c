#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "quakedef.h"

static void
dmtimeend(void)
{
	int f;
	float t;

	cls.timedemo = 0;
	f = host_framecount - cls.td_startframe - 1;
	t = realtime - cls.td_starttime;
	if(t == 0.0)
		t = 1;
	Con_Printf("%d frames %5.1f seconds %5.1f fps\n", f, t, f/t);
}

static int
dmmsg(void)
{
	if(cls.signon == SIGNONS){
		if(cls.timedemo){
			if(host_framecount == cls.td_lastframe)
				return 0;
			cls.td_lastframe = host_framecount;
			if(host_framecount == cls.td_startframe + 1)
				cls.td_starttime = realtime;
		}else if(cl.time <= cl.mtime[0])
				return 0;
	}
	if(rlmpmsg() < 0){
		abortdemo();
		return 0;
	}
	return 1;
}

int
clmsg(void)
{
	int r;

	if(cls.demoplayback)
		return dmmsg();
	for(;;){
		r = NET_GetMessage(cls.netcon);
		if(r != 1 && r != 2)
			return r;
		if(net_message.cursize == 1 && net_message.data[0] == svc_nop)
			dprint("<-- server to client keepalive");
		else
			break;
	}
	if(cls.demorecording)
		wlmpmsg();
	return r;
}

void
abortdemo(void)
{
	if(!cls.demoplayback)
		return;
	endlmp();
	cls.demoplayback = 0;
	cls.state = ca_disconnected;
	if(cls.timedemo)
		dmtimeend();
}

void
stopdemo(void)
{
	if(cmd_source != src_command)
		return;
	if(!cls.demorecording){
		Con_Printf("stop: no recording in progress\n");
		return;
	}
	SZ_Clear(&net_message);
	MSG_WriteByte(&net_message, svc_disconnect);
	wlmpmsg();
	endlmp();
	cls.demorecording = 0;
}

void
recdemo(void)
{
	int c, trk;
	char f[Nfspath];

	if(cmd_source != src_command)
		return;
	c = Cmd_Argc();
	if(c < 2 || c > 4){
		Con_Printf("record <demoname> [<map> [cd track]]\n");
		return;
	}
	trk = -1;
	if(strstr(Cmd_Argv(1), "..") != nil){
		Con_Printf("recdemo: invalid path\n");
		return;
	}else if(c == 2 && cls.state == ca_connected){
		Con_Printf("recdemo: too late, already connected\n");
		return;
	}else if(c == 4)
		trk = atoi(Cmd_Argv(3));
	snprint(f, sizeof f, "%s/%s", fsdir, Cmd_Argv(1));
	ext(f, ".dem");
	if(c > 2)
		Cmd_ExecuteString(va("map %s", Cmd_Argv(2)), src_command);
	if(reclmp(f, trk) < 0){
		Con_Printf("recdemo: can't open %s: %r\n", f);
		return;
	}
	cls.demorecording = 1;
	cls.forcetrack = trk;	
}

/* when a demo is playing back, all NET_SendMessages are skipped, and
 * NET_GetMessages are read from the demo file. whenever cl.time gets past
 * the last received message, another message is read from the demo lump. */
void
playdemo(void)
{
	char f[Nfspath];

	if(cmd_source != src_command)
		return;
	if(Cmd_Argc() != 2){
		Con_Printf("playdemo <demo> : plays a demo\n");
		return;
	}
	CL_Disconnect();
	memset(f, 0, sizeof f);
	strncpy(f, Cmd_Argv(1), sizeof(f)-5);
	ext(f, ".dem");
	if(demolmp(f) < 0){
		Con_Printf("playdemo: can't open %s: %r\n", f);
		cls.demonum = -1;
		return;
	}
	cls.demoplayback = 1;
	cls.state = ca_connected;
}

void
timedemo(void)
{
	if(cmd_source != src_command)
		return;
	if(Cmd_Argc() != 2){
		Con_Printf("timedemo <demoname> : gets demo speeds\n");
		return;
	}
	playdemo();
	if(cls.demoplayback != 1)
		return;
	/* cls.td_starttime will be grabbed at the second frame of the demo, so
	 * all the loading time doesn't get counted */
	cls.timedemo = 1;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;	/* get a new message this frame */
}
