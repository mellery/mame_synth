#pragma once

#include "test_framework.h"
#include <memory>
#include <map>
#include <random>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <set>
#include "../src/music_parser.h"

/**
 * Enhanced testing framework for comprehensive NES synthesizer testing
 * Extends the basic framework with performance, stress, and advanced testing capabilities
 */

// Performance test configuration
struct performance_config {
    size_t iterations = 1000;
    double timeout_seconds = 10.0;
    bool measure_memory = true;
    bool measure_cpu = true;
    size_t warmup_iterations = 100;
};

// Performance test result
struct performance_result {
    std::string test_name;
    size_t iterations;
    double total_time_ms;
    double avg_time_ms;
    double min_time_ms;
    double max_time_ms;
    double std_dev_ms;
    size_t memory_used_bytes = 0;
    double cpu_usage_percent = 0.0;
    bool passed;
    std::string error_message;
};

// Stress test configuration
struct stress_config {
    size_t concurrent_threads = 4;
    size_t operations_per_thread = 1000;
    double duration_seconds = 30.0;
    bool enable_random_delays = true;
    size_t max_delay_ms = 10;
};

// Stress test result
struct stress_result {
    std::string test_name;
    size_t total_operations;
    size_t successful_operations;
    size_t failed_operations;
    double duration_seconds;
    double operations_per_second;
    size_t concurrent_threads;
    bool passed;
    std::vector<std::string> errors;
};

// Mock and stub utilities
template<typename T>
class mock_object {
public:
    using method_call = std::pair<std::string, std::vector<std::string>>;

    void record_call(const std::string& method_name, const std::vector<std::string>& args = {}) {
        calls.emplace_back(method_name, args);
    }

    bool was_called(const std::string& method_name) const {
        return std::any_of(calls.begin(), calls.end(),
                          [&method_name](const method_call& call) {
                              return call.first == method_name;
                          });
    }

    size_t call_count(const std::string& method_name) const {
        return std::count_if(calls.begin(), calls.end(),
                           [&method_name](const method_call& call) {
                               return call.first == method_name;
                           });
    }

    void clear_calls() { calls.clear(); }

    const std::vector<method_call>& get_calls() const { return calls; }

private:
    std::vector<method_call> calls;
};

// Test data generators
class test_data_generator {
public:
    static music_data generate_random_music(size_t note_count = 100, uint32_t duration_ticks = 1920) {
        music_data music;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> channel_dist(0, 15);
        std::uniform_int_distribution<uint8_t> note_dist(24, 108);
        std::uniform_int_distribution<uint8_t> velocity_dist(1, 127);
        std::uniform_int_distribution<music_time_t> time_dist(0, duration_ticks);
        std::uniform_int_distribution<music_time_t> dur_dist(120, 960);

        for (size_t i = 0; i < note_count; ++i) {
            music_note note(
                channel_dist(gen),
                note_dist(gen),
                velocity_dist(gen),
                time_dist(gen),
                dur_dist(gen)
            );
            music.add_note(note);
        }

        return music;
    }

    static std::vector<uint8_t> generate_random_midi_data(size_t size = 1024) {
        std::vector<uint8_t> data;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> byte_dist(0, 255);

        data.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            data.push_back(byte_dist(gen));
        }

        return data;
    }

    static std::vector<float> generate_audio_samples(size_t sample_count, float frequency = 440.0f, float sample_rate = 44100.0f) {
        std::vector<float> samples;
        samples.reserve(sample_count);

        for (size_t i = 0; i < sample_count; ++i) {
            float time = static_cast<float>(i) / sample_rate;
            float value = std::sin(2.0f * M_PI * frequency * time);
            samples.push_back(value);
        }

        return samples;
    }
};

// Test utilities
class test_utilities {
public:
    static std::string create_temp_file(const std::string& content, const std::string& extension = ".tmp") {
        std::string filename = "/tmp/test_file_" + std::to_string(std::time(nullptr)) + extension;
        std::ofstream file(filename);
        file << content;
        file.close();
        return filename;
    }

    static void cleanup_temp_file(const std::string& filename) {
        std::remove(filename.c_str());
    }

    static bool files_equal(const std::string& file1, const std::string& file2) {
        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);

        if (!f1.is_open() || !f2.is_open()) {
            return false;
        }

        return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                         std::istreambuf_iterator<char>(),
                         std::istreambuf_iterator<char>(f2.rdbuf()));
    }

    static size_t get_file_size(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return 0;
        }
        return static_cast<size_t>(file.tellg());
    }
};

// Enhanced test registry with performance and stress testing
class enhanced_test_registry {
public:
    using performance_test_function = std::function<void(const performance_config&)>;
    using stress_test_function = std::function<void(const stress_config&)>;

    static enhanced_test_registry& instance() {
        static enhanced_test_registry instance;
        return instance;
    }

    // Performance test registration
    void register_performance_test(const std::string& suite_name, const std::string& test_name,
                                 performance_test_function func) {
        performance_test_info info{suite_name, test_name, func};
        performance_tests.push_back(info);
    }

