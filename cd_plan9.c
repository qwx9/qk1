#include "quakedef.h"
#include <thread.h>
#include <regexp.h>

static char cdfile[13];
static int chtrk, cdread;

static void
cproc(void *)
{
	int n, afd, fd, v;
	char f[32];
	s16int buf[4096];
	short *p;
	double vol;

	if((afd = open("/dev/audio", OWRITE)) < 0){
		Con_DPrintf("cd: open: %r\n");
		return;
	}
	fd = -1;
	for(;;){
		if(chtrk > 0){
			close(fd);
			cdtrk = chtrk;
			snprint(f, sizeof f, "%s%03ud", cdfile, cdtrk);
			if((fd = open(f, OREAD)) < 0)
				Con_DPrintf("cd: open: %r\n");
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
		vol = bgmvolume.value;
		for(p=buf; p<buf+n/sizeof*buf; p++){
			v = *p * vol;
			if(v > 0x7fff)
				v = 0x7fff;
			else if(v < -0x8000)
				v = -0x8000;
			*p = v;
		}
		if(write(afd, buf, n) != n)
			break;
	}
}

void
stopcd(void)
{
	cdread = 0;
	cdloop = 0;
}

void
pausecd(void)
{
	cdread = 0;
}

void
resumecd(void)
{
	cdread = 1;
}

void
shutcd(void)
{
	stopcd();
}

void
stepcd(void)
{
	if(!cdenabled || bgmvolume.value <= 0.0 || cdread == 0)
		return;
	cdread = bgmvolume.value > 0.0;
}

void
playcd(int nt, bool loop)
{
	if(cdntrk < 1)
		return;
	nt -= 1;	/* d001 assumed part of track list */
	if(nt < 1 || nt > cdntrk){
		Con_Printf("cd: invalid track number %d\n", nt);
		return;
	}
	chtrk = nt;
	cdloop = loop;
	cdread = bgmvolume.value > 0.0;
}

static int
cdinfo(void)
{
	int fd, i, n;
	char type;
	Reprog *pat;
	Dir *d;

	cdntrk = 0;
	if((fd = open("/mnt/cd", OREAD)) < 0)
		return -1;
	n = dirreadall(fd, &d);
	close(fd);
	if(n < 0)
		return -1;
	pat = regcomp("[au][0-9][0-9][0-9]");
	for(type=0, i=0; i<n; i++)
		if(regexec(pat, d[i].name, nil, 0)){
			if(type == 0)
				type = d[i].name[0];
			cdntrk++;
		}
	free(pat);
	free(d);
	if(cdntrk < 1)
		return -1;
	snprint(cdfile, sizeof cdfile, "/mnt/cd/%c", type);
	return 0;
}

int
initcd(void)
{
	Cvar_RegisterVariable(&bgmvolume);
	Cmd_AddCommand("cd", cdcmd);

	if(!(cdenabled = !isdisabled("cd")))
		return 0;

	if(cdinfo() < 0){
		Con_DPrintf("cdinfo: %r\n");
		return -1;
	}
	if(proccreate(cproc, nil, 16384) < 0)
		sysfatal("proccreate: %r");
	return 0;
}
