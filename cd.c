#include <u.h>
#include <libc.h>
#include <thread.h>
#include <regexp.h>
#include "dat.h"
#include "quakedef.h"
#include "fns.h"

static int cdread;
static int cdloop;
static int cdvol;
static int chtrk;
static int ntrk;
static int trk;
static char trtype;
static int ctid = -1;
static Reprog *pat;


static int
cdinfo(void)
{
	int fd, i, n;
	Dir *d;

	ntrk = 0;

	if((fd = open("/mnt/cd", OREAD)) < 0)
		goto err;
	if((n = dirreadall(fd, &d)) < 0)
		goto err;
	close(fd);
	for(i = 0; i < n; i++)
		if(regexec(pat, d[i].name, nil, 0)){
			if(!trtype)
				trtype = d[i].name[0];
			ntrk++;
		}
	free(d);
	if(ntrk < 1)
		return -1;
	return 0;

err:
	close(fd);
	fprint(2, "cdinfo: %r\n");
	return -1;
}

static void
cproc(void *)
{
	int a, n, afd, fd;
	char s[24];
	uchar buf[8192];
	short *p;

	if((afd = open("/dev/audio", OWRITE)) < 0)
		return;
	fd = -1;
	for(;;){
		if(chtrk > 0){
			close(fd);
			trk = chtrk;
			snprint(s, sizeof s, "/mnt/cd/%c%03ud", trtype, trk);
			if((fd = open(s, OREAD)) < 0)
				fprint(2, "cproc: %r");
			chtrk = 0;
		}
		if(!cdread || fd < 0){
			sleep(1);
			continue;
		}
		if((n = read(fd, buf, sizeof buf)) < 0)
			break;
		if(n == 0){
			if(cdloop)
				seek(fd, 0, 0);
			else{
				close(fd);
				fd = -1;
			}
			continue;
		}
		p = (short *)buf;
		while((uintptr)p < (uintptr)(buf + sizeof buf)){
			a = *p * cdvol >> 8;
			if(a < (short)0x8000)
				a = 0x8000;
			*p++ = a;
		}
		if(write(afd, buf, n) != n)
			break;
	}
	close(afd);
	close(fd);
}

void
CDAudio_Play(byte nt, qboolean loop)
{
	if(ctid < 0)
		return;
	nt -= 1;	/* d001 was assumed part of the track list */
	if(nt < 1 || nt > ntrk){
		Con_Printf("cd: invalid track number %ud\n", nt);
		return;
	}

	chtrk = nt;
	cdloop = loop;
	if(cdvol > 0)
		cdread = 1;
}

void
CDAudio_Stop(void)
{
	cdread = 0;
	cdloop = 0;
}

void
CDAudio_Pause(void)
{
	cdread = 0;
}

void
CDAudio_Resume(void)
{
	cdread = 1;
}

void
CD_f(void)
{
	char *cmd;

	if(Cmd_Argc() < 2)
		return;
	cmd = Cmd_Argv(1);
	if(cistrcmp(cmd, "play") == 0){
		CDAudio_Play((uchar)atoi(Cmd_Argv(2)), false);
		return;
	}else if(cistrcmp(cmd, "loop") == 0){
		CDAudio_Play((uchar)atoi(Cmd_Argv(2)), true);
		return;
	}else if(cistrcmp(cmd, "stop") == 0){
		CDAudio_Stop();
		return;
	}else if(cistrcmp(cmd, "pause") == 0){
		CDAudio_Pause();
		return;
	}else if(cistrcmp(cmd, "resume") == 0){
		CDAudio_Resume();
		return;
	}else if(cistrcmp(cmd, "info") == 0){
		Con_Printf("track %ud/%ud; loop %d; vol %d\n", trk, ntrk, cdloop, cdvol);
		return;
	}
}

void
CDAudio_Update(void)
{
	int v;

	v = cdvol;
	cdvol = bgmvolume.value * 256;
	if(v <= 0 && cdvol > 0)
		cdread = 1;
	else if(v > 0 && cdvol <= 0)
		cdread = 0;
}

int
CDAudio_Init(void)
{
	if(cls.state == ca_dedicated)
		return -1;
	pat = regcomp("[au][0-9][0-9][0-9]");
	if(cdinfo() < 0)
		return -1;
	if((ctid = proccreate(cproc, nil, 16384)) < 0)
		sysfatal("proccreate: %r");

	Cmd_AddCommand("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");
	return 0;
}

void
CDAudio_Shutdown(void)
{
	if(ctid < 0)
		return;

	CDAudio_Stop();
	postnote(PNPROC, ctid, "shutdown");
	ctid = -1;
	free(pat);
	pat = nil;
	cdread = cdloop = 0;
}
