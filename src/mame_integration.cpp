// Include real MAME core FIRST - must come before any of our headers
#include "emu.h"
#include "emuopts.h"
#include "main.h"  // For machine_manager complete type

// Include NES APU device
#include "sound/nes_apu.h"

// Include audio synth driver
#include "mame_core/audio_synth_driver.h"
#include "mame_core/minimal_osd.h"

// Now include our headers
#include "mame_integration.h"
#include "debug_config.h"
#include <iostream>
#include <cstring>
#include <fstream>

// Include minimal MAME core implementation (for minimal_machine_config wrapper)
#include "mame_core/mame_minimal.h"

// MAME Machine Context Implementation
mame_machine_context::mame_machine_context() {
    std::cout << "Initializing MAME machine context..." << std::endl;
}

mame_machine_context::~mame_machine_context() {
    shutdown();
}

bool mame_machine_context::initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "Setting up MAME runtime (running_machine)..." << std::endl;

    try {
        // 1. Create osd_options (not emu_options - needed for osd_common_t)
        std::cout << "  Creating osd_options..." << std::endl;
        m_options = new osd_options();
        m_options->set_value(OPTION_SAMPLERATE, std::to_string(m_sample_rate), OPTION_PRIORITY_CMDLINE);

        // 2. Create minimal OSD interface (now inherits from osd_common_t)
        std::cout << "  Creating minimal OSD interface (osd_common_t-based)..." << std::endl;
        m_osd = new minimal_osd_interface(*m_options);

        // 3. Create machine_manager
        std::cout << "  Creating machine_manager..." << std::endl;
        m_manager = create_minimal_machine_manager(*m_options, *m_osd);

        // 4. Create machine_config from our audiosynth driver (with NES APU devices)
        std::cout << "  Creating machine_config from audiosynth driver..." << std::endl;
        const game_driver &driver = GAME_NAME(audiosynth);
        m_real_machine_config = std::make_unique<machine_config>(driver, *m_options);

        std::cout << "  machine_config created with NES APU devices!" << std::endl;

        // 5. Create running_machine to initialize devices
        std::cout << "  Creating running_machine..." << std::endl;
        m_running_machine = std::make_unique<running_machine>(*m_real_machine_config, *m_manager);

        // 6. Start devices by running machine briefly
        //    machine.run() will call start() internally which initializes all devices
        //    We'll run in a separate thread and schedule immediate exit
        std::cout << "  running_machine created - starting devices via machine.run()..." << std::endl;

        std::atomic<bool> machine_started{false};
        std::atomic<bool> exit_scheduled{false};

        std::thread machine_thread([this, &machine_started, &exit_scheduled]() {
            try {
                std::cout << "  Machine thread: Calling machine.run()..." << std::endl;
                machine_started = true;

                // This will call start() which initializes devices,
                // then enter the emulation loop
                // quiet = false to see output
                int result = m_running_machine->run(false);

                std::cout << "  Machine thread: run() returned with code " << result << std::endl;
            } catch (const std::exception& e) {
                std::cout << "  Machine thread error: " << e.what() << std::endl;
            }
        });

        // Wait for machine to start
        std::cout << "  Waiting for machine to start devices..." << std::endl;
        while (!machine_started) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Give it significant time to complete start() and soft_reset()
        std::cout << "  Allowing time for device initialization..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Devices should now be started - detach the emulation thread
        // and let it run in the background. We'll use the devices from the main thread.
        std::cout << "  Detaching machine thread (letting emulation run in background)..." << std::endl;
        machine_thread.detach();

        std::cout << "  Devices should now be started and ready for use" << std::endl;

    } catch (const std::exception &e) {
        std::cout << "  ERROR: Exception creating MAME runtime: " << e.what() << std::endl;
        return false;
    }

    // Also create legacy wrapper for compatibility
    if (!setup_machine_config()) {
        std::cout << "Warning: Failed to setup legacy machine configuration wrapper" << std::endl;
        // Not fatal - continue
    }

    m_initialized = true;
    std::cout << "MAME machine context initialized successfully with running_machine" << std::endl;
    return true;
}

