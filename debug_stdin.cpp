#include <iostream>
#include <unistd.h>

int main() {
    std::cout << "isatty(STDIN_FILENO): " << isatty(STDIN_FILENO) << std::endl;
    std::cout << "STDIN_FILENO: " << STDIN_FILENO << std::endl;

    if (isatty(STDIN_FILENO)) {
        std::cout << "stdin is a terminal" << std::endl;
    } else {
        std::cout << "stdin is NOT a terminal (likely piped)" << std::endl;
    }

    return 0;
}