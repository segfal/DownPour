#!/bin/bash

# Format all C++ files in the src directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
cd "$PROJECT_ROOT"
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i