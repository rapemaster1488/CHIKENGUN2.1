#pragma once

// Linux desktop preflight using the same SDL instance as Urho3D. In particular,
// the system SDL package may have different backends than Urho3D's bundled SDL.
#if defined(__linux__) && !defined(__ANDROID__)

#if defined(__has_include) && __has_include(<SDL/SDL.h>)
#  include <SDL/SDL.h>
#elif defined(__has_include) && __has_include(<SDL2/SDL.h>)
#  include <SDL2/SDL.h>
#else
#  include <SDL.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace DesktopSDL {

struct Result {
    bool run = false;
    bool disableAudio = false;
    int exitCode = EXIT_FAILURE;
};

inline bool HasVideoDriver(const char* name) {
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        const char* driver = SDL_GetVideoDriver(i);
        if (driver && std::strcmp(driver, name) == 0) return true;
    }
    return false;
}

inline bool HasAudioDriver(const char* name) {
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        const char* driver = SDL_GetAudioDriver(i);
        if (driver && std::strcmp(driver, name) == 0) return true;
    }
    return false;
}

inline const char* Environment(const char* name) {
    const char* value = std::getenv(name);
    return !value ? "<unset>" : (*value ? value : "<empty>");
}

inline bool HasEnvironment(const char* name) {
    const char* value = std::getenv(name);
    return value && *value;
}

inline void PrintDrivers(FILE* output) {
    SDL_version version;
    SDL_GetVersion(&version);
    std::fprintf(output, "[SDL] Linked SDL %u.%u.%u\n", version.major, version.minor, version.patch);
    std::fprintf(output, "[SDL] Video drivers (built):");
    const int videos = SDL_GetNumVideoDrivers();
    if (!videos) std::fprintf(output, " <none>");
    for (int i = 0; i < videos; ++i) std::fprintf(output, " %s", SDL_GetVideoDriver(i));
    std::fprintf(output, "\n[SDL] Audio drivers (built):");
    const int audios = SDL_GetNumAudioDrivers();
    if (!audios) std::fprintf(output, " <none>");
    for (int i = 0; i < audios; ++i) std::fprintf(output, " %s", SDL_GetAudioDriver(i));
    std::fprintf(output, "\n");
}

inline void PrintEnvironment(FILE* output) {
    std::fprintf(output, "[SDL] DISPLAY=%s WAYLAND_DISPLAY=%s XDG_RUNTIME_DIR=%s\n",
                 Environment("DISPLAY"), Environment("WAYLAND_DISPLAY"), Environment("XDG_RUNTIME_DIR"));
    std::fprintf(output, "[SDL] SDL_VIDEODRIVER=%s SDL_AUDIODRIVER=%s\n",
                 Environment("SDL_VIDEODRIVER"), Environment("SDL_AUDIODRIVER"));
}

inline void ExplainVideoFailure(FILE* output) {
    if (!HasVideoDriver("x11") && !HasVideoDriver("wayland")) {
        std::fprintf(output, "[SDL] This SDL has neither an X11 nor a Wayland backend. "
                     "Install the display development libraries and rebuild the Urho3D SDK.\n");
    }
    if (!HasEnvironment("DISPLAY") && !HasEnvironment("WAYLAND_DISPLAY")) {
        std::fprintf(output, "[SDL] DISPLAY and WAYLAND_DISPLAY are both unset or empty. "
                     "Start from a graphical desktop session, configured SSH X11 forwarding, or WSLg.\n");
    } else {
        std::fprintf(output, "[SDL] Check that the display socket is reachable, its authorization is valid, "
                     "and the libraries for the selected X11/Wayland backend are installed.\n");
    }
    if (HasEnvironment("SDL_VIDEODRIVER")) {
        std::fprintf(output, "[SDL] SDL_VIDEODRIVER is explicitly set; unset it to let SDL choose, "
                     "or select a built backend that matches the session.\n");
    }
    std::fprintf(output, "[SDL] Run ./run.sh --diagnose-sdl for backend and OpenGL diagnostics.\n");
}

inline void ExplainAudioFailure(FILE* output) {
    if (!HasAudioDriver("alsa") && !HasAudioDriver("pulseaudio")) {
        std::fprintf(output, "[SDL] This SDL has no ALSA/PulseAudio backend. "
                     "Install their development libraries and rebuild the Urho3D SDK to restore sound.\n");
    } else {
        std::fprintf(output, "[SDL] Check the audio server/device and the runtime libraries for the selected backend.\n");
    }
    if (HasEnvironment("SDL_AUDIODRIVER")) {
        std::fprintf(output, "[SDL] SDL_AUDIODRIVER was explicitly set; check it or unset it to try automatic selection.\n");
    }
}

// Open a paused output device just long enough to detect an absent/unusable
// device. SDL can initialize ALSA even when opening the default device fails.
inline bool CanOpenAudio(std::string& error) {
    SDL_AudioSpec desired{};
    SDL_AudioSpec obtained{};
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.samples = 4096;
    desired.channels = 2;
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!device) {
        desired.channels = 1;
        device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    }
    if (!device) {
        error = SDL_GetError();
        return false;
    }
    SDL_CloseAudioDevice(device);
    return true;
}

