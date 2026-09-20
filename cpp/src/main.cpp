#include "harness/cli.hpp"

#include <iostream>

int main(int argc, char** argv) {
    try {
        return run_cli(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
