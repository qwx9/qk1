#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "quakedef.h"

static qboolean cdValid;
static qboolean	playing;
static qboolean	wasPlaying;
static qboolean	initialized;
static qboolean	enabled = true;
static qboolean playLooping;
static float cdvolume;
static byte remap[100];
static byte playTrack;
static byte maxTrack;
static int cdfile = -1;
static char cd_dev[64] = "/dev/cdrom";


static void CDAudio_Eject(void)
{
	if (cdfile == -1 || !enabled)
		return; // no cd init'd
/*
	if ( ioctl(cdfile, CDROMEJECT) == -1 ) 
		Con_DPrintf("ioctl cdromeject failed\n");
*/
}

static void CDAudio_CloseDoor(void)
{
	if (cdfile == -1 || !enabled)
		return; // no cd init'd
/*
	if ( ioctl(cdfile, CDROMCLOSETRAY) == -1 ) 
		Con_DPrintf("ioctl cdromclosetray failed\n");
*/
}

static int CDAudio_GetAudioDiskInfo(void)
{
	cdValid = false;
	return -1;

/*
	struct cdrom_tochdr tochdr;

	if ( ioctl(cdfile, CDROMREADTOCHDR, &tochdr) == -1 ) 
    {
      Con_DPrintf("ioctl cdromreadtochdr failed\n");
	  return -1;
    }

	if (tochdr.cdth_trk0 < 1)
	{
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}

	cdValid = true;
	maxTrack = tochdr.cdth_trk1;

	return 0;
*/
}


void CDAudio_Play(byte track, qboolean looping)
{
	if (cdfile == -1 || !enabled)
		return;
	
	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
			return;
	}

	track = remap[track];

	if (track < 1 || track > maxTrack)
	{
		Con_DPrintf("CDAudio: Bad track number %u.\n", track);
		return;
	}

	USED(looping);
/*
	struct cdrom_tocentry entry;
	struct cdrom_ti ti;

	// don't try to play a non-audio track
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;
    if ( ioctl(cdfile, CDROMREADTOCENTRY, &entry) == -1 )
	{
		Con_DPrintf("ioctl cdromreadtocentry failed\n");
		return;
	}
	if (entry.cdte_ctrl == CDROM_DATA_TRACK)
	{
		Con_Printf("CDAudio: track %i is not audio\n", track);
		return;
	}

	if (playing)
	{
		if (playTrack == track)
			return;
		CDAudio_Stop();
	}

	ti.cdti_trk0 = track;
	ti.cdti_trk1 = track;
	ti.cdti_ind0 = 1;
	ti.cdti_ind1 = 99;

	if ( ioctl(cdfile, CDROMPLAYTRKIND, &ti) == -1 ) 
    {
		Con_DPrintf("ioctl cdromplaytrkind failed\n");
		return;
    }

	if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
		Con_DPrintf("ioctl cdromresume failed\n");

	playLooping = looping;
	playTrack = track;
	playing = true;

	if (cdvolume == 0.0)
		CDAudio_Pause ();
*/
}


void CDAudio_Stop(void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!playing)
		return;

/*
	if ( ioctl(cdfile, CDROMSTOP) == -1 )
		Con_DPrintf("ioctl cdromstop failed (%d)\n", errno);
	wasPlaying = false;
	playing = false;
*/
}

void CDAudio_Pause(void)
{
	if (cdfile == -1 || !enabled)
		return;

	if (!playing)
		return;

/*
	if ( ioctl(cdfile, CDROMPAUSE) == -1 ) 
		Con_DPrintf("ioctl cdrompause failed\n");
	wasPlaying = playing;
	playing = false;
*/
}


void CDAudio_Resume(void)
{
	if (cdfile == -1 || !enabled)
		return;
	
	if (!cdValid)
		return;

	if (!wasPlaying)
		return;
/*
	if ( ioctl(cdfile, CDROMRESUME) == -1 ) 
		Con_DPrintf("ioctl cdromresume failed\n");
	playing = true;
*/
}

