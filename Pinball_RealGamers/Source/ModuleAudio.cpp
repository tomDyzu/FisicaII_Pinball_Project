#include "Globals.h"
#include "Application.h"
#include "ModuleAudio.h"

#include "raylib.h"

ModuleAudio::ModuleAudio(Application* app, bool start_enabled) : Module(app, start_enabled)
{
	fx_count = 0;
	music = Music{ 0 };
	music_loaded = false;
}

// Destructor
ModuleAudio::~ModuleAudio()
{
}

// Called before render is available
bool ModuleAudio::Init()
{
	LOG("Loading raylib audio system");

	InitAudioDevice();

	if (!IsAudioDeviceReady())
	{
		LOG("WARNING: no audio device, the game will run without sound");
	}

	return true;
}

// raylib needs this every frame or the music does not play
update_status ModuleAudio::Update()
{
	if (music_loaded)
		UpdateMusicStream(music);

	return UPDATE_CONTINUE;
}

// Called before quitting
bool ModuleAudio::CleanUp()
{
	LOG("Freeing sound FX, closing Mixer and Audio subsystem");

	for (unsigned int i = 0; i < fx_count; i++)
	{
		for (int j = 0; j < MAX_FX_ALIASES; ++j)
			UnloadSoundAlias(fx_alias[i][j]);

		UnloadSound(fx[i]);
	}
	fx_count = 0;

	if (music_loaded)
	{
		StopMusicStream(music);
		UnloadMusicStream(music);
		music_loaded = false;
	}

	if (IsAudioDeviceReady())
		CloseAudioDevice();

	return true;
}

// Play a music file
bool ModuleAudio::PlayMusic(const char* path, float fade_time)
{
	if (IsEnabled() == false)
		return false;

	// Free the previous music before loading a new one
	if (music_loaded)
	{
		StopMusicStream(music);
		UnloadMusicStream(music);
		music_loaded = false;
	}

	music = LoadMusicStream(path);

	if (music.stream.buffer == NULL)
	{
		LOG("Cannot load music: %s", path);
		return false;
	}

	music_loaded = true;
	PlayMusicStream(music);

	LOG("Successfully playing %s", path);

	return true;
}

// Load WAV. Returns an id starting at 1, 0 means error
unsigned int ModuleAudio::LoadFx(const char* path)
{
	if (IsEnabled() == false || fx_count >= MAX_SOUNDS)
		return 0;

	Sound sound = LoadSound(path);

	if (sound.stream.buffer == NULL)
	{
		LOG("Cannot load sound: %s", path);
		return 0;
	}

	fx[fx_count] = sound;

	for (int j = 0; j < MAX_FX_ALIASES; ++j)
		fx_alias[fx_count][j] = LoadSoundAlias(sound);

	fx_next_alias[fx_count] = 0;
	fx_count++;

	return fx_count;
}

// Play WAV with the id returned by LoadFx
bool ModuleAudio::PlayFx(unsigned int id, int repeat)
{
	if (IsEnabled() == false || id == 0 || id > fx_count)
		return false;

	PlaySound(fx[id - 1]);

	return true;
}

// Stop WAV with the id returned by LoadFx
bool ModuleAudio::StopFx(unsigned int id)
{
	if (IsEnabled() == false || id == 0 || id > fx_count)
		return false;

	StopSound(fx[id - 1]);

	return true;
}

// Play WAV in a free copy, so the same sound can overlap with itself
bool ModuleAudio::PlayFxMix(unsigned int id)
{
	if (IsEnabled() == false || id == 0 || id > fx_count)
		return false;

	unsigned int i = id - 1;

	// Look for a copy that is not playing
	for (int j = 0; j < MAX_FX_ALIASES; ++j)
	{
		if (!IsSoundPlaying(fx_alias[i][j]))
		{
			PlaySound(fx_alias[i][j]);
			return true;
		}
	}

	// All the copies are busy: the next one starts again
	PlaySound(fx_alias[i][fx_next_alias[i]]);
	fx_next_alias[i] = (fx_next_alias[i] + 1) % MAX_FX_ALIASES;

	return true;
}