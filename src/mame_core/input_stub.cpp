// Minimal input_manager stub for audio-only testing
// We don't use input in our audio tests, so provide minimal stubs

// Forward declarations to avoid pulling in full MAME headers
class running_machine;
class input_code;
class input_item_id;
enum input_item_class : unsigned;
namespace osd { class input_seq; }

#include <string>
#include <string_view>
#include <map>
#include <cstdint>

// Minimal input_manager stub - just declare what we need
class input_manager {
public:
    input_manager(running_machine &machine);
    ~input_manager();

private:
    running_machine &m_machine;
};

// Constructor/destructor
input_manager::input_manager(running_machine &machine) : m_machine(machine) {
}

input_manager::~input_manager() {
}

// Stub implementations that match the actual input_manager interface
// These are extern "C++" linkage to match the class methods

extern "C++" {

const char* input_manager_standard_token(const input_manager*, input_item_id) {
    return "NONE";
}

std::string input_manager_code_name(const input_manager*, input_code) {
    return "NONE";
}

std::string input_manager_seq_to_tokens(const input_manager*, const osd::input_seq&) {
    return "";
}

osd::input_seq input_manager_seq_clean(const input_manager*, const osd::input_seq& seq) {
    return seq;
}

bool input_manager_seq_pressed(input_manager*, const osd::input_seq&) {
    return false;
}

int32_t input_manager_seq_axis_value(input_manager*, const osd::input_seq&, input_item_class&) {
    return 0;
}

input_code input_manager_code_from_token(input_manager*, std::string_view) {
    input_code invalid = {};
    return invalid;
}

void input_manager_seq_from_tokens(input_manager*, osd::input_seq&, std::string_view) {
}

bool input_manager_map_device_to_controller(input_manager*, const std::map<std::string, std::string>&) {
    return false;
}

}  // extern "C++"
