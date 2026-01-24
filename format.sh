#!/bin/bash

# Format all C++ files in the src directory
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i