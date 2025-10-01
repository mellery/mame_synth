// Additional MAME stubs for unused subsystems
// These are for features we don't use in an audio-only synthesizer

#include "emu.h"

// Simple forward-declared stubs using extern "C" linkage where possible

// Network stubs
extern "C" {
    // NanoSVG stubs (used by screen rendering)
    void* nsvgParse(char*, const char*, float) { return nullptr; }
    void nsvgDelete(void*) {}
    void* nsvgCreateRasterizer() { return nullptr; }
    void nsvgDeleteRasterizer(void*) {}
    void nsvgRasterize(void*, void*, float, float, float, unsigned char*, int, int, int) {}
}

// OSD debugger break stub
void osd_break_into_debugger(char const*) {
    // Don't break - we're not debugging
}

// Include necessary headers for stubs
#include "rendutil.h"
#include "video/rgbgen.h"

// Layout stubs - extern to make them visible
extern const internal_layout layout_noscreens = { 0, 0, internal_layout::compression::NONE, nullptr };
extern const internal_layout layout_monitors = { 0, 0, internal_layout::compression::NONE, nullptr };
extern const internal_layout layout_dualhsxs = { 0, 0, internal_layout::compression::NONE, nullptr };
extern const internal_layout layout_dualhovu = { 0, 0, internal_layout::compression::NONE, nullptr };
extern const internal_layout layout_dualhuov = { 0, 0, internal_layout::compression::NONE, nullptr };
extern const internal_layout layout_triphsxs = { 0, 0, internal_layout::compression::NONE, nullptr };
extern const internal_layout layout_quadhsxs = { 0, 0, internal_layout::compression::NONE, nullptr };

// Network handler stub
namespace osd {
class network_handler {
public:
    network_handler();
    virtual ~network_handler();
};

network_handler::network_handler() {}
network_handler::~network_handler() {}
}

// HTTP manager stub - will be linked as weak symbol
// The actual implementation is in machine.cpp but we may need this for some builds

// Hash file stub
#include "hash.h"
void hashfile_extrainfo(char const*, game_driver const&, util::hash_collection const&, std::string&) {}

// emulator_info layout script stub
#include "rendlay.h"
namespace emulator_info {
    void layout_script_cb(layout_file&, char const*) {}
}

// disasm_interface typeinfo - just provide the typeinfo, no implementation needed

// AVI file stub
#include "aviio.h"
avi_file::error avi_file::create(std::string const&, avi_file::movie_info const&, std::unique_ptr<avi_file>&) {
    return avi_file::error::NONE;
}
const char * avi_file::error_string(avi_file::error) { return ""; }

// path2regex stub
namespace path2regex {
struct Token {};
void path_to_regex(std::string const&, std::vector<Token>&, std::map<std::string, bool> const&) {}
}
