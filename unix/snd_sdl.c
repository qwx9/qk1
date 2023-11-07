#include "quakedef.h"
#include <SDL.h>

static SDL_AudioDeviceID adev;

void
sndwrite(uchar *buf, long sz)
{
	if(adev != 0 && SDL_QueueAudio(adev, buf, sz) != 0){
		Con_Printf("sndwrite: %s\n", SDL_GetError());
		sndclose();
	}
}

void
sndclose(void)
{
	if(adev != 0){
		SDL_CloseAudioDevice(adev);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		adev = 0;
	}
}

int
sndopen(void)
{
	SDL_AudioSpec got, want = {
		.freq = 44100,
		.format = AUDIO_S16LSB,
		.channels = 2,
		.samples = 441,
		0,
	};

	if(SDL_InitSubSystem(SDL_INIT_AUDIO) != 0){
err:
		Con_Printf("sndopen: %s\n", SDL_GetError());
		return -1;
	}
	if((adev = SDL_OpenAudioDevice(nil, 0, &want, &got, SDL_AUDIO_ALLOW_SAMPLES_CHANGE)) == 0){
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		goto err;
	}
	SDL_PauseAudioDevice(adev, 0);
	return 0;
}
