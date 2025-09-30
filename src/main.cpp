#include "nes_cli.h"
#include "debug_config.h"
#include <cstring>
#include <vector>

// Main entry point - use CLI application
int main(int argc, char* argv[]) {
    // Check for debug flags and filter them out for CLI
    std::vector<char*> filtered_args;
    filtered_args.push_back(argv[0]); // Keep program name

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--debug") == 0 ||
            std::strcmp(argv[i], "--debug-export") == 0) {
            // Enable export debugging preset
            g_debug_config = debug_presets::export_debugging();
            debug_logger::enable_file_logging("debug_mame_synth.log");
            std::cout << "Debug logging enabled (output: debug_mame_synth.log)" << std::endl;
        } else if (std::strcmp(argv[i], "--debug-all") == 0) {
            // Enable comprehensive debugging
            g_debug_config = debug_presets::comprehensive_debugging();
            debug_logger::enable_file_logging("debug_mame_synth.log");
            std::cout << "Comprehensive debug logging enabled (output: debug_mame_synth.log)" << std::endl;
        } else {
            // Not a debug flag, pass it to CLI
            filtered_args.push_back(argv[i]);
        }
    }

    return nes_cli_app::main(static_cast<int>(filtered_args.size()), filtered_args.data());
}