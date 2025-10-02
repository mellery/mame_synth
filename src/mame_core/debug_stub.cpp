// Debug and driver stubs for MAME integration
// These provide minimal implementations for functions we don't use in audio-only mode

#include <string_view>
#include <cstddef>
#include "emu.h"
#include "drivenum.h"

// Use our custom audiosynth driver (based on ___empty but with NES APU devices)
// This follows MAmidiMEmo's approach of adding sound chips to a minimal driver
GAME_EXTERN(audiosynth);

// Provide definitions for driver_list's static members (declared in drivenum.h)
// These are the global symbols MAME expects to find
std::size_t const driver_list::s_driver_count = 1;  // We have 1 driver (audiosynth)
game_driver const * const driver_list::s_drivers_sorted[] = { &GAME_NAME(audiosynth) };

// Note: We use the real find() from drivenum.cpp
// It correctly handles parent="0" by returning -1 (not found)

// Debugger manager stub
class debugger_manager {
public:
    void refresh_display() {}
};

// LZMA special function stub
extern "C" {
unsigned int GetMatchesSpecN_2(const unsigned char *, unsigned int, unsigned int, unsigned int *, unsigned int, unsigned int, unsigned int) {
    return 0;
}
}
