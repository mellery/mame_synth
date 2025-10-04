// license:BSD-3-Clause
// Sound capture module for MAME Synthesizer
// Captures audio from MAME's sound system for external processing

#include "modules/sound/sound_module.h"
#include "modules/osdmodule.h"
#include <functional>
#include <iostream>

namespace osd {

namespace {

// Sound module that captures audio instead of playing it
class sound_capture : public osd_module, public sound_module
{
public:
	sound_capture() : osd_module(OSD_SOUND_PROVIDER, "capture")
	{
	}
	virtual ~sound_capture() { }

	// Callback type for audio capture
	using audio_callback_t = std::function<void(const int16_t*, int)>;

	// Set callback for audio capture
	void set_audio_callback(audio_callback_t callback) {
		m_audio_callback = callback;
	}

	virtual int init(osd_interface &osd, const osd_options &options) override {
		std::cout << "Sound capture module initialized" << std::endl;
		m_generation = 1;
		return 0;
	}

	virtual void exit() override {
		std::cout << "Sound capture module exited" << std::endl;
	}

	virtual uint32_t get_generation() override {
		return m_generation;
	}

	virtual audio_info get_information() override
	{
		std::cout << "!!! SOUND_CAPTURE get_information() called !!!" << std::endl;

		audio_info result;
		result.m_generation = m_generation;

		// CRITICAL: Create a sink node so MAME creates output streams!
		// Without nodes, MAME's sound_manager won't create m_osd_output_streams
		audio_info::node_info sink_node;
		sink_node.m_name = "capture";
		sink_node.m_display_name = "Audio Capture";
		sink_node.m_id = 0;

		// Set audio rate range - default to 48000Hz to match NES APU
		sink_node.m_rate.m_default_rate = 48000;
		sink_node.m_rate.m_min_rate = 8000;
		sink_node.m_rate.m_max_rate = 96000;

		sink_node.m_sinks = 2;  // Stereo output
		sink_node.m_sources = 0;  // No inputs
		sink_node.m_port_names.push_back("Left");
		sink_node.m_port_names.push_back("Right");
		sink_node.m_port_positions.push_back(channel_position::FL());  // Front Left
		sink_node.m_port_positions.push_back(channel_position::FR());  // Front Right

		result.m_nodes.push_back(sink_node);
		result.m_default_sink = 0;
		result.m_default_source = 0;

		std::cout << "!!! Returning audio_info with " << result.m_nodes.size() << " sink nodes !!!" << std::endl;

		return result;
	}

	virtual uint32_t stream_sink_open(uint32_t node, std::string name, uint32_t rate) override {
		// CRITICAL: Return non-zero stream ID so MAME knows stream was created
		static uint32_t next_stream_id = 1;
		uint32_t stream_id = next_stream_id++;

		std::cout << "!!! stream_sink_open: node=" << node
		          << ", name=" << name
		          << ", rate=" << rate
		          << " -> id=" << stream_id << " !!!" << std::endl;

		return stream_id;
	}

	virtual void stream_close(uint32_t id) override {
		std::cout << "Sound capture: stream_close - id=" << id << std::endl;
	}

	virtual void stream_sink_update(uint32_t id, const int16_t *buffer, int samples_this_frame) override {
		// Forward audio to callback if registered
		if (m_audio_callback) {
			m_audio_callback(buffer, samples_this_frame);
		}

		// Debug output for first few callbacks
		static int callback_count = 0;
		if (callback_count < 5) {
			std::cout << "Sound capture: stream_sink_update #" << callback_count
			          << " - id=" << id << " samples=" << samples_this_frame << std::endl;
			callback_count++;
		}
	}

private:
	audio_callback_t m_audio_callback;
	uint32_t m_generation = 1;
};

} // anonymous namespace

} // namespace osd

MODULE_DEFINITION(SOUND_CAPTURE, osd::sound_capture)
