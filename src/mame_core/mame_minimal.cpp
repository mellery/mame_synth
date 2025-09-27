#include "mame_minimal.h"
#include <algorithm>
#include <iostream>

// Minimal machine configuration implementation
minimal_machine_config::minimal_machine_config() {
    std::cout << "Minimal MAME machine configuration created" << std::endl;
}

minimal_machine_config::~minimal_machine_config() {
    std::cout << "Minimal MAME machine configuration destroyed" << std::endl;
}

void minimal_machine_config::register_device(device_t* device) {
    if (device) {
        m_devices.push_back(device);
        std::cout << "Device registered with minimal machine config" << std::endl;
    }
}

void minimal_machine_config::unregister_device(device_t* device) {
    if (device) {
        auto it = std::find(m_devices.begin(), m_devices.end(), device);
        if (it != m_devices.end()) {
            m_devices.erase(it);
            std::cout << "Device unregistered from minimal machine config" << std::endl;
        }
    }
}

// Minimal device implementation
minimal_device_t::minimal_device_t(minimal_machine_config* config, const char* tag, u32 clock)
    : m_config(config), m_tag(tag ? tag : ""), m_clock(clock) {

    if (m_config) {
        m_config->register_device(reinterpret_cast<device_t*>(this));
    }

    std::cout << "Minimal device '" << m_tag << "' created @ " << m_clock << "Hz" << std::endl;
}

minimal_device_t::~minimal_device_t() {
    if (m_config) {
        m_config->unregister_device(reinterpret_cast<device_t*>(this));
    }
    std::cout << "Minimal device '" << m_tag << "' destroyed" << std::endl;
}

// Minimal sound interface implementation
minimal_device_sound_interface::minimal_device_sound_interface(minimal_device_t* device)
    : m_device(device) {
    std::cout << "Minimal sound interface created for device '"
              << (device ? device->tag() : "unknown") << "'" << std::endl;
}

minimal_device_sound_interface::~minimal_device_sound_interface() {
    std::cout << "Minimal sound interface destroyed" << std::endl;
}

void minimal_device_sound_interface::allocate_stream(int inputs, int outputs) {
    std::cout << "Allocated sound stream with " << inputs << " inputs, "
              << outputs << " outputs" << std::endl;
    m_stream_allocated = true;
}

// Machine manager implementation
minimal_machine_manager& minimal_machine_manager::instance() {
    static minimal_machine_manager manager;
    return manager;
}

void minimal_machine_manager::set_machine_config(minimal_machine_config* config) {
    m_config = config;
    if (config) {
        std::cout << "Minimal machine manager initialized with config" << std::endl;
    } else {
        std::cout << "Minimal machine manager config cleared" << std::endl;
    }
}