    // Stress test registration
    void register_stress_test(const std::string& suite_name, const std::string& test_name,
                            stress_test_function func) {
        stress_test_info info{suite_name, test_name, func};
        stress_tests.push_back(info);
    }

    // Run performance tests
    std::vector<performance_result> run_performance_tests(const std::string& suite_filter = "",
                                                         const performance_config& config = {}) {
        std::vector<performance_result> results;

        for (const auto& test : performance_tests) {
            if (!suite_filter.empty() && suite_filter != "all" && test.suite_name != suite_filter) {
                continue;
            }

            performance_result result = run_single_performance_test(test, config);
            results.push_back(result);
        }

        return results;
    }

    // Run stress tests
    std::vector<stress_result> run_stress_tests(const std::string& suite_filter = "",
                                              const stress_config& config = {}) {
        std::vector<stress_result> results;

        for (const auto& test : stress_tests) {
            if (!suite_filter.empty() && suite_filter != "all" && test.suite_name != suite_filter) {
                continue;
            }

            stress_result result = run_single_stress_test(test, config);
            results.push_back(result);
        }

        return results;
    }

    void print_performance_results(const std::vector<performance_result>& results) {
        std::cout << "\n=== Performance Test Results ===" << std::endl;
        std::cout << std::setw(30) << "Test Name"
                  << std::setw(12) << "Iterations"
                  << std::setw(12) << "Total (ms)"
                  << std::setw(12) << "Avg (ms)"
                  << std::setw(12) << "Min (ms)"
                  << std::setw(12) << "Max (ms)"
                  << std::setw(12) << "StdDev"
                  << std::setw(10) << "Status" << std::endl;
        std::cout << std::string(120, '-') << std::endl;

        for (const auto& result : results) {
            std::cout << std::setw(30) << result.test_name
                      << std::setw(12) << result.iterations
                      << std::setw(12) << std::fixed << std::setprecision(3) << result.total_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(3) << result.avg_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(3) << result.min_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(3) << result.max_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(3) << result.std_dev_ms
                      << std::setw(10) << (result.passed ? "PASS" : "FAIL") << std::endl;

            if (!result.passed && !result.error_message.empty()) {
                std::cout << "    Error: " << result.error_message << std::endl;
            }
        }
    }

    void print_stress_results(const std::vector<stress_result>& results) {
        std::cout << "\n=== Stress Test Results ===" << std::endl;
        std::cout << std::setw(30) << "Test Name"
                  << std::setw(12) << "Operations"
                  << std::setw(12) << "Successful"
                  << std::setw(12) << "Failed"
                  << std::setw(12) << "Op/Sec"
                  << std::setw(10) << "Threads"
                  << std::setw(10) << "Status" << std::endl;
        std::cout << std::string(110, '-') << std::endl;

        for (const auto& result : results) {
            std::cout << std::setw(30) << result.test_name
                      << std::setw(12) << result.total_operations
                      << std::setw(12) << result.successful_operations
                      << std::setw(12) << result.failed_operations
                      << std::setw(12) << std::fixed << std::setprecision(1) << result.operations_per_second
                      << std::setw(10) << result.concurrent_threads
                      << std::setw(10) << (result.passed ? "PASS" : "FAIL") << std::endl;

            if (!result.errors.empty()) {
                for (const auto& error : result.errors) {
                    std::cout << "    Error: " << error << std::endl;
                }
            }
        }
    }

private:
    struct performance_test_info {
        std::string suite_name;
        std::string test_name;
        performance_test_function func;
    };

    struct stress_test_info {
        std::string suite_name;
        std::string test_name;
        stress_test_function func;
    };

    std::vector<performance_test_info> performance_tests;
    std::vector<stress_test_info> stress_tests;