static void CD_f (void)
{
	char	*command;
	int		ret;
	int		n;

	if (Cmd_Argc() < 2)
		return;

	command = Cmd_Argv (1);

	if (cistrcmp(command, "on") == 0)
	{
		enabled = true;
		return;
	}

	if (cistrcmp(command, "off") == 0)
	{
		if (playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}

	if (cistrcmp(command, "reset") == 0)
	{
		enabled = true;
		if (playing)
			CDAudio_Stop();
		for (n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}

	if (cistrcmp(command, "remap") == 0)
	{
		ret = Cmd_Argc() - 2;
		if (ret <= 0)
		{
			for (n = 1; n < 100; n++)
				if (remap[n] != n)
					Con_Printf("  %u -> %u\n", n, remap[n]);
			return;
		}
		for (n = 1; n <= ret; n++)
			remap[n] = Q_atoi(Cmd_Argv (n+1));
		return;
	}

	if (cistrcmp(command, "close") == 0)
	{
		CDAudio_CloseDoor();
		return;
	}

	if (!cdValid)
	{
		CDAudio_GetAudioDiskInfo();
		if (!cdValid)
		{
			Con_Printf("No CD in player.\n");
			return;
		}
	}

	if (cistrcmp(command, "play") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), false);
		return;
	}

	if (cistrcmp(command, "loop") == 0)
	{
		CDAudio_Play((byte)Q_atoi(Cmd_Argv (2)), true);
		return;
	}

	if (cistrcmp(command, "stop") == 0)
	{
		CDAudio_Stop();
		return;
	}

	if (cistrcmp(command, "pause") == 0)
	{
		CDAudio_Pause();
		return;
	}

	if (cistrcmp(command, "resume") == 0)
	{
		CDAudio_Resume();
		return;
	}

	if (cistrcmp(command, "eject") == 0)
	{
		if (playing)
			CDAudio_Stop();
		CDAudio_Eject();
		cdValid = false;
		return;
	}

	if (cistrcmp(command, "info") == 0)
	{
		Con_Printf("%u tracks\n", maxTrack);
		if (playing)
			Con_Printf("Currently %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		else if (wasPlaying)
			Con_Printf("Paused %s track %u\n", playLooping ? "looping" : "playing", playTrack);
		Con_Printf("Volume is %f\n", cdvolume);
		return;
	}
}

void CDAudio_Update(void)
{
	if (!enabled)
		return;

	if (bgmvolume.value != cdvolume)
	{
		if (cdvolume)
		{
			Cvar_SetValue ("bgmvolume", 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause ();
		}
		else
		{
			Cvar_SetValue ("bgmvolume", 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume ();
		}
	}

/*
	struct cdrom_subchnl subchnl;
	static time_t lastchk;
	if (playing && lastchk < time(NULL)) {
		lastchk = time(NULL) + 2; //two seconds between chks

		subchnl.cdsc_format = CDROM_MSF;
		if (ioctl(cdfile, CDROMSUBCHNL, &subchnl) == -1 ) {
			Con_DPrintf("ioctl cdromsubchnl failed\n");
			playing = false;
			return;
		}
		if (subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY &&
			subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED) {
			playing = false;
			if (playLooping)
				CDAudio_Play(playTrack, true);
		}
	}
*/
}

int CDAudio_Init(void)
{
	int i;

	if (COM_CheckParm("-nocdaudio"))
		return -1;

	if ((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1) {
		strncpy(cd_dev, com_argv[i + 1], sizeof(cd_dev));
		cd_dev[sizeof(cd_dev) - 1] = 0;
	}

	if ((cdfile = open(cd_dev, OREAD)) == -1) {
		fprint(2, "CDAudio_Init:open: %r\n");
		cdfile = -1;
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;
	initialized = true;
	enabled = true;

	if (CDAudio_GetAudioDiskInfo())
	{
		Con_Printf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand ("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}


void CDAudio_Shutdown(void)
{
	if (!initialized)
		return;
	CDAudio_Stop();
	close(cdfile);
	cdfile = -1;
}