void mame_machine_context::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "Shutting down MAME machine context..." << std::endl;

    // Clean up any registered devices
    for (auto* device : m_devices) {
        // Devices are owned by running_machine, so we just clear our pointers
        (void)device; // Suppress unused variable warning
        std::cout << "  Clearing MAME device reference" << std::endl;
    }
    m_devices.clear();

    // Clean up MAME infrastructure in reverse order
    std::cout << "  Destroying running_machine..." << std::endl;
    m_running_machine.reset();

    std::cout << "  Destroying machine_config..." << std::endl;
    m_real_machine_config.reset();

    std::cout << "  Destroying machine_manager..." << std::endl;
    if (m_manager) {
        delete m_manager;
        m_manager = nullptr;
    }

    std::cout << "  Destroying OSD interface..." << std::endl;
    if (m_osd) {
        destroy_minimal_osd_interface(m_osd);
        m_osd = nullptr;
    }

    std::cout << "  Destroying emu_options..." << std::endl;
    if (m_options) {
        delete m_options;
        m_options = nullptr;
    }

    cleanup_machine_config();
    m_initialized = false;
    std::cout << "MAME machine context shut down" << std::endl;
}

std::string mame_machine_context::get_info() const {
    if (!m_initialized) {
        return "MAME machine context: Not initialized";
    }

    return "MAME machine context: Initialized\n"
           "Sample rate: " + std::to_string(m_sample_rate) + "Hz\n"
           "Active devices: " + std::to_string(m_devices.size());
}

std::unique_ptr<mame_audio_device_base> mame_machine_context::create_nes_apu(
    const std::string& tag, uint32_t clock_rate) {

    if (!m_initialized) {
        std::cout << "Cannot create NES APU: machine context not initialized" << std::endl;
        return nullptr;
    }

    std::cout << "Creating MAME NES APU device '" << tag << "' @ " << clock_rate << "Hz" << std::endl;
    auto device = std::make_unique<mame_nes_apu_device>(this, tag, clock_rate);

    // Set up OSD audio callback to route MAME's audio to this device
    set_audio_callback([dev = device.get()](const int16_t* buffer, int sample_count) {
        dev->on_audio_callback(buffer, sample_count);
    });

    return device;
}

std::unique_ptr<mame_audio_device_base> mame_machine_context::create_snes_dsp(
    const std::string& tag, uint32_t clock_rate) {

    if (!m_initialized) {
        std::cout << "Cannot create SNES S-DSP: machine context not initialized" << std::endl;
        return nullptr;
    }

    std::cout << "Creating MAME SNES S-DSP device '" << tag << "' @ " << clock_rate << "Hz" << std::endl;
    return std::make_unique<mame_snes_dsp_device>(this, tag, clock_rate);
}

void mame_machine_context::register_device(device_t* device) {
    if (device) {
        m_devices.push_back(device);
    }
}

void mame_machine_context::set_audio_callback(audio_callback_t callback) {
    if (!m_osd) {
        std::cout << "ERROR: Cannot set audio callback - OSD not initialized" << std::endl;
        return;
    }

    // Cast to our minimal_osd_interface to access the set_audio_callback method
    minimal_osd_interface* minimal_osd = static_cast<minimal_osd_interface*>(m_osd);
    if (minimal_osd) {
        minimal_osd->set_audio_callback(callback);
        std::cout << "Audio callback registered with MAME OSD" << std::endl;
    }
}

bool mame_machine_context::setup_machine_config() {
    // Create real minimal MAME machine configuration
    m_machine_config = new minimal_machine_config();
    if (!m_machine_config) {
        return false;
    }

    // Configure sample rate
    m_machine_config->set_sample_rate(m_sample_rate);

    std::cout << "Real minimal MAME machine configuration created" << std::endl;
    return true;
}

void mame_machine_context::cleanup_machine_config() {
    // Clean up the real minimal MAME machine config
    if (m_machine_config) {
        delete m_machine_config;
        m_machine_config = nullptr;
    }

    // Clear machine manager
    minimal_machine_manager::instance().set_machine_config(nullptr);
}

