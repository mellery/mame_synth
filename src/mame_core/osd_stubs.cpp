// OSD stub implementations for minimal MAME integration
#include "emu.h"
#include "osdepend.h"
#include "modules/osdmodule.h"
#include "modules/osdwindow.h"
#include "emucore.h"
#include "main.h"

// Video config global - required by osdwindow.cpp
osd_video_config video_config;

// NOTE: emulator_info functions are defined in emulator_stub.cpp - don't duplicate them here

// Network handler stub
namespace osd {
    network_handler::network_handler() noexcept {
        // Minimal network handler - we don't use network features
    }
}

// Hash file extra info stub - used for software list management (not needed for our purposes)
void hashfile_extrainfo(const char *hash_path, const game_driver& driver, const util::hash_collection& hashes, std::string& result) {
    // Not using software lists, so no extra info needed
    result.clear();
}

// NOTE: osd_uchar_from_osdchar is provided by mame/src/osd/strconv.cpp for main executable
// For tests, we may need a stub version if strconv.cpp isn't included

// Module registration stubs - these are referenced by osd_common_t but not actually used
// We provide nullptr implementations so they register but don't instantiate

// Sound modules
extern const module_type SOUND_PORTAUDIO;
extern const module_type SOUND_PULSEAUDIO;
extern const module_type SOUND_PIPEWIRE;

// Monitor modules
extern const module_type MONITOR_SDL;
extern const module_type MONITOR_WIN32;
extern const module_type MONITOR_DXGI;
extern const module_type MONITOR_MAC;

// Debug modules
extern const module_type DEBUG_WINDOWS;
extern const module_type DEBUG_QT;
extern const module_type DEBUG_IMGUI;
extern const module_type DEBUG_GDBSTUB;

// Network modules
extern const module_type NETDEV_TAPTUN;
extern const module_type NETDEV_PCAP;

// MIDI modules
extern const module_type MIDI_PM;

// Input modules
extern const module_type KEYBOARDINPUT_SDL;
extern const module_type KEYBOARDINPUT_RAWINPUT;
extern const module_type KEYBOARDINPUT_DINPUT;
extern const module_type KEYBOARDINPUT_WIN32;

extern const module_type MOUSEINPUT_SDL;
extern const module_type MOUSEINPUT_RAWINPUT;
extern const module_type MOUSEINPUT_DINPUT;
extern const module_type MOUSEINPUT_WIN32;

extern const module_type LIGHTGUNINPUT_SDL;
extern const module_type LIGHTGUN_X11;
extern const module_type LIGHTGUNINPUT_RAWINPUT;
extern const module_type LIGHTGUNINPUT_WIN32;

extern const module_type JOYSTICKINPUT_SDLGAME;
extern const module_type JOYSTICKINPUT_SDLJOY;
extern const module_type JOYSTICKINPUT_WINHYBRID;
extern const module_type JOYSTICKINPUT_DINPUT;
extern const module_type JOYSTICKINPUT_XINPUT;

// Output modules
extern const module_type OUTPUT_WIN32;

// Font modules
extern const module_type FONT_OSX;
extern const module_type FONT_WINDOWS;
extern const module_type FONT_DWRITE;
extern const module_type FONT_SDL;

// Renderer modules
extern const module_type RENDERER_GDI;
extern const module_type RENDERER_OPENGL;
extern const module_type RENDERER_BGFX;
extern const module_type RENDERER_SDL2;
extern const module_type RENDERER_SDL1;

// Sound modules (platform-specific)
extern const module_type SOUND_WASAPI;
extern const module_type SOUND_XAUDIO2;
extern const module_type SOUND_COREAUDIO;
extern const module_type SOUND_JS;
extern const module_type SOUND_SDL;

// Now provide dummy implementations for all of these
const module_type FONT_OSX = nullptr;
const module_type FONT_WINDOWS = nullptr;
const module_type FONT_DWRITE = nullptr;
const module_type FONT_SDL = nullptr;

const module_type RENDERER_GDI = nullptr;
const module_type RENDERER_OPENGL = nullptr;
const module_type RENDERER_BGFX = nullptr;
const module_type RENDERER_SDL2 = nullptr;
const module_type RENDERER_SDL1 = nullptr;

const module_type SOUND_WASAPI = nullptr;
const module_type SOUND_XAUDIO2 = nullptr;
const module_type SOUND_COREAUDIO = nullptr;
const module_type SOUND_JS = nullptr;
const module_type SOUND_SDL = nullptr;

const module_type SOUND_PORTAUDIO = nullptr;
const module_type SOUND_PULSEAUDIO = nullptr;
const module_type SOUND_PIPEWIRE = nullptr;

const module_type MONITOR_SDL = nullptr;
const module_type MONITOR_WIN32 = nullptr;
const module_type MONITOR_DXGI = nullptr;
const module_type MONITOR_MAC = nullptr;

const module_type DEBUG_WINDOWS = nullptr;
const module_type DEBUG_QT = nullptr;
const module_type DEBUG_IMGUI = nullptr;
const module_type DEBUG_GDBSTUB = nullptr;

const module_type NETDEV_TAPTUN = nullptr;
const module_type NETDEV_PCAP = nullptr;

const module_type MIDI_PM = nullptr;

const module_type KEYBOARDINPUT_SDL = nullptr;
const module_type KEYBOARDINPUT_RAWINPUT = nullptr;
const module_type KEYBOARDINPUT_DINPUT = nullptr;
const module_type KEYBOARDINPUT_WIN32 = nullptr;

const module_type MOUSEINPUT_SDL = nullptr;
const module_type MOUSEINPUT_RAWINPUT = nullptr;
const module_type MOUSEINPUT_DINPUT = nullptr;
const module_type MOUSEINPUT_WIN32 = nullptr;

const module_type LIGHTGUNINPUT_SDL = nullptr;
const module_type LIGHTGUN_X11 = nullptr;
const module_type LIGHTGUNINPUT_RAWINPUT = nullptr;
const module_type LIGHTGUNINPUT_WIN32 = nullptr;

const module_type JOYSTICKINPUT_SDLGAME = nullptr;
const module_type JOYSTICKINPUT_SDLJOY = nullptr;
const module_type JOYSTICKINPUT_WINHYBRID = nullptr;
const module_type JOYSTICKINPUT_DINPUT = nullptr;
const module_type JOYSTICKINPUT_XINPUT = nullptr;

const module_type OUTPUT_WIN32 = nullptr;
