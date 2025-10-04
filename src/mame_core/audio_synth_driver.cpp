// license:GPL-2.0+
// copyright-holders:MAME Synth Project
/*************************************************************************

    audio_synth_driver.cpp

    Audio synthesizer driver - based on MAME's ___empty driver but with sound devices.
    This follows the MAmidiMEmo approach of adding sound chips to a minimal driver.

**************************************************************************/

#include "emu.h"
#include "audio_synth_driver.h"
#include "speaker.h"
#include "sound/nes_apu.h"
#include "cpu/m6502/rp2a03.h"  // NES CPU with integrated APU (RP2A03 = Ricoh version of N2A03)
#include "screen.h"  // For screen device (needed to drive audio timing)
#include "layout/generic.h"  // For layout_noscreens

//**************************************************************************
//  DRIVER STATE
//**************************************************************************

class audio_synth_state : public driver_device
{
public:
	// constructor
	audio_synth_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
	{ }

	void audio_synth(machine_config &config);

	virtual std::vector<std::string> searchpath() const override { return std::vector<std::string>(); }

protected:
	virtual void machine_start() override
	{
		// Minimal initialization for audio-only machine
		// Don't show UI chooser like ___empty does - we're running headless
	}

	void audio_synth_map(address_map &map);

	// Dummy screen update - just returns 0 (no actual drawing)
	// The screen device is only needed to drive audio timing
	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
	{
		return 0;
	}

private:
	required_device<cpu_device> m_maincpu;
};

//**************************************************************************
//  ADDRESS MAPS
//**************************************************************************

void audio_synth_state::audio_synth_map(address_map &map)
{
	// Minimal memory map for CPU execution
	// The CPU just needs somewhere to execute from - it doesn't matter what
	map(0x0000, 0xffff).ram();  // All RAM for simplicity
}

//**************************************************************************
//  MACHINE DRIVERS
//**************************************************************************

void audio_synth_state::audio_synth(machine_config &config)
{
	// Use MAME's properly compiled "no screens" layout for audio-only operation
	// This layout shows "No screens attached" message but doesn't crash
	config.set_default_layout(layout_noscreens);

	// Add dummy screen device - CRITICAL for audio timing!
	// MAME's sound system is driven by screen refresh, even for audio-only applications
	// Without a screen, sound_stream::update() is never called and audio stays silent
	// This follows MAmidiMEmo's approach (see vsnes.cpp)
	screen_device &screen(SCREEN(config, "screen", SCREEN_TYPE_RASTER));
	screen.set_refresh_hz(60);  // 60Hz refresh drives audio generation
	screen.set_size(256, 240);  // Dummy NES resolution
	screen.set_visarea(0, 255, 0, 239);
	screen.set_screen_update(FUNC(audio_synth_state::screen_update));

	// Add speakers for audio output
	// CRITICAL: Use MONO speaker like MAmidiMEmo - stereo may not route to OSD properly!
	SPEAKER(config, "mono").front_center();

	// Add RP2A03G CPU (NES CPU with integrated NES_APU)
	// Try using RP2A03G which uses NES_APU instead of APU_2A03
	// The CPU needs to run to drive MAME's scheduler and audio generation
	rp2a03_device &cpu(RP2A03G(config, m_maincpu, NTSC_APU_CLOCK));  // NTSC NES CPU clock (1.789773 MHz)
	cpu.set_addrmap(AS_PROGRAM, &audio_synth_state::audio_synth_map);

	// Route from CPU mixer to speaker
	// The RP2A03 is a device_mixer_interface that receives from APU and should output
	cpu.add_route(ALL_OUTPUTS, "mono", 0.50);

	// NOTE: The RP2A03G creates its own internal NES_APU device with tag "maincpu:nesapu"
	// The APU routes to the RP2A03 mixer (configured in rp2a03g_device::device_add_mconfig)
}

//**************************************************************************
//  ROM DEFINITIONS
//**************************************************************************

ROM_START( audiosynth )
	ROM_REGION( 0x10000, "maincpu", 0 )
	// Simple 6502 infinite loop at reset vector
	// This keeps the CPU executing so the scheduler stays active
	ROM_FILL( 0x0000, 0x10000, 0xEA )  // Fill with NOP instructions
	ROM_FILL( 0xFFFC, 2, 0x00 )        // Reset vector points to $0000
	ROM_FILL( 0xFFFD, 1, 0x00 )
ROM_END

//**************************************************************************
//  GAME DRIVERS
//**************************************************************************

GAME( 2025, audiosynth, 0, audio_synth, 0, audio_synth_state, empty_init, ROT0, "MAME Synth", "Audio Synthesizer Machine", MACHINE_SUPPORTS_SAVE )
