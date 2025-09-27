#include "../src/mame_integration.h"
#include <iostream>
#include <cassert>
#include <memory>

// Test MAME machine context
void test_mame_machine_context() {
    std::cout << "\n=== test_mame_machine_context ===" << std::endl;

    mame_machine_context machine;

    // Test initialization
    assert(machine.initialize() == true);
    assert(machine.is_initialized() == true);

    // Test device creation
    auto nes_device = machine.create_nes_apu("test_nes", 1789773);
    assert(nes_device != nullptr);
    assert(nes_device->get_device_name() == "NES APU (MAME)");
    assert(nes_device->get_device_tag() == "test_nes");

    auto snes_device = machine.create_snes_dsp("test_snes", 24576000);
    assert(snes_device != nullptr);
    assert(snes_device->get_device_name() == "SNES S-DSP (MAME - Placeholder)");
    assert(snes_device->get_device_tag() == "test_snes");

    // Test device initialization
    assert(nes_device->initialize() == true);
    assert(nes_device->is_initialized() == true);
    assert(snes_device->initialize() == true);
    assert(snes_device->is_initialized() == true);

    std::cout << "Machine info: " << machine.get_info() << std::endl;

    std::cout << "[PASS] mame_machine_context test passed" << std::endl;
}

// Test MAME NES APU device wrapper
void test_mame_nes_apu_device() {
    std::cout << "\n=== test_mame_nes_apu_device ===" << std::endl;

    mame_machine_context machine;
    machine.initialize();

    auto nes_device = machine.create_nes_apu("test_nes", 1789773);
    assert(nes_device->initialize() == true);

    // Test register interface
    nes_device->write_register(0x15, 0x1F); // Enable all channels
    uint8_t status = nes_device->read_register(0x15);
    std::cout << "Status register returned: 0x" << std::hex << (int)status
              << " (expected 0xF)" << std::endl;
    assert(status == 0x0F); // Only bits 0-3 are valid for channel status

    // Test NES-specific register helpers
    auto* nes_specific = dynamic_cast<mame_nes_apu_device*>(nes_device.get());
    assert(nes_specific != nullptr);

    nes_specific->write_pulse1_control(0x4F);      // $4000
    nes_specific->write_pulse1_sweep(0x08);        // $4001
    nes_specific->write_pulse1_timer_low(0xAA);    // $4002
    nes_specific->write_pulse1_timer_high(0x03);   // $4003

    // Test audio output (should be silent in placeholder)
    std::vector<int16_t> buffer(1024, 0x1234); // Fill with non-zero pattern
    nes_device->update_audio_stream(buffer.data(), 1024);

    // Verify buffer was cleared to silence
    for (int16_t sample : buffer) {
        assert(sample == 0);
    }

    assert(nes_device->get_sample_rate() == 44100);

    nes_device->reset();
    nes_device->shutdown();

    std::cout << "[PASS] mame_nes_apu_device test passed" << std::endl;
}

// Test MAME device factory
void test_mame_device_factory() {
    std::cout << "\n=== test_mame_device_factory ===" << std::endl;

    mame_machine_context machine;
    machine.initialize();

    mame_device_factory factory(&machine);

    // Test supported devices
    auto devices = factory.get_supported_devices();
    assert(devices.size() == 2);
    assert(devices[0] == "NES APU");
    assert(devices[1] == "SNES S-DSP");

    // Test device creation
    auto nes_device = factory.create_nes_apu("factory_nes");
    assert(nes_device != nullptr);
    assert(nes_device->get_device_name() == "NES APU (MAME)");

    auto snes_device = factory.create_snes_dsp("factory_snes");
    assert(snes_device != nullptr);
    assert(snes_device->get_device_name() == "SNES S-DSP (MAME - Placeholder)");

    std::cout << "[PASS] mame_device_factory test passed" << std::endl;
}

// Test error conditions
void test_mame_error_handling() {
    std::cout << "\n=== test_mame_error_handling ===" << std::endl;

    // Test device creation without machine initialization
    mame_machine_context machine; // Not initialized

    auto nes_device = machine.create_nes_apu("test_nes", 1789773);
    assert(nes_device == nullptr); // Should fail to create device without initialized machine

    // Test invalid register access
    machine.initialize();
    nes_device = machine.create_nes_apu("test_nes", 1789773);
    nes_device->initialize();

    // Invalid register offsets
    nes_device->write_register(0xFF, 0x42); // Should be logged as invalid
    uint8_t invalid_read = nes_device->read_register(0xFF);
    assert(invalid_read == 0);

    std::cout << "[PASS] mame_error_handling test passed" << std::endl;
}

int main() {
    std::cout << "\n=== MAME Integration Test Suite ===" << std::endl;

    test_mame_machine_context();
    test_mame_nes_apu_device();
    test_mame_device_factory();
    test_mame_error_handling();

    std::cout << "\n=== All MAME Integration Tests Passed! ===" << std::endl;

    return 0;
}