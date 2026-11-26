#pragma once

// Minimal SDL declarations for the headless startup-policy tests only.
#include <cstdint>

using Uint8 = std::uint8_t;
using Uint16 = std::uint16_t;
using Uint32 = std::uint32_t;
using SDL_AudioDeviceID = Uint32;
struct SDL_version { Uint8 major, minor, patch; };
struct SDL_AudioSpec {
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint16 samples;
};
struct SDL_Window {};
using SDL_GLContext = void*;

constexpr Uint32 SDL_INIT_AUDIO = 0x00000010u;
constexpr Uint32 SDL_INIT_VIDEO = 0x00000020u;
constexpr Uint16 AUDIO_S16SYS = 0x8010u;
constexpr int SDL_AUDIO_ALLOW_ANY_CHANGE = 15;
constexpr int SDL_WINDOWPOS_UNDEFINED = 0;
constexpr Uint32 SDL_WINDOW_HIDDEN = 0x00000008u;
constexpr Uint32 SDL_WINDOW_OPENGL = 0x00000002u;

void SDL_GetVersion(SDL_version* version);
int SDL_GetNumVideoDrivers();
const char* SDL_GetVideoDriver(int index);
int SDL_GetNumAudioDrivers();
const char* SDL_GetAudioDriver(int index);
int SDL_Init(Uint32 flags);
int SDL_InitSubSystem(Uint32 flags);
void SDL_Quit();
const char* SDL_GetError();
const char* SDL_GetCurrentVideoDriver();
const char* SDL_GetCurrentAudioDriver();
int SDL_setenv(const char* name, const char* value, int overwrite);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int);
void SDL_CloseAudioDevice(SDL_AudioDeviceID);
SDL_Window* SDL_CreateWindow(const char*, int, int, int, int, Uint32);
SDL_GLContext SDL_GL_CreateContext(SDL_Window*);
void SDL_GL_DeleteContext(SDL_GLContext);
void SDL_DestroyWindow(SDL_Window*);
