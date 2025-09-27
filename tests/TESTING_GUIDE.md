# NES Synthesizer Testing Guide

Comprehensive testing documentation for the NES synthesizer project, covering all test types, frameworks, and procedures.

## Test Architecture Overview

The NES synthesizer uses a multi-layered testing approach:

1. **Unit Tests** - Test individual components in isolation
2. **Integration Tests** - Test component interactions and workflows
3. **Performance Tests** - Measure speed and efficiency
4. **Stress Tests** - Test system behavior under load
5. **End-to-End Tests** - Test complete user workflows

## Test Framework Features

### Core Testing Framework (`test_framework.h`)
- Lightweight, no external dependencies
- Automatic test registration
- Comprehensive assertion macros
- Timing measurement
- Result reporting

### Enhanced Testing Framework (`test_framework_enhanced.h`)
- Performance testing with statistical analysis
- Stress testing with concurrent operations
- Mock objects and test utilities
- Test data generation
- Coverage tracking

## Test Categories

### 1. Unit Tests

#### Channel Assignment Tests (`test_channel_assignment.cpp`)
- **Coverage**: Track analysis, assignment algorithms, strategy management
- **Key Tests**:
  - `track_analysis_basic` - Basic frequency and note analysis
  - `track_analysis_bass` - Bass frequency detection
  - `track_analysis_drums` - Drum pattern recognition
  - `assignment_basic` - Channel assignment logic
  - `assignment_conflict_resolution` - Handling multiple tracks

#### Configuration Tests (`test_nes_config.cpp`)
- **Coverage**: Configuration management, serialization, validation
- **Key Tests**:
  - `default_configuration` - Default value verification
  - `configuration_validation` - Input validation
  - `json_serialization` - JSON format support
  - `ini_serialization` - INI format support
  - `file_operations` - File I/O operations

#### Audio Stream Tests (`test_audio_stream.cpp`)
- **Coverage**: Audio buffer management, format conversion, file output
- **Key Tests**:
  - `basic_creation` - Stream initialization
  - `buffer_operations` - Buffer read/write operations
  - `format_conversion` - Sample format conversion
  - `underrun_detection` - Buffer underrun handling

#### Sequencer Tests (`test_nes_sequencer.cpp`)
- **Coverage**: Music playback, timing, channel mapping
- **Key Tests**:
  - `basic_creation` - Sequencer initialization
  - `music_loading` - Music data loading
  - `channel_mapping` - MIDI to NES channel mapping
  - `tempo_control` - Tempo scaling and control

### 2. Integration Tests

#### End-to-End Tests (`test_end_to_end_integration.cpp`)
- **Coverage**: Complete pipeline from MIDI to audio output
- **Key Tests**:
  - `complete_pipeline` - Full MIDI → channel assignment → audio generation
  - `file_to_audio_pipeline` - File processing pipeline
  - `configuration_integration` - Configuration system integration
  - `error_recovery` - Error handling across components

### 3. Performance Tests

All test files include performance tests using `REGISTER_PERFORMANCE_TEST`:

- **Channel Assignment Performance**:
  - Large music analysis (1000+ notes)
  - Assignment algorithm speed
  - Strategy switching performance

- **Configuration Performance**:
  - JSON serialization speed
  - File I/O performance
  - Validation speed

- **Audio Performance**:
  - Write throughput testing
  - Format conversion speed

### 4. Stress Tests

Stress tests use `REGISTER_STRESS_TEST` with concurrent operations:

- **Concurrent Access**: Multiple threads accessing shared resources
- **Memory Usage**: Large dataset processing
- **Rapid Operations**: High-frequency operations
- **Resource Limits**: Operation under resource constraints

## Running Tests

### Basic Test Execution

```bash
# Build tests
cmake -B build && make -C build

# Run all unit tests
cd build && ctest

# Run specific test suite
./mame_synth_tests --test-suite=channel_assignment

# Run with verbose output
./mame_synth_tests --test-suite=all --verbose
```

### Comprehensive Test Runner

```bash
# Run unit tests only
./comprehensive_test_runner --unit

# Run performance tests
./comprehensive_test_runner --performance --perf-iterations 1000

# Run stress tests
./comprehensive_test_runner --stress --stress-threads 4 --stress-duration 30

# Run all tests with JSON output
./comprehensive_test_runner --all --json --output results.json
```

### Continuous Integration

The project includes GitHub Actions workflows:

```bash
# All CI tests run automatically on:
- Push to main/develop branches
- Pull requests to main
- Manual workflow dispatch
```

## Test Data Generation

### Random Music Generation
```cpp
// Generate test music data
music_data music = test_data_generator::generate_random_music(100, 1920);

// Generate audio samples
std::vector<float> samples = test_data_generator::generate_audio_samples(1024, 440.0f);

// Generate MIDI data
std::vector<uint8_t> midi = test_data_generator::generate_random_midi_data(512);
```

