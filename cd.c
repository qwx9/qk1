#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "quakedef.h"

qboolean cdValid = false;
qboolean playing = false;
qboolean wasPlaying = false;
qboolean initialized = false;
qboolean enabled = true;
qboolean playLooping = false;
float cdvolume;
byte remap[100];
byte playTrack;
byte maxTrack;
int cdfd = -1;
char cd_dev[64] = "/dev/cdrom";


int CDAudio_GetAudioDiskInfo (void)
{
	cdValid = false;
	Con_DPrintf("CDAudio_GetAudioDiskInfo: PORTME\n");
	return -1;

	/*
	struct cdrom_tochdr tochdr;

	if(tochdr.cdth_trk0 < 1){
		Con_DPrintf("CDAudio: no music tracks\n");
		return -1;
	}
	cdValid = true;
	maxTrack = tochdr.cdth_trk1;
	return 0;
	*/
}

void CDAudio_Play (byte track, qboolean /*looping*/)
{
	if(cdfd == -1 || !enabled)
		return;
	if(!cdValid){
		CDAudio_GetAudioDiskInfo();
		if(!cdValid)
			return;
	}

	track = remap[track];
	if(track < 1 || track > maxTrack){
		Con_DPrintf("CDAudio: Bad track number %ud.\n", track);
		return;
	}

	Con_DPrintf("CDAudio_Play: PORTME\n");
	return;

	/*
	struct cdrom_tocentry entry;
	struct cdrom_ti ti;

	// don't try to play a non-audio track
	entry.cdte_track = track;
	entry.cdte_format = CDROM_MSF;
	if(ioctl(cdfd, CDROMREADTOCENTRY, &entry) == -1){
		Con_DPrintf("ioctl cdromreadtocentry failed\n");
		return;
	}
	if(entry.cdte_ctrl == CDROM_DATA_TRACK){
		Con_Printf("CDAudio: track %d is not audio\n", track);
		return;
	}

	if(playing){
		if(playTrack == track)
			return;
		CDAudio_Stop();
	}

	ti.cdti_trk0 = track;
	ti.cdti_trk1 = track;
	ti.cdti_ind0 = 1;
	ti.cdti_ind1 = 99;
	if(ioctl(cdfd, CDROMPLAYTRKIND, &ti) == -1) {
		Con_DPrintf("ioctl cdromplaytrkind failed\n");
		return;
	}
	if(ioctl(cdfd, CDROMRESUME) == -1) 
		Con_DPrintf("ioctl cdromresume failed\n");

	playLooping = looping;
	playTrack = track;
	playing = true;

	if(cdvolume == 0.0)
		CDAudio_Pause();
	*/
}

void CDAudio_Stop (void)
{
	if(cdfd == -1 || !enabled || !playing)
		return;
	wasPlaying = false;
	playing = false;
}

void CDAudio_Pause (void)
{
	if(cdfd == -1 || !enabled || !playing)
		return;
	wasPlaying = playing;
	playing = false;
}

void CDAudio_Resume (void)
{
	if(cdfd == -1 || !enabled || !cdValid || !wasPlaying)
		return;
	playing = true;
}