// NES APU MAME Device Implementation
mame_nes_apu_device::mame_nes_apu_device(mame_machine_context* machine_ctx,
                                         const std::string& tag, uint32_t clock_rate) {

    m_machine_context = machine_ctx;
    m_tag = tag;
    m_clock_rate = clock_rate;

    // Pre-allocate ring buffer - 1 second at 44100Hz stereo
    m_audio_buffer.resize(44100 * 2);
    m_buffer_write_pos = 0;
    m_buffer_read_pos = 0;

    std::cout << "  NES APU device created with tag '" << tag << "'" << std::endl;
}

mame_nes_apu_device::~mame_nes_apu_device() {
    shutdown();
}

bool mame_nes_apu_device::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!m_machine_context || !m_machine_context->is_initialized()) {
        std::cout << "Cannot initialize NES APU: invalid machine context" << std::endl;
        return false;
    }

    std::cout << "  Looking up NES APU device from machine_config..." << std::endl;

    try {
        // Get machine_config from context
        machine_config *config = m_machine_context->get_real_machine_config();
        if (!config) {
            std::cout << "  ERROR: No machine_config available" << std::endl;
            return false;
        }

        // Look up the pre-existing NES APU device by name
        // The device was already created when the driver's machine_config was built
        std::cout << "  Looking for device with tag: " << m_tag << std::endl;

        m_apu = dynamic_cast<nesapu_device *>(config->device(m_tag.c_str()));

        if (!m_apu) {
            std::cout << "  ERROR: NES APU device '" << m_tag << "' not found in running_machine" << std::endl;
            std::cout << "  Available devices in machine:" << std::endl;
            // Try to list available devices for debugging
            return false;
        }

        std::cout << "  Found NES APU device '" << m_tag << "' successfully!" << std::endl;

        // Register with machine context (for tracking)
        m_machine_context->register_device(m_apu);

    } catch (const std::exception &e) {
        std::cout << "  ERROR: Exception looking up NES APU device: " << e.what() << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "  MAME NES APU device initialized successfully" << std::endl;
    return true;
}

void mame_nes_apu_device::reset() {
    if (!m_initialized || !m_apu) {
        return;
    }

    // Only reset if the device has been started (has sound_stream initialized)
    // With lazy initialization, the device may not have been started yet
    if (!m_apu->started()) {
        std::cout << "  MAME NES APU device not started yet - skipping reset" << std::endl;
        return;
    }

    std::cout << "  Resetting MAME NES APU device..." << std::endl;

    // Reset the APU (device_reset is public for nesapu_device)
    m_apu->device_reset();

    std::cout << "  MAME NES APU device reset" << std::endl;
}

void mame_nes_apu_device::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "  Shutting down MAME NES APU device..." << std::endl;

    // Clear our pointer to the device
    // The device itself is owned and will be destroyed by running_machine
    if (m_apu) {
        std::cout << "  Clearing nesapu_device pointer (owned by running_machine)" << std::endl;
        m_apu = nullptr;
    }

    m_initialized = false;
    std::cout << "  MAME NES APU device shut down" << std::endl;
}

bool mame_nes_apu_device::is_device_started() const {
    if (!m_initialized || !m_apu) {
        return false;
    }
    // Check if the MAME device has been started (has sound_stream initialized)
    return m_apu->started();
}

