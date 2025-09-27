#include "test_framework.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "MAME Synth Test Suite" << std::endl;
    std::cout << "=====================" << std::endl;

    // Parse command line arguments
    std::string suite_filter = "all";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--test-suite=") == 0) {
            suite_filter = arg.substr(13); // Skip "--test-suite="
        }
    }

    if (suite_filter != "all") {
        std::cout << "Running test suite: " << suite_filter << std::endl;
    } else {
        std::cout << "Running all test suites" << std::endl;
    }

    // Run tests
    auto& registry = test_registry::instance();
    auto results = registry.run_tests(suite_filter);

    // Print results
    registry.print_results(results);

    // Return appropriate exit code
    int total_failed = 0;
    for (const auto& suite : results) {
        total_failed += suite.failed_count;
    }

    if (total_failed > 0) {
        std::cout << "\nSome tests failed!" << std::endl;
        return 1;
    } else {
        std::cout << "\nAll tests passed!" << std::endl;
        return 0;
    }
}