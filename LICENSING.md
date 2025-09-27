# Licensing Information

## Overview

MAME Synth is licensed under the **GNU General Public License version 2 (GPL-2.0)** to ensure compatibility with the MAME project, which this software uses as a submodule.

## Why GPL-2.0?

This project integrates with MAME (Multiple Arcade Machine Emulator), which is licensed under GPL-2.0. Since our software:

1. Uses MAME as a submodule
2. Interfaces directly with MAME's audio device implementations
3. May be considered a derivative work under GPL terms

We have chosen GPL-2.0 to ensure full compatibility and compliance with MAME's licensing requirements.

## License Compatibility

The GPL-2.0 license is compatible with:
- GPL-3.0 (upgradeable)
- LGPL (can be used in GPL projects)
- MIT/BSD licensed code (can be incorporated into GPL projects)

## Dependencies and Submodules

### MAME
- **License**: GNU General Public License version 2
- **Copyright**: (c) 1997-2025 MAMEdev and contributors
- **Usage**: Git submodule providing audio device emulation
- **Location**: `./mame/` directory
- **License Text**: `mame/docs/legal/GPL-2.0`

### Project Components
All original code in this project (outside the MAME submodule) is:
- **License**: GNU General Public License version 2
- **Copyright**: (c) 2024 MAME Synth Contributors
- **Components**:
  - Music parsing system (`src/music_parser.*`)
  - Audio device abstraction (`src/audio_device.*`)
  - Register mapping system (`src/register_mapping.*`)
  - Machine context stub (`src/machine_stub.*`)
  - Unit testing framework (`tests/`)

## Usage Guidelines

### For End Users
- You may use this software freely for personal or commercial purposes
- You may redistribute the software under GPL-2.0 terms
- Source code must be made available when distributing binaries

### For Developers
- You may modify and extend this software
- Any derivative works must also be licensed under GPL-2.0 or compatible license
- You must preserve copyright notices and license information
- Modified versions must be clearly marked as such

### For Commercial Use
- Commercial use is permitted under GPL-2.0
- If you distribute the software commercially, you must:
  - Provide source code to recipients
  - Include the complete license text
  - Preserve all copyright notices
  - Make any modifications available under GPL-2.0

## Contributing

By contributing to this project, you agree that your contributions will be licensed under the same GPL-2.0 license as the rest of the project.

## Trademark Information

- **MAME** is a registered trademark of Gregory Ember
- This project is not affiliated with or endorsed by MAMEdev or Gregory Ember
- Use of the MAME name is for descriptive and compatibility purposes only

## License Text

The complete GPL-2.0 license text is available in:
- [`LICENSE`](./LICENSE) (this project)
- [`mame/docs/legal/GPL-2.0`](./mame/docs/legal/GPL-2.0) (MAME's copy)

## Contact

For licensing questions or concerns, please open an issue on the project repository.