void mame_nes_apu_device::write_register(uint32_t offset, uint8_t value) {
    if (!m_initialized || !m_apu) {
        std::cout << "Cannot write register: device not initialized" << std::endl;
        return;
    }

    // Check if device has been started - if not, try reset to initialize
    if (!m_apu->started()) {
        static bool tried_reset = false;
        if (!tried_reset) {
            std::cout << "MAME NES APU: Device not started, trying device_reset()..." << std::endl;
            m_apu->reset();
            tried_reset = true;
            if (m_apu->started()) {
                std::cout << "MAME NES APU: Device started via reset!" << std::endl;
            } else {
                std::cout << "MAME NES APU: Device still not started after reset" << std::endl;
            }
        }
    }

    if (offset > 0x17) {
        std::cout << "Invalid NES APU register offset: $" << std::hex << offset << std::endl;
        return;
    }

    std::cout << "MAME NES APU: Writing $" << std::hex << static_cast<int>(value)
              << " to register $" << std::hex << offset << std::dec << std::endl;

    // Debug logging for register writes
    if (g_debug_config.log_register_writes) {
        const char* reg_names[] = {
            "SQ1_VOL", "SQ1_SWEEP", "SQ1_LO", "SQ1_HI",      // $4000-$4003
            "SQ2_VOL", "SQ2_SWEEP", "SQ2_LO", "SQ2_HI",      // $4004-$4007
            "TRI_LINEAR", "unused", "TRI_LO", "TRI_HI",      // $4008-$400B
            "NOISE_VOL", "unused", "NOISE_LO", "NOISE_HI",   // $400C-$400F
            "DMC_FREQ", "DMC_RAW", "DMC_START", "DMC_LEN",   // $4010-$4013
            "unused", "SND_CHN", "unused", "FRAME_CNT"       // $4014-$4017
        };
        std::string desc = (offset <= 0x17) ? reg_names[offset] : "UNKNOWN";
        DEBUG_LOG_REGISTER(offset, value, desc);
    }

    // Write to the APU subdevice
    // Devices should now be properly started via machine.run()
    if (!m_apu->started()) {
        std::cout << "WARNING: MAME NES APU device not started, write may fail" << std::endl;
    }

    try {
        m_apu->write(offset, value);
    } catch (const std::exception& e) {
        std::cout << "EXCEPTION in m_apu->write(): " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cout << "UNKNOWN EXCEPTION in m_apu->write()" << std::endl;
        throw;
    }
}

uint8_t mame_nes_apu_device::read_register(uint32_t offset) const {
    if (!m_initialized || !m_apu) {
        std::cout << "Cannot read register: device not initialized" << std::endl;
        return 0;
    }

    if (offset > 0x17) {
        std::cout << "Invalid NES APU register offset: $" << std::hex << offset << std::endl;
        return 0;
    }

    // Read from the APU subdevice
    if (offset == 0x15) {
        return m_apu->status_r();
    }

    // Most registers are write-only
    return 0;
}

