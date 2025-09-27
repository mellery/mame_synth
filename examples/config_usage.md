# NES Configuration System Usage Examples

The NES Synthesizer includes a comprehensive configuration system that allows you to customize all aspects of the audio engine, performance settings, and user interface.

## Available Configuration Commands

### Loading and Saving Configurations

```bash
# Load configuration from file
config-load config/authentic.json

# Save current configuration
config-save my_settings.json

# Load a preset configuration
config-preset performance    # Options: performance, quality, authentic, creative
```

### Viewing Configuration

```bash
# Show complete configuration summary
config-show

# Show specific section
config-show audio           # Options: audio, performance, ui
```

### Managing Configuration Values

```bash
# Get specific configuration value
config-get audio.sample_rate
config-get performance.enable_multithreading

# Set specific configuration value
config-set audio.sample_rate 48000
config-set audio.enable_nonlinear_mixing true
config-set performance.worker_thread_count 4
```

### Validation and Discovery

```bash
# Validate current configuration
config-validate

# Validate a configuration file
config-validate config/my_settings.json

# List available configuration files
config-list
config-list config/          # List files in specific directory

# Reset to default configuration
config-reset
```

## Configuration Presets

### Performance Preset
Optimized for maximum speed and minimum latency:
- Simplified mixing algorithms
- Reduced buffer sizes
- Disabled advanced filters
- Fast math optimizations

```bash
config-preset performance
```

### Quality Preset
Optimized for best audio quality:
- Hardware-accurate mixing
- Anti-aliasing filters
- Stereo separation effects
- Higher sample rates

```bash
config-preset quality
```

### Authentic Preset
Hardware-accurate NES emulation:
- Exact NES frequency response
- Hardware-accurate volume scaling
- Period-correct limitations
- No modern enhancements

```bash
config-preset authentic
```

### Creative Preset
Enhanced features for modern music production:
- Extended polyphony support
- Stereo effects
- Reverb processing
- Relaxed hardware constraints

```bash
config-preset creative
```

## Key Configuration Sections

### Audio Settings
Control audio processing and quality:

```bash
# Sample rate (8000-192000)
config-set audio.sample_rate 44100

# Buffer size (64-8192)
config-set audio.buffer_size 1024

# Hardware-accurate mixing
config-set audio.enable_nonlinear_mixing true

# NES filters
config-set audio.enable_highpass_filter true
config-set audio.enable_lowpass_filter true

# Channel volume scaling
config-set audio.pulse1_volume_scale 1.0
config-set audio.triangle_volume_scale 0.9
config-set audio.noise_volume_scale 0.7
config-set audio.dmc_volume_scale 0.5
```

### Performance Settings
Optimize for your system:

```bash
# Enable multithreading
config-set performance.enable_multithreading true

# Worker thread count (0 = auto-detect)
config-set performance.worker_thread_count 0

# Sample cache size in MB
config-set performance.sample_cache_size_mb 16

# Maximum polyphony
config-set performance.max_polyphony 5
```

### User Interface Settings
Customize CLI behavior:

```bash
# Verbose output
config-set ui.verbose_output false

# Progress bars
config-set ui.use_progress_bars true

# Colored output
config-set ui.enable_colored_output true

# Log level (debug, info, warn, error)
config-set ui.log_level info
```

## Configuration Files

### JSON Format (default)
Comprehensive, human-readable format with full metadata:

```json
{
  "config_version": "1.0",
  "config_name": "my_settings",
  "description": "Custom NES configuration",
  "audio": {
    "sample_rate": 44100,
    "buffer_size": 1024,
    "enable_nonlinear_mixing": true
  }
}
```

### INI Format
Simple, traditional configuration format:

```ini
[audio]
sample_rate = 44100
buffer_size = 1024
enable_nonlinear_mixing = true

[performance]
enable_multithreading = true
worker_thread_count = 0
```

### Binary Format
Compact format for embedded systems:
- Use `.bin` or `.dat` extension
- Optimized for size and speed
- Less human-readable

## Advanced Usage

### Creating Custom Configurations

1. Start with a preset:
   ```bash
   config-preset quality
   ```

2. Customize specific settings:
   ```bash
   config-set audio.sample_rate 48000
   config-set performance.sample_cache_size_mb 32
   ```

3. Save your configuration:
   ```bash
   config-save my_custom_config.json
   ```

### System-Wide vs User Configurations

- **System configurations**: Place in `config/` directory
- **User configurations**: Stored in `~/.config/nes_synth/`
- **Project configurations**: Any local directory

### Configuration Validation

The system automatically validates configurations and provides helpful error messages:

```bash
config-validate
# Output: Current configuration is valid

config-set audio.sample_rate 999999
config-validate
# Output: Current configuration is invalid: Sample rate must be between 8000 and 192000 Hz
```

## Tips and Best Practices

1. **Start with presets**: Use built-in presets as starting points
2. **Validate frequently**: Run `config-validate` after making changes
3. **Save working configurations**: Keep backups of configurations that work well
4. **Use descriptive names**: Give your configurations meaningful names and descriptions
5. **Test performance**: Use different presets to find the best balance for your system

## Troubleshooting

### Common Configuration Issues

1. **Invalid sample rate**: Must be between 8000-192000 Hz
2. **Buffer size too small**: Minimum 64 samples
3. **Memory limits**: Sample cache cannot exceed 1024 MB
4. **Thread count**: Cannot exceed system capabilities

### Recovery Options

```bash
# Reset to defaults if configuration is broken
config-reset

# Load a known-good configuration
config-load config/default.json

# Use the performance preset for minimal requirements
config-preset performance
```