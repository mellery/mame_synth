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

//**************************************************************************
//  DRIVER STATE
//**************************************************************************

class audio_synth_state : public driver_device
{
public:
	// constructor
	using driver_device::driver_device;

	void audio_synth(machine_config &config);

	virtual std::vector<std::string> searchpath() const override { return std::vector<std::string>(); }

protected:
	virtual void machine_start() override
	{
		// Minimal initialization for audio-only machine
		// Don't show UI chooser like ___empty does - we're running headless
	}
};

//**************************************************************************
//  MACHINE DRIVERS
//**************************************************************************

void audio_synth_state::audio_synth(machine_config &config)
{
	// Add speakers for audio output
	SPEAKER(config, "lspeaker").front_left();
	SPEAKER(config, "rspeaker").front_right();

	// Add NES APU devices (up to 8 instances like MAmidiMEmo supports)
	// These will be accessible as "nes_apu_0" through "nes_apu_7"
	for (int i = 0; i < 8; i++)
	{
		char tag[16];
		snprintf(tag, sizeof(tag), "nes_apu_%d", i);

		// Create NES APU device with standard NES APU clock rate (1.789773 MHz for NTSC)
		NES_APU(config, tag, 1789773).add_route(ALL_OUTPUTS, "lspeaker", 0.50).add_route(ALL_OUTPUTS, "rspeaker", 0.50);
	}
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