### Test Utilities
```cpp
// File operations
std::string temp_file = test_utilities::create_temp_file("content", ".txt");
bool files_equal = test_utilities::files_equal("file1.txt", "file2.txt");
test_utilities::cleanup_temp_file(temp_file);

// File size checking
size_t size = test_utilities::get_file_size("test.wav");
```

## Assertion Reference

### Basic Assertions
```cpp
ASSERT_TRUE(condition);
ASSERT_FALSE(condition);
ASSERT_EQ(expected, actual);
ASSERT_NE(not_expected, actual);
ASSERT_LT(left, right);
ASSERT_GT(left, right);
ASSERT_NEAR(expected, actual, tolerance);
ASSERT_THROW(expression, exception_type);
```

### Performance Assertions
```cpp
ASSERT_PERFORMANCE_LT(expression, max_time_ms);
ASSERT_NO_MEMORY_LEAKS(expression);
```

## Mock Objects

```cpp
// Create mock object
mock_object<audio_device> mock_device;

// Record method calls
mock_device.record_call("initialize", {"param1", "param2"});

// Verify calls
ASSERT_TRUE(mock_device.was_called("initialize"));
ASSERT_EQ(mock_device.call_count("initialize"), 1);
```

## Coverage Analysis

### Function Coverage
```cpp
// Mark functions as covered
test_coverage::mark_function_covered("nes_sequencer::load_music");
test_coverage::mark_line_covered("sequencer.cpp", 42);

// Generate coverage report
test_coverage::print_coverage_report();
```

### Coverage CI Integration
```bash
# Coverage is automatically collected in CI
# Results uploaded to Codecov
# Requires: gcov, lcov
```

## Performance Benchmarking

### Configuration
```cpp
performance_config config;
config.iterations = 1000;          // Number of test iterations
config.timeout_seconds = 10.0;     // Maximum test time
config.warmup_iterations = 100;    // Warmup before measurement
```

### Results Analysis
Performance tests provide detailed statistics:
- Total execution time
- Average, minimum, maximum times
- Standard deviation
- Memory usage (if enabled)
- CPU usage tracking

## Stress Testing

### Configuration
```cpp
stress_config config;
config.concurrent_threads = 4;        // Number of concurrent threads
config.operations_per_thread = 1000;  // Operations per thread
config.duration_seconds = 30.0;       // Maximum test duration
config.enable_random_delays = true;   // Add random delays
```

### Failure Analysis
Stress tests track:
- Total operations attempted
- Successful vs failed operations
- Error messages and types
- Operations per second
- Thread coordination issues

## Test Maintenance

### Adding New Tests

1. **Unit Tests**: Add to appropriate `test_*.cpp` file
2. **Integration Tests**: Add to `test_end_to_end_integration.cpp`
3. **Performance Tests**: Use `REGISTER_PERFORMANCE_TEST`
4. **Stress Tests**: Use `REGISTER_STRESS_TEST`

### Test File Structure
```cpp
#include "test_framework_enhanced.h"
#include "../src/component_to_test.h"

REGISTER_TEST(suite_name, test_name) {
    // Test implementation
    ASSERT_TRUE(condition);
}

REGISTER_PERFORMANCE_TEST(suite_name, perf_test_name) {
    // Performance test implementation
}

REGISTER_STRESS_TEST(suite_name, stress_test_name) {
    // Stress test implementation
}
```

### Best Practices

1. **Test Naming**: Use descriptive names indicating what is tested
2. **Test Independence**: Each test should be independent and isolated
3. **Data Cleanup**: Clean up temporary files and resources
4. **Error Messages**: Provide clear assertion messages
5. **Performance Baselines**: Document expected performance characteristics

## Debugging Tests

### Verbose Output
```bash
./mame_synth_tests --test-suite=channel_assignment --verbose
```

### Individual Test Execution
```bash
# Run single test suite
./mame_synth_tests --test-suite=nes_config

# Debug with GDB
gdb ./mame_synth_tests
(gdb) run --test-suite=audio_stream
```

### Test Results Analysis
```bash
# JSON output for detailed analysis
./comprehensive_test_runner --all --json --output detailed_results.json

# Parse results
cat detailed_results.json | jq '.test_results.unit_tests[] | select(.passed == false)'
```

## Integration with Development Workflow

### Pre-commit Testing
```bash
# Run quick test suite before commit
./comprehensive_test_runner --unit --suite=critical

# Performance regression check
./comprehensive_test_runner --performance --perf-iterations 100
```

### Release Testing
```bash
# Full test suite for releases
./comprehensive_test_runner --all --verbose

# Stress testing for stability
./comprehensive_test_runner --stress --stress-duration 300
```

This comprehensive testing framework ensures the NES synthesizer maintains high quality, performance, and reliability across all components and use cases.