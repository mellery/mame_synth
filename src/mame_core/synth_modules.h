// license:BSD-3-Clause
// Custom OSD modules for MAME Synthesizer

#pragma once

#include "modules/osdmodule.h"

// Sound capture module - captures audio instead of playing it
extern const module_type SOUND_CAPTURE;

// Renderer modules - minimal no-op implementations
extern const module_type RENDERER_CAPTURE;  // For future: capture video/screenshots if needed

// Monitor module - minimal implementation
extern const module_type MONITOR_MINIMAL;
