/**
 * @file main.cpp
 * @brief Entry point for the DownPour rain simulator application
 * 
 * This file contains only the main function which creates and runs
 * the DownPour application instance.
 */

#include "src/DownPour.h"

#include <iostream>
#include <exception>

int main() {
    DownPour::Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

