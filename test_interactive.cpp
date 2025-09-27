#include <iostream>
#include <string>

int main() {
    std::cout << "Testing basic interactive input..." << std::endl;
    std::cout << "Type 'quit' to exit" << std::endl;

    std::string line;
    while (true) {
        std::cout << "test> ";
        std::cout.flush(); // Force output

        if (!std::getline(std::cin, line)) {
            std::cout << "\nInput stream ended" << std::endl;
            break;
        }

        std::cout << "You entered: '" << line << "'" << std::endl;

        if (line == "quit" || line == "exit") {
            break;
        }

        if (line.empty()) {
            continue;
        }
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}