void CD_f (void)
{
	int ret, n;
	char *command;

	if(Cmd_Argc() < 2)
		return;
	command = Cmd_Argv(1);
	if(cistrcmp(command, "on") == 0){
		enabled = true;
		return;
	}
	if(cistrcmp(command, "off") == 0){
		if(playing)
			CDAudio_Stop();
		enabled = false;
		return;
	}
	if(cistrcmp(command, "reset") == 0){
		enabled = true;
		if(playing)
			CDAudio_Stop();
		for(n = 0; n < 100; n++)
			remap[n] = n;
		CDAudio_GetAudioDiskInfo();
		return;
	}
	if(cistrcmp(command, "remap") == 0){
		ret = Cmd_Argc() - 2;
		if(ret <= 0){
			for(n = 1; n < 100; n++)
				if(remap[n] != n)
					Con_Printf("  %ud -> %ud\n", n, remap[n]);
			return;
		}
		for(n = 1; n <= ret; n++)
			remap[n] = atoi(Cmd_Argv(n+1));
		return;
	}
	if(!cdValid){
		CDAudio_GetAudioDiskInfo();
		if(!cdValid){
			Con_Printf("No CD in player.\n");
			return;
		}
	}
	if(cistrcmp(command, "play") == 0){
		CDAudio_Play((uchar)atoi(Cmd_Argv(2)), false);
		return;
	}
	if(cistrcmp(command, "loop") == 0){
		CDAudio_Play((uchar)atoi(Cmd_Argv(2)), true);
		return;
	}
	if(cistrcmp(command, "stop") == 0){
		CDAudio_Stop();
		return;
	}
	if (cistrcmp(command, "pause") == 0){
		CDAudio_Pause();
		return;
	}
	if(cistrcmp(command, "resume") == 0){
		CDAudio_Resume();
		return;
	}
	if(cistrcmp(command, "info") == 0){
		Con_Printf("%ud tracks\n", maxTrack);
		if(playing)
			Con_Printf("Currently %s track %ud\n", playLooping ? "looping" : "playing", playTrack);
		else if(wasPlaying)
			Con_Printf("Paused %s track %ud\n", playLooping ? "looping" : "playing", playTrack);
		Con_Printf("Volume is %f\n", cdvolume);
		return;
	}
}

void CDAudio_Update (void)
{
	static long lastchk;

	if(!enabled)
		return;

	if(bgmvolume.value != cdvolume){
		if(cdvolume){
			Cvar_SetValue("bgmvolume", 0.0);
			cdvolume = bgmvolume.value;
			CDAudio_Pause();
		}else{
			Cvar_SetValue("bgmvolume", 1.0);
			cdvolume = bgmvolume.value;
			CDAudio_Resume();
		}
	}
	if(playing && lastchk < time(nil)){
		lastchk = time(nil) + 2;	//two seconds between chks

		Con_DPrintf("CDAudio_Update: PORTME\n");

		/*
		struct cdrom_subchnl subchnl;

		subchnl.cdsc_format = CDROM_MSF;
		if(ioctl(cdfd, CDROMSUBCHNL, &subchnl) == -1 ) {
			Con_DPrintf("ioctl cdromsubchnl failed\n");
			playing = false;
			return;
		}
		if(subchnl.cdsc_audiostatus != CDROM_AUDIO_PLAY
		&& subchnl.cdsc_audiostatus != CDROM_AUDIO_PAUSED){
			playing = false;
			if(playLooping)
				CDAudio_Play(playTrack, true);
		}
		*/
	}
}

int CDAudio_Init(void)
{
	int i;

	if(cls.state == ca_dedicated)
		return -1;
	if(COM_CheckParm("-nocdaudio"))
		return -1;

	if((i = COM_CheckParm("-cddev")) != 0 && i < com_argc - 1) {
		strncpy(cd_dev, com_argv[i + 1], sizeof(cd_dev));
		cd_dev[sizeof(cd_dev)-1] = 0;
	}

	if((cdfd = open(cd_dev, OREAD)) == -1){
		fprint(2, "open: %r\n");
		cdfd = -1;
		return -1;
	}

	for (i = 0; i < 100; i++)
		remap[i] = i;
	initialized = true;
	enabled = true;

	if(CDAudio_GetAudioDiskInfo()){
		Con_Printf("CDAudio_Init: No CD in player.\n");
		cdValid = false;
	}

	Cmd_AddCommand("cd", CD_f);

	Con_Printf("CD Audio Initialized\n");

	return 0;
}

void CDAudio_Shutdown (void)
{
	if(!initialized)
		return;
	CDAudio_Stop();
	close(cdfd);
	cdfd = -1;
}
