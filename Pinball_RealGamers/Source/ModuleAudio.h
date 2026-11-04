#pragma once

#include "Module.h"

#define MAX_SOUNDS	16
#define MAX_FX_ALIASES  4     // copies of every sound: they let the same sound play several times at once
#define DEFAULT_MUSIC_FADE_TIME 2.0f

class ModuleAudio : public Module
{
public:

	ModuleAudio(Application* app, bool start_enabled = true);
	~ModuleAudio();

	bool Init();
	update_status Update();   // keeps the music stream fed
	bool CleanUp();

	// Play a music file (fade_time is not implemented yet)
	bool PlayMusic(const char* path, float fade_time = DEFAULT_MUSIC_FADE_TIME);

	// Load a sound in memory. Returns an id (1..MAX_SOUNDS), 0 means error
	unsigned int LoadFx(const char* path);

	// Play a previously loaded sound using the id returned by LoadFx
	bool PlayFx(unsigned int fx, int repeat = 0);

	// Stop a sound that is playing, using the id returned by LoadFx
	bool StopFx(unsigned int fx);

	// Play a sound without cutting the same sound if it is already playing (uses a free copy)
	bool PlayFxMix(unsigned int fx);

private:

	Music music;
	bool music_loaded;
	Sound fx[MAX_SOUNDS];
	Sound fx_alias[MAX_SOUNDS][MAX_FX_ALIASES];
	unsigned int fx_next_alias[MAX_SOUNDS];
	unsigned int fx_count;
};