inline bool CheckOpenGL(FILE* output) {
    SDL_Window* window = SDL_CreateWindow("CHIKENGUN SDL diagnostic", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 64, 64, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
    if (!window) {
        std::fprintf(output, "[SDL] OpenGL window test failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(output, "[SDL] OpenGL context test failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    std::fprintf(output, "[SDL] OpenGL: hidden test window and context created. "
                 "The game's own graphics settings can still require more.\n");
    return true;
}

inline Result Prepare(bool diagnose, bool noSound) {
    Result result;
    FILE* output = diagnose ? stdout : stderr;
    if (diagnose) {
        PrintDrivers(output);
        PrintEnvironment(output);
    }
    if (SDL_Init(0) != 0) {
        if (!diagnose) { PrintDrivers(output); PrintEnvironment(output); }
        std::fprintf(output, "[SDL] Initialization failed: %s\n", SDL_GetError());
        SDL_Quit();
        return result;
    }

    bool videoOK = SDL_InitSubSystem(SDL_INIT_VIDEO) == 0;
    if (!videoOK) {
        const std::string error = SDL_GetError();
        if (!diagnose) { PrintDrivers(output); PrintEnvironment(output); }
        std::fprintf(output, "[SDL] Video initialization failed: %s\n", error.c_str());
        ExplainVideoFailure(output);
    } else if (const char* driver = SDL_GetCurrentVideoDriver()) {
        // Headless backends cannot supply a visible game window, even when
        // newer SDL versions can create an offscreen OpenGL context.
        if (std::strcmp(driver, "dummy") == 0 || std::strcmp(driver, "offscreen") == 0) {
            videoOK = false;
            if (!diagnose) { PrintDrivers(output); PrintEnvironment(output); }
            std::fprintf(output, "[SDL] The %s video driver cannot display the game; "
                         "use a real X11/Wayland session instead.\n", driver);
        } else if (diagnose) {
            std::fprintf(output, "[SDL] Video initialized: %s\n", driver);
        }
    }

    // On a normal launch, fail fast and do not invent a dummy display. The
    // diagnostic continues so it can report audio even without a desktop.
    if (!videoOK && !diagnose) {
        SDL_Quit();
        return result;
    }
    const bool glOK = !diagnose || (videoOK && CheckOpenGL(output));

    // Urho3D constructs Audio before Setup(), even with -nosound. If sound is
    // explicitly disabled, initialize its silent driver when available.
    if (noSound && HasAudioDriver("dummy")) SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
    bool audioInitialized = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    if (!audioInitialized) {
        const std::string error = SDL_GetError();
        if (!diagnose) PrintDrivers(output);
        std::fprintf(output, "[SDL] Audio initialization failed: %s\n", error.c_str());
        if (!noSound) ExplainAudioFailure(output);
        result.disableAudio = true;

        // Silent audio is a nonfatal fallback, never a video fallback. Keep it
        // initialized so Urho's Audio constructor does not repeat the error.
        if (HasAudioDriver("dummy") && SDL_setenv("SDL_AUDIODRIVER", "dummy", 1) == 0)
            audioInitialized = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
        if (!audioInitialized)
            std::fprintf(output, "[SDL] No silent audio backend is available; Urho3D may log an audio error.\n");
    } else if (noSound) {
        result.disableAudio = true;
        if (diagnose) std::fprintf(output, "[SDL] Audio disabled by -nosound.\n");
    } else if (const char* driver = SDL_GetCurrentAudioDriver()) {
        if (std::strcmp(driver, "dummy") == 0) {
            result.disableAudio = true;
            std::fprintf(output, "[SDL] The selected audio driver is dummy; sound is disabled.\n");
        } else if (std::strcmp(driver, "disk") == 0) {
            // SDL's disk driver opens its output with "wb". A probe would
            // truncate an existing recording, even for --diagnose-sdl.
            if (diagnose) std::fprintf(output, "[SDL] Audio initialized: disk; output probe skipped to avoid overwriting its file.\n");
        } else {
            std::string error;
            if (!CanOpenAudio(error)) {
                result.disableAudio = true;
                std::fprintf(output, "[SDL] Audio output could not be opened: %s\n", error.c_str());
                ExplainAudioFailure(output);
            } else if (diagnose) {
                std::fprintf(output, "[SDL] Audio initialized: %s; a paused output device opened successfully.\n", driver);
            }
        }
    }
    if (result.disableAudio && !noSound)
        std::fprintf(output, diagnose ? "[SDL] Sound is unavailable or disabled.\n"
                                     : "[SDL] Continuing without sound.\n");

    if (diagnose) {
        SDL_Quit();
        result.exitCode = videoOK && glOK ? EXIT_SUCCESS : EXIT_FAILURE;
        return result;
    }
    // Keep the initialized subsystems alive for Urho3D. Its Context takes over
    // SDL lifetime and calls SDL_Quit when its subsystems are destroyed.
    result.run = true;
    result.exitCode = EXIT_SUCCESS;
    return result;
}

} // namespace DesktopSDL
#endif
