// Debug and driver stubs for MAME integration
// These provide minimal implementations for functions we don't use in audio-only mode

#include <string_view>
#include <cstddef>

// Forward declarations
struct game_driver;

// Driver list stubs - we use a minimal empty driver list
namespace {
    game_driver const *const s_empty_drivers[] = { nullptr };
}

// These symbols are used by MAME's driver enumeration, but we don't have any drivers
struct driver_list {
    static int s_driver_count;
    static game_driver const * const * s_drivers_sorted;
};

int driver_list::s_driver_count = 0;
game_driver const * const * driver_list::s_drivers_sorted = s_empty_drivers;

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