void mame_nes_apu_device::update_audio_stream(int16_t* buffer, size_t sample_count) {
    if (!m_initialized || !m_apu) {
        // Fill with silence
        std::memset(buffer, 0, sample_count * sizeof(int16_t));
        return;
    }

    // MAME's sound system generates audio automatically when:
    // 1. Registers are written (which we do in write_register)
    // 2. The sound_manager's timer fires periodically
    // 3. The OSD callback receives the mixed audio
    //
    // For now, read from the ring buffer that gets filled by the OSD callback

    // Read samples from ring buffer
    std::lock_guard<std::mutex> lock(m_buffer_mutex);

    size_t samples_available = 0;
    if (m_buffer_write_pos >= m_buffer_read_pos) {
        samples_available = m_buffer_write_pos - m_buffer_read_pos;
    } else {
        samples_available = m_audio_buffer.size() - m_buffer_read_pos + m_buffer_write_pos;
    }

    size_t samples_to_copy = std::min(sample_count, samples_available);

    // Copy samples from ring buffer
    for (size_t i = 0; i < samples_to_copy; i++) {
        buffer[i] = m_audio_buffer[m_buffer_read_pos];
        m_buffer_read_pos = (m_buffer_read_pos + 1) % m_audio_buffer.size();
    }

    // Fill remaining with silence if not enough samples
    if (samples_to_copy < sample_count) {
        std::memset(buffer + samples_to_copy, 0, (sample_count - samples_to_copy) * sizeof(int16_t));
    }

    // DEBUG: Check what MAME is generating
    static int call_count = 0;
    static bool found_mame_audio = false;
    if (!found_mame_audio && call_count < 10000) {
        int16_t max_val = 0;
        int max_idx = 0;
        for (size_t i = 0; i < sample_count; i++) {
            if (abs(buffer[i]) > abs(max_val)) {
                max_val = buffer[i];
                max_idx = i;
            }
        }
        if (call_count < 10 || abs(max_val) > 1000) {
            std::cout << "MAME_gen[" << call_count << "]: max=" << max_val
                      << " at idx=" << max_idx
                      << ", first_5=[" << buffer[0] << "," << buffer[1] << ","
                      << buffer[2] << "," << buffer[3] << "," << buffer[4] << "]" << std::endl;
            if (abs(max_val) > 3000) {
                found_mame_audio = true;
                std::cout << "*** MAME FIRST AUDIO AT CALL " << call_count << ", sample " << (call_count * sample_count + max_idx) << " ***" << std::endl;
            }
        }
        call_count++;
    }

    // Debug: Dump audio to WAV file (DISABLED for export testing)
    #ifdef ENABLE_DEBUG_WAV_OUTPUT
    static std::ofstream* wav_file = nullptr;
    static bool wav_header_written = false;
    static int total_samples = 0;

    if (!wav_file) {
        std::cout << "WAV: Creating debug_output.wav file" << std::endl;
        wav_file = new std::ofstream("debug_output.wav", std::ios::binary);

        // Write WAV header
        struct {
            char riff[4] = {'R', 'I', 'F', 'F'};
            uint32_t fileSize = 0; // Will update later
            char wave[4] = {'W', 'A', 'V', 'E'};
            char fmt[4] = {'f', 'm', 't', ' '};
            uint32_t fmtSize = 16;
            uint16_t audioFormat = 1;
            uint16_t numChannels = 1;
            uint32_t sampleRate = 44100;
            uint32_t byteRate = 44100 * 1 * 16 / 8;
            uint16_t blockAlign = 1 * 16 / 8;
            uint16_t bitsPerSample = 16;
            char data[4] = {'d', 'a', 't', 'a'};
            uint32_t dataSize = 0; // Will update later
        } header;

        wav_file->write(reinterpret_cast<char*>(&header), sizeof(header));
        wav_header_written = true;
    }

    // Write audio samples
    wav_file->write(reinterpret_cast<char*>(buffer), sample_count * sizeof(int16_t));
    wav_file->flush(); // Ensure data is written immediately
    total_samples += sample_count;

    // Debug: Check if we have non-zero samples
    static bool found_audio = false;
    if (!found_audio) {
        for (size_t i = 0; i < sample_count; ++i) {
            if (buffer[i] != 0) {
                std::cout << "WAV: Found non-zero audio sample: " << buffer[i] << " at position " << i << std::endl;
                found_audio = true;
                break;
            }
        }
    }

    // Close after 5 seconds
    if (total_samples > 44100 * 5) {
        if (wav_file) {
            // Update header with actual sizes
            wav_file->seekp(4);
            uint32_t fileSize = 36 + total_samples * 2;
            wav_file->write(reinterpret_cast<char*>(&fileSize), 4);

            wav_file->seekp(40);
            uint32_t dataSize = total_samples * 2;
            wav_file->write(reinterpret_cast<char*>(&dataSize), 4);

            wav_file->close();
            delete wav_file;
            wav_file = nullptr;
            std::cout << "Wrote debug_output.wav with " << total_samples << " samples" << std::endl;
        }
    }
    #endif // ENABLE_DEBUG_WAV_OUTPUT
}

uint32_t mame_nes_apu_device::get_sample_rate() const {
    if (m_apu) {
        // TODO: Get sample rate from APU device
        // return m_apu->sample_rate();
        return 44100; // Fallback for now
    }
    return 44100; // Fallback
}

device_t* mame_nes_apu_device::get_mame_device() {
    // Return the APU device
    return m_apu;
}

nesapu_device* mame_nes_apu_device::get_nes_apu_device() {
    // Return the APU subdevice
    return m_apu;
}