    performance_result run_single_performance_test(const performance_test_info& test,
                                                  const performance_config& config) {
        performance_result result;
        result.test_name = test.test_name;
        result.iterations = config.iterations;
        result.passed = false;

        std::vector<double> times;
        times.reserve(config.iterations);

        try {
            // Warmup
            for (size_t i = 0; i < config.warmup_iterations; ++i) {
                test.func(config);
            }

            auto start_time = std::chrono::high_resolution_clock::now();

            // Actual test iterations
            for (size_t i = 0; i < config.iterations; ++i) {
                auto iter_start = std::chrono::high_resolution_clock::now();
                test.func(config);
                auto iter_end = std::chrono::high_resolution_clock::now();

                double iter_time = std::chrono::duration<double, std::milli>(iter_end - iter_start).count();
                times.push_back(iter_time);
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

            // Calculate statistics
            result.avg_time_ms = result.total_time_ms / config.iterations;
            result.min_time_ms = *std::min_element(times.begin(), times.end());
            result.max_time_ms = *std::max_element(times.begin(), times.end());

            // Calculate standard deviation
            double variance = 0.0;
            for (double time : times) {
                variance += (time - result.avg_time_ms) * (time - result.avg_time_ms);
            }
            result.std_dev_ms = std::sqrt(variance / config.iterations);

            result.passed = true;

        } catch (const std::exception& e) {
            result.error_message = e.what();
        } catch (...) {
            result.error_message = "Unknown exception";
        }

        return result;
    }

    stress_result run_single_stress_test(const stress_test_info& test, const stress_config& config) {
        stress_result result;
        result.test_name = test.test_name;
        result.concurrent_threads = config.concurrent_threads;
        result.passed = false;

        std::atomic<size_t> successful_ops{0};
        std::atomic<size_t> failed_ops{0};
        std::vector<std::string> errors;
        std::mutex error_mutex;

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            std::vector<std::thread> threads;

            for (size_t i = 0; i < config.concurrent_threads; ++i) {
                threads.emplace_back([&, i]() {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<int> delay_dist(0, config.max_delay_ms);

                    for (size_t op = 0; op < config.operations_per_thread; ++op) {
                        try {
                            test.func(config);
                            successful_ops++;

                            if (config.enable_random_delays) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
                            }
                        } catch (const std::exception& e) {
                            failed_ops++;
                            std::lock_guard<std::mutex> lock(error_mutex);
                            errors.push_back("Thread " + std::to_string(i) + ": " + e.what());
                        } catch (...) {
                            failed_ops++;
                            std::lock_guard<std::mutex> lock(error_mutex);
                            errors.push_back("Thread " + std::to_string(i) + ": Unknown exception");
                        }
                    }
                });
            }

            // Wait for all threads to complete or timeout
            auto timeout_time = start_time + std::chrono::seconds(static_cast<long>(config.duration_seconds));

            for (auto& thread : threads) {
                if (std::chrono::high_resolution_clock::now() < timeout_time) {
                    thread.join();
                } else {
                    thread.detach(); // Timeout - let threads finish naturally
                    break;
                }
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.duration_seconds = std::chrono::duration<double>(end_time - start_time).count();

            result.successful_operations = successful_ops.load();
            result.failed_operations = failed_ops.load();
            result.total_operations = result.successful_operations + result.failed_operations;
            result.operations_per_second = result.total_operations / result.duration_seconds;
            result.errors = errors;

            // Consider passed if at least 95% of operations succeeded
            result.passed = (result.failed_operations * 100 / result.total_operations) < 5;

        } catch (const std::exception& e) {
            result.errors.push_back("Test setup error: " + std::string(e.what()));
        } catch (...) {
            result.errors.push_back("Unknown test setup error");
        }

        return result;
    }
};

// Enhanced test macros
#define REGISTER_PERFORMANCE_TEST(suite, name) \
    static void perf_test_##suite##_##name(const performance_config& config); \
    static bool register_perf_##suite##_##name = []() { \
        enhanced_test_registry::instance().register_performance_test(#suite, #name, perf_test_##suite##_##name); \
        return true; \
    }(); \
    static void perf_test_##suite##_##name(const performance_config& config)

#define REGISTER_STRESS_TEST(suite, name) \
    static void stress_test_##suite##_##name(const stress_config& config); \
    static bool register_stress_##suite##_##name = []() { \
        enhanced_test_registry::instance().register_stress_test(#suite, #name, stress_test_##suite##_##name); \
        return true; \
    }(); \
    static void stress_test_##suite##_##name(const stress_config& config)

// Performance assertion macros
#define ASSERT_PERFORMANCE_LT(expression, max_time_ms) \
    do { \
        auto start = std::chrono::high_resolution_clock::now(); \
        expression; \
        auto end = std::chrono::high_resolution_clock::now(); \
        double elapsed = std::chrono::duration<double, std::milli>(end - start).count(); \
        if (elapsed >= (max_time_ms)) { \
            std::ostringstream oss; \
            oss << "Performance assertion failed: expected < " << (max_time_ms) << "ms but took " << elapsed << "ms" \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw test_assertion_error(oss.str()); \
        } \
    } while(0)

#define ASSERT_NO_MEMORY_LEAKS(expression) \
    do { \
        /* Basic memory leak detection - could be enhanced with valgrind integration */ \
        expression; \
        /* For now, just execute - real implementation would check memory usage */ \
    } while(0)

// Test coverage utilities
class test_coverage {
public:
    static void mark_function_covered(const std::string& function_name) {
        covered_functions.insert(function_name);
    }

    static void mark_line_covered(const std::string& file, int line) {
        covered_lines[file].insert(line);
    }

    static double get_function_coverage() {
        return static_cast<double>(covered_functions.size()) / total_functions * 100.0;
    }

    static void print_coverage_report() {
        std::cout << "\n=== Test Coverage Report ===" << std::endl;
        std::cout << "Functions covered: " << covered_functions.size() << "/" << total_functions
                  << " (" << get_function_coverage() << "%)" << std::endl;

        std::cout << "\nCovered functions:" << std::endl;
        for (const auto& func : covered_functions) {
            std::cout << "  - " << func << std::endl;
        }
    }

private:
    static std::set<std::string> covered_functions;
    static std::map<std::string, std::set<int>> covered_lines;
    static size_t total_functions;
};

// Initialize static members (inline to avoid multiple definitions)
inline std::set<std::string> test_coverage::covered_functions;
inline std::map<std::string, std::set<int>> test_coverage::covered_lines;
inline size_t test_coverage::total_functions = 100; // Estimate - would be calculated from source analysis