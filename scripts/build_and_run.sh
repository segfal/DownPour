#!/bin/bash

# DownPour Build and Run Script
# This script builds the project and runs the application

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== DownPour Build and Run ===${NC}"

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p "$BUILD_DIR"
fi

# Navigate to build directory
cd "$BUILD_DIR"

# Configure with CMake (only if needed)
if [ ! -f "CMakeCache.txt" ]; then
    echo -e "${YELLOW}Running CMake configuration...${NC}"
    cmake ..
else
    echo -e "${GREEN}CMake already configured${NC}"
fi

# Build the project
echo -e "${YELLOW}Building project...${NC}"
if cmake --build .; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${BLUE}==================================${NC}"
    echo -e "${YELLOW}Running DownPour...${NC}"
    echo -e "${BLUE}==================================${NC}\n"

    # Run the executable from the project root (where assets are)
    cd "$PROJECT_ROOT"
    "$BUILD_DIR/DownPour" "$@"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
