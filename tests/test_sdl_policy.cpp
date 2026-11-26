// Headless tests for the Linux SDL startup policy. The actual SDL integration
// is separately exercised by test_sdl_startup.sh and graphical smoke tests.
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "../src/SDLStartup.h"

namespace Fake {
bool videoAvailable = true, audioAvailable = true, outputAvailable = true;
bool dummyAudioAvailable = true, glAvailable = true, stereoAvailable = true;
int audioInitCount = 0, outputOpenCount = 0, outputCloseCount = 0, quits = 0;
std::string error, video, audio;
SDL_Window window;

void Reset() {
    videoAvailable = audioAvailable = outputAvailable = dummyAudioAvailable = glAvailable = stereoAvailable = true;
    audioInitCount = outputOpenCount = outputCloseCount = quits = 0;
    error.clear(); video.clear(); audio.clear();
    unsetenv("DISPLAY"); unsetenv("WAYLAND_DISPLAY"); unsetenv("XDG_RUNTIME_DIR");
    unsetenv("SDL_VIDEODRIVER"); unsetenv("SDL_AUDIODRIVER");
}
}

void SDL_GetVersion(SDL_version* version) { *version = {2, 0, 10}; }
int SDL_GetNumVideoDrivers() { return 4; }
const char* SDL_GetVideoDriver(int index) {
    static const char* drivers[] = {"x11", "wayland", "dummy", "offscreen"};
    return drivers[index];
}
int SDL_GetNumAudioDrivers() { return Fake::dummyAudioAvailable ? 4 : 3; }
const char* SDL_GetAudioDriver(int index) {
    static const char* drivers[] = {"pulseaudio", "alsa", "disk", "dummy"};
    return drivers[index];
}
int SDL_Init(Uint32) { return 0; }
int SDL_InitSubSystem(Uint32 flags) {
    if (flags == SDL_INIT_VIDEO) {
        if (!Fake::videoAvailable) { Fake::error = "no video"; return -1; }
        const char* requested = std::getenv("SDL_VIDEODRIVER");
        Fake::video = requested && *requested ? requested : "x11";
        return 0;
    }
    assert(flags == SDL_INIT_AUDIO);
    ++Fake::audioInitCount;
    const char* requested = std::getenv("SDL_AUDIODRIVER");
    if (requested && std::strcmp(requested, "dummy") == 0 && Fake::dummyAudioAvailable) {
        Fake::audio = "dummy";
        return 0;
    }
    if (requested && std::strcmp(requested, "disk") == 0) {
        Fake::audio = "disk";
        return 0;
    }
    if (!Fake::audioAvailable) { Fake::error = "no audio"; return -1; }
    Fake::audio = "alsa";
    return 0;
}
void SDL_Quit() { ++Fake::quits; }
const char* SDL_GetError() { return Fake::error.c_str(); }
const char* SDL_GetCurrentVideoDriver() { return Fake::video.c_str(); }
const char* SDL_GetCurrentAudioDriver() { return Fake::audio.c_str(); }
int SDL_setenv(const char* name, const char* value, int overwrite) { return setenv(name, value, overwrite); }
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* desired, SDL_AudioSpec*, int) {
    ++Fake::outputOpenCount;
    assert(desired->channels == (Fake::outputOpenCount == 1 ? 2 : 1));
    if (!Fake::outputAvailable || (desired->channels == 2 && !Fake::stereoAvailable)) {
        Fake::error = "no output device";
        return 0;
    }
    return 1;
}
void SDL_CloseAudioDevice(SDL_AudioDeviceID) { ++Fake::outputCloseCount; }
SDL_Window* SDL_CreateWindow(const char*, int, int, int, int, Uint32) { return &Fake::window; }
SDL_GLContext SDL_GL_CreateContext(SDL_Window*) {
    if (!Fake::glAvailable) { Fake::error = "no OpenGL"; return nullptr; }
    return &Fake::window;
}
void SDL_GL_DeleteContext(SDL_GLContext) {}
void SDL_DestroyWindow(SDL_Window*) {}

