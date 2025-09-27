#include "test_framework_enhanced.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <cstring>

/**
 * Comprehensive test runner for the NES synthesizer project
 * Supports unit tests, performance tests, stress tests, and coverage analysis
 */

enum class test_mode {
    UNIT_TESTS,
    PERFORMANCE_TESTS,
    STRESS_TESTS,
    ALL_TESTS,
    COVERAGE_ANALYSIS
};

struct test_options {
    test_mode mode = test_mode::UNIT_TESTS;
    std::string suite_filter = "all";
    bool verbose = false;
    bool json_output = false;
    std::string output_file;
    size_t performance_iterations = 1000;
    size_t stress_threads = 4;
    double stress_duration = 30.0;
    bool enable_coverage = false;
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --unit                   Run unit tests (default)\n";
    std::cout << "  --performance            Run performance tests\n";
    std::cout << "  --stress                 Run stress tests\n";
    std::cout << "  --all                    Run all test types\n";
    std::cout << "  --coverage               Enable coverage analysis\n";
    std::cout << "  --suite <name>           Run specific test suite (default: all)\n";
    std::cout << "  --verbose                Enable verbose output\n";
    std::cout << "  --json                   Output results in JSON format\n";
    std::cout << "  --output <file>          Write results to file\n";
    std::cout << "  --perf-iterations <n>    Performance test iterations (default: 1000)\n";
    std::cout << "  --stress-threads <n>     Stress test thread count (default: 4)\n";
    std::cout << "  --stress-duration <s>    Stress test duration in seconds (default: 30)\n";
    std::cout << "  --help                   Show this help message\n\n";
    std::cout << "Available test suites:\n";
    std::cout << "  channel_assignment       Channel assignment algorithm tests\n";
    std::cout << "  nes_config                NES configuration system tests\n";
    std::cout << "  music_parser              Music parsing tests\n";
    std::cout << "  audio_device              Audio device tests\n";
    std::cout << "  nes_integration           NES integration tests\n";
    std::cout << "  all                       All test suites\n";
}

test_options parse_arguments(int argc, char* argv[]) {
    test_options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--unit") {
            options.mode = test_mode::UNIT_TESTS;
        } else if (arg == "--performance") {
            options.mode = test_mode::PERFORMANCE_TESTS;
        } else if (arg == "--stress") {
            options.mode = test_mode::STRESS_TESTS;
        } else if (arg == "--all") {
            options.mode = test_mode::ALL_TESTS;
        } else if (arg == "--coverage") {
            options.enable_coverage = true;
        } else if (arg == "--suite" && i + 1 < argc) {
            options.suite_filter = argv[++i];
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else if (arg == "--json") {
            options.json_output = true;
        } else if (arg == "--output" && i + 1 < argc) {
            options.output_file = argv[++i];
        } else if (arg == "--perf-iterations" && i + 1 < argc) {
            options.performance_iterations = std::stoul(argv[++i]);
        } else if (arg == "--stress-threads" && i + 1 < argc) {
            options.stress_threads = std::stoul(argv[++i]);
        } else if (arg == "--stress-duration" && i + 1 < argc) {
            options.stress_duration = std::stod(argv[++i]);
        } else if (arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            exit(1);
        }
    }

    return options;
}

