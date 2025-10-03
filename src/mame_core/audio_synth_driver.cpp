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

	// Add RP2A03 CPU (NES CPU with integrated APU)
	// The CPU needs to run to drive MAME's scheduler and audio generation
	RP2A03(config, m_maincpu, NTSC_APU_CLOCK);  // NTSC NES CPU clock (1.789773 MHz)
	m_maincpu->set_addrmap(AS_PROGRAM, &audio_synth_state::audio_synth_map);

	// Add speakers for audio output
	SPEAKER(config, "lspeaker").front_left();
	SPEAKER(config, "rspeaker").front_right();

	// Add single NES APU device with standard NES APU clock rate (1.789773 MHz for NTSC)
	// Tag: "nes_apu" - provides 5 channels (2 pulse, 1 triangle, 1 noise, 1 DMC)
	NES_APU(config, "nes_apu", 1789773).add_route(ALL_OUTPUTS, "lspeaker", 0.50).add_route(ALL_OUTPUTS, "rspeaker", 0.50);
}

//**************************************************************************
//  ROM DEFINITIONS
//**************************************************************************

ROM_START( audiosynth )
ROM_END

//**************************************************************************
//  GAME DRIVERS
//**************************************************************************

GAME( 2025, audiosynth, 0, audio_synth, 0, audio_synth_state, empty_init, ROT0, "MAME Synth", "Audio Synthesizer Machine", MACHINE_SUPPORTS_SAVE )