// OSD audio callback handler - called by MAME's sound_manager via OSD
void mame_nes_apu_device::on_audio_callback(const int16_t* buffer, int sample_count) {
    std::lock_guard<std::mutex> lock(m_buffer_mutex);

    // Write samples to ring buffer
    for (int i = 0; i < sample_count; i++) {
        m_audio_buffer[m_buffer_write_pos] = buffer[i];
        m_buffer_write_pos = (m_buffer_write_pos + 1) % m_audio_buffer.size();

        // If buffer is full, overwrite oldest samples
        if (m_buffer_write_pos == m_buffer_read_pos) {
            m_buffer_read_pos = (m_buffer_read_pos + 1) % m_audio_buffer.size();
        }
    }
}

// SNES S-DSP MAME Device Implementation (Placeholder)
mame_snes_dsp_device::mame_snes_dsp_device(mame_machine_context* machine_ctx,
                                           const std::string& tag, uint32_t clock_rate) {

    m_machine_context = machine_ctx;
    m_tag = tag;
    m_clock_rate = clock_rate;
    m_registers.resize(128, 0); // SNES S-DSP has 128 registers

    std::cout << "  SNES S-DSP device created with tag '" << tag << "' (placeholder)" << std::endl;
}

mame_snes_dsp_device::~mame_snes_dsp_device() {
    shutdown();
}

bool mame_snes_dsp_device::initialize() {
    if (m_initialized) {
        return true;
    }

    std::cout << "  Initializing MAME SNES S-DSP device (placeholder)..." << std::endl;

    // TODO: Find and integrate actual SNES S-DSP device from MAME
    // This is a placeholder implementation

    m_initialized = true;
    std::cout << "  MAME SNES S-DSP device initialized (placeholder)" << std::endl;
    return true;
}

void mame_snes_dsp_device::reset() {
    if (!m_initialized) {
        return;
    }

    std::cout << "  Resetting MAME SNES S-DSP device (placeholder)..." << std::endl;
    std::fill(m_registers.begin(), m_registers.end(), 0);
}

void mame_snes_dsp_device::shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "  Shutting down MAME SNES S-DSP device (placeholder)..." << std::endl;
    m_initialized = false;
}

void mame_snes_dsp_device::write_register(uint32_t offset, uint8_t value) {
    if (offset >= m_registers.size()) {
        std::cout << "Invalid SNES S-DSP register offset: $" << std::hex << offset << std::endl;
        return;
    }

    m_registers[offset] = value;
    std::cout << "MAME SNES S-DSP: Writing $" << std::hex << static_cast<int>(value)
              << " to register $" << std::hex << offset << std::dec << " (placeholder)" << std::endl;
}

uint8_t mame_snes_dsp_device::read_register(uint32_t offset) const {
    if (offset >= m_registers.size()) {
        return 0;
    }

    return m_registers[offset];
}

void mame_snes_dsp_device::update_audio_stream(int16_t* buffer, size_t sample_count) {
    // Generate silence for placeholder
    std::memset(buffer, 0, sample_count * sizeof(int16_t));
}

uint32_t mame_snes_dsp_device::get_sample_rate() const {
    return 32000; // SNES S-DSP typical sample rate
}

device_t* mame_snes_dsp_device::get_mame_device() {
    return m_snes_device;
}

// MAME Device Factory Implementation
mame_device_factory::mame_device_factory(mame_machine_context* machine_ctx)
    : m_machine_context(machine_ctx) {
}

std::unique_ptr<mame_audio_device_base> mame_device_factory::create_nes_apu(
    const std::string& tag, uint32_t clock_rate) {

    return m_machine_context->create_nes_apu(tag, clock_rate);
}

std::unique_ptr<mame_audio_device_base> mame_device_factory::create_snes_dsp(
    const std::string& tag, uint32_t clock_rate) {

    return m_machine_context->create_snes_dsp(tag, clock_rate);
}

std::vector<std::string> mame_device_factory::get_supported_devices() const {
    return {"NES APU", "SNES S-DSP"};
}