void print_test_summary(const std::vector<test_suite>& suites,
                       const std::vector<performance_result>& perf_results,
                       const std::vector<stress_result>& stress_results,
                       const test_options& options) {
    auto total_duration = std::chrono::steady_clock::now();

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "COMPREHENSIVE TEST SUMMARY" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    // Unit test summary
    if (!suites.empty()) {
        int total_unit_tests = 0;
        int total_unit_passed = 0;
        double total_unit_time = 0.0;

        for (const auto& suite : suites) {
            total_unit_tests += suite.passed_count + suite.failed_count;
            total_unit_passed += suite.passed_count;

            for (const auto& result : suite.results) {
                total_unit_time += result.duration_ms;
            }
        }

        std::cout << "\nUnit Tests:\n";
        std::cout << "  Total Tests: " << total_unit_tests << std::endl;
        std::cout << "  Passed: " << total_unit_passed << std::endl;
        std::cout << "  Failed: " << (total_unit_tests - total_unit_passed) << std::endl;
        std::cout << "  Success Rate: " << std::fixed << std::setprecision(1)
                  << (total_unit_tests > 0 ? (100.0 * total_unit_passed / total_unit_tests) : 0)
                  << "%" << std::endl;
        std::cout << "  Total Time: " << std::fixed << std::setprecision(3)
                  << total_unit_time << "ms" << std::endl;
    }

    // Performance test summary
    if (!perf_results.empty()) {
        int perf_passed = 0;
        double total_perf_time = 0.0;

        for (const auto& result : perf_results) {
            if (result.passed) perf_passed++;
            total_perf_time += result.total_time_ms;
        }

        std::cout << "\nPerformance Tests:\n";
        std::cout << "  Total Tests: " << perf_results.size() << std::endl;
        std::cout << "  Passed: " << perf_passed << std::endl;
        std::cout << "  Failed: " << (perf_results.size() - perf_passed) << std::endl;
        std::cout << "  Total Iterations: " << options.performance_iterations * perf_results.size() << std::endl;
        std::cout << "  Total Time: " << std::fixed << std::setprecision(3)
                  << total_perf_time << "ms" << std::endl;
    }

    // Stress test summary
    if (!stress_results.empty()) {
        int stress_passed = 0;
        size_t total_operations = 0;
        size_t total_successful = 0;

        for (const auto& result : stress_results) {
            if (result.passed) stress_passed++;
            total_operations += result.total_operations;
            total_successful += result.successful_operations;
        }

        std::cout << "\nStress Tests:\n";
        std::cout << "  Total Tests: " << stress_results.size() << std::endl;
        std::cout << "  Passed: " << stress_passed << std::endl;
        std::cout << "  Failed: " << (stress_results.size() - stress_passed) << std::endl;
        std::cout << "  Total Operations: " << total_operations << std::endl;
        std::cout << "  Successful Operations: " << total_successful << std::endl;
        std::cout << "  Operation Success Rate: " << std::fixed << std::setprecision(1)
                  << (total_operations > 0 ? (100.0 * total_successful / total_operations) : 0)
                  << "%" << std::endl;
    }

    // Overall status
    bool all_passed = true;
    for (const auto& suite : suites) {
        if (suite.failed_count > 0) {
            all_passed = false;
            break;
        }
    }
    for (const auto& result : perf_results) {
        if (!result.passed) {
            all_passed = false;
            break;
        }
    }
    for (const auto& result : stress_results) {
        if (!result.passed) {
            all_passed = false;
            break;
        }
    }

    std::cout << "\n" << std::string(80, '-') << std::endl;
    std::cout << "OVERALL STATUS: " << (all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

void output_json_results(const std::vector<test_suite>& suites,
                        const std::vector<performance_result>& perf_results,
                        const std::vector<stress_result>& stress_results,
                        const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << filename << std::endl;
        return;
    }

    file << "{\n";
    file << "  \"test_results\": {\n";
    file << "    \"timestamp\": \"" << std::time(nullptr) << "\",\n";

    // Unit tests
    file << "    \"unit_tests\": [\n";
    bool first_suite = true;
    for (const auto& suite : suites) {
        if (!first_suite) file << ",\n";
        first_suite = false;

        file << "      {\n";
        file << "        \"suite\": \"" << suite.name << "\",\n";
        file << "        \"passed\": " << suite.passed_count << ",\n";
        file << "        \"failed\": " << suite.failed_count << ",\n";
        file << "        \"tests\": [\n";

        bool first_test = true;
        for (const auto& result : suite.results) {
            if (!first_test) file << ",\n";
            first_test = false;

            file << "          {\n";
            file << "            \"name\": \"" << result.name << "\",\n";
            file << "            \"passed\": " << (result.passed ? "true" : "false") << ",\n";
            file << "            \"duration_ms\": " << result.duration_ms << ",\n";
            file << "            \"message\": \"" << result.message << "\"\n";
            file << "          }";
        }
        file << "\n        ]\n";
        file << "      }";
    }
    file << "\n    ],\n";

    // Performance tests
    file << "    \"performance_tests\": [\n";
    bool first_perf = true;
    for (const auto& result : perf_results) {
        if (!first_perf) file << ",\n";
        first_perf = false;

        file << "      {\n";
        file << "        \"name\": \"" << result.test_name << "\",\n";
        file << "        \"passed\": " << (result.passed ? "true" : "false") << ",\n";
        file << "        \"iterations\": " << result.iterations << ",\n";
        file << "        \"total_time_ms\": " << result.total_time_ms << ",\n";
        file << "        \"avg_time_ms\": " << result.avg_time_ms << ",\n";
        file << "        \"min_time_ms\": " << result.min_time_ms << ",\n";
        file << "        \"max_time_ms\": " << result.max_time_ms << ",\n";
        file << "        \"std_dev_ms\": " << result.std_dev_ms << "\n";
        file << "      }";
    }
    file << "\n    ],\n";

    // Stress tests
    file << "    \"stress_tests\": [\n";
    bool first_stress = true;
    for (const auto& result : stress_results) {
        if (!first_stress) file << ",\n";
        first_stress = false;

        file << "      {\n";
        file << "        \"name\": \"" << result.test_name << "\",\n";
        file << "        \"passed\": " << (result.passed ? "true" : "false") << ",\n";
        file << "        \"total_operations\": " << result.total_operations << ",\n";
        file << "        \"successful_operations\": " << result.successful_operations << ",\n";
        file << "        \"failed_operations\": " << result.failed_operations << ",\n";
        file << "        \"operations_per_second\": " << result.operations_per_second << ",\n";
        file << "        \"concurrent_threads\": " << result.concurrent_threads << "\n";
        file << "      }";
    }
    file << "\n    ]\n";

    file << "  }\n";
    file << "}\n";

    file.close();
    std::cout << "Results written to: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    test_options options = parse_arguments(argc, argv);

    std::cout << "NES Synthesizer Comprehensive Test Suite" << std::endl;
    std::cout << "=========================================" << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    std::vector<test_suite> unit_suites;
    std::vector<performance_result> perf_results;
    std::vector<stress_result> stress_results;

    // Run unit tests
    if (options.mode == test_mode::UNIT_TESTS || options.mode == test_mode::ALL_TESTS) {
        std::cout << "\nRunning unit tests..." << std::endl;
        unit_suites = test_registry::instance().run_tests(options.suite_filter);

        if (options.verbose) {
            test_registry::instance().print_results(unit_suites);
        } else {
            // Just print summary
            int total_passed = 0, total_failed = 0;
            for (const auto& suite : unit_suites) {
                total_passed += suite.passed_count;
                total_failed += suite.failed_count;
            }
            std::cout << "Unit tests completed: " << total_passed << " passed, " << total_failed << " failed" << std::endl;
        }
    }

    // Run performance tests
    if (options.mode == test_mode::PERFORMANCE_TESTS || options.mode == test_mode::ALL_TESTS) {
        std::cout << "\nRunning performance tests..." << std::endl;
        performance_config perf_config;
        perf_config.iterations = options.performance_iterations;

        perf_results = enhanced_test_registry::instance().run_performance_tests(options.suite_filter, perf_config);

        if (options.verbose) {
            enhanced_test_registry::instance().print_performance_results(perf_results);
        } else {
            std::cout << "Performance tests completed: " << perf_results.size() << " tests" << std::endl;
        }
    }

    // Run stress tests
    if (options.mode == test_mode::STRESS_TESTS || options.mode == test_mode::ALL_TESTS) {
        std::cout << "\nRunning stress tests..." << std::endl;
        stress_config stress_config;
        stress_config.concurrent_threads = options.stress_threads;
        stress_config.duration_seconds = options.stress_duration;

        stress_results = enhanced_test_registry::instance().run_stress_tests(options.suite_filter, stress_config);

        if (options.verbose) {
            enhanced_test_registry::instance().print_stress_results(stress_results);
        } else {
            std::cout << "Stress tests completed: " << stress_results.size() << " tests" << std::endl;
        }
    }

    // Coverage analysis
    if (options.enable_coverage) {
        std::cout << "\nGenerating coverage report..." << std::endl;
        test_coverage::print_coverage_report();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration<double>(end_time - start_time).count();

    // Print comprehensive summary
    print_test_summary(unit_suites, perf_results, stress_results, options);

    std::cout << "\nTotal execution time: " << std::fixed << std::setprecision(3)
              << total_duration << " seconds" << std::endl;

    // Output to JSON file if requested
    if (options.json_output && !options.output_file.empty()) {
        output_json_results(unit_suites, perf_results, stress_results, options.output_file);
    }

    // Return appropriate exit code
    bool all_passed = true;
    for (const auto& suite : unit_suites) {
        if (suite.failed_count > 0) {
            all_passed = false;
            break;
        }
    }
    for (const auto& result : perf_results) {
        if (!result.passed) {
            all_passed = false;
            break;
        }
    }
    for (const auto& result : stress_results) {
        if (!result.passed) {
            all_passed = false;
            break;
        }
    }

    return all_passed ? 0 : 1;
}