int main() {
    using DesktopSDL::Prepare;
    Fake::Reset();
    Fake::videoAvailable = false;
    setenv("SDL_VIDEODRIVER", "x11", 1);
    auto noVideo = Prepare(false, false);
    assert(!noVideo.run && noVideo.exitCode != 0 && Fake::quits == 1);
    assert(Fake::audioInitCount == 0 && std::strcmp(std::getenv("SDL_VIDEODRIVER"), "x11") == 0);

    Fake::Reset();
    auto normal = Prepare(false, false);
    assert(normal.run && normal.exitCode == 0 && !normal.disableAudio);
    assert(Fake::audioInitCount == 1 && Fake::outputOpenCount == 1 && Fake::outputCloseCount == 1);
    assert(Fake::quits == 0 && std::getenv("SDL_VIDEODRIVER") == nullptr);

    Fake::Reset();
    Fake::stereoAvailable = false;
    auto monoFallback = Prepare(false, false);
    assert(monoFallback.run && !monoFallback.disableAudio);
    assert(Fake::outputOpenCount == 2 && Fake::outputCloseCount == 1);

    Fake::Reset();
    Fake::audioAvailable = false;
    setenv("SDL_AUDIODRIVER", "invalid", 1);
    auto failedInit = Prepare(false, false);
    assert(failedInit.run && failedInit.disableAudio && Fake::audioInitCount == 2);
    assert(std::strcmp(std::getenv("SDL_AUDIODRIVER"), "dummy") == 0);
    assert(std::getenv("SDL_VIDEODRIVER") == nullptr && Fake::outputOpenCount == 0);

    Fake::Reset();
    Fake::outputAvailable = false;
    auto noDevice = Prepare(false, false);
    assert(noDevice.run && noDevice.disableAudio && Fake::audioInitCount == 1);
    assert(Fake::outputOpenCount == 2 && Fake::outputCloseCount == 0);
    assert(std::getenv("SDL_AUDIODRIVER") == nullptr);

    Fake::Reset();
    Fake::audioAvailable = false;
    setenv("SDL_AUDIODRIVER", "invalid", 1);
    auto noSound = Prepare(false, true);
    assert(noSound.run && noSound.disableAudio && Fake::audioInitCount == 1);
    assert(Fake::outputOpenCount == 0 && std::strcmp(std::getenv("SDL_AUDIODRIVER"), "dummy") == 0);

    Fake::Reset();
    Fake::audioAvailable = false;
    Fake::dummyAudioAvailable = false;
    auto noSilentDriver = Prepare(false, false);
    assert(noSilentDriver.run && noSilentDriver.disableAudio && Fake::audioInitCount == 1);
    assert(std::getenv("SDL_VIDEODRIVER") == nullptr);

    for (const char* headless : {"dummy", "offscreen"}) {
        Fake::Reset();
        setenv("SDL_VIDEODRIVER", headless, 1);
        auto noWindow = Prepare(false, false);
        assert(!noWindow.run && noWindow.exitCode != 0 && Fake::audioInitCount == 0);
        assert(std::strcmp(std::getenv("SDL_VIDEODRIVER"), headless) == 0);
    }

    Fake::Reset();
    setenv("SDL_AUDIODRIVER", "disk", 1);
    auto diskDiagnostic = Prepare(true, false);
    assert(!diskDiagnostic.run && diskDiagnostic.exitCode == 0 && !diskDiagnostic.disableAudio);
    assert(Fake::outputOpenCount == 0 && Fake::outputCloseCount == 0);
    Fake::Reset();
    setenv("SDL_AUDIODRIVER", "disk", 1);
    auto diskLaunch = Prepare(false, false);
    assert(diskLaunch.run && !diskLaunch.disableAudio && Fake::outputOpenCount == 0);

    Fake::Reset();
    assert(std::strcmp(DesktopSDL::Environment("DISPLAY"), "<unset>") == 0);
    setenv("DISPLAY", "", 1);
    assert(std::strcmp(DesktopSDL::Environment("DISPLAY"), "<empty>") == 0);
    assert(!DesktopSDL::HasEnvironment("DISPLAY"));

    Fake::Reset();
    Fake::glAvailable = false;
    auto noGL = Prepare(true, false);
    assert(!noGL.run && noGL.exitCode != 0 && Fake::quits == 1);
    assert(Fake::outputOpenCount == 1);

    Fake::Reset();
    auto diagnostic = Prepare(true, false);
    assert(!diagnostic.run && diagnostic.exitCode == 0 && Fake::quits == 1);
    assert(Fake::outputOpenCount == 1 && Fake::outputCloseCount == 1);
    Fake::Reset();
    std::cout << "SDL startup policy: display failures, audio fallback, -nosound and OpenGL passed.\n";
}
