#!/bin/bash

# DownPour Build and Run Script
# This script builds and runs the DownPour application

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== DownPour Build and Run ===${NC}"

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="$SCRIPT_DIR/build"

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
cmake --build .

# Check if build was successful
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Build successful!${NC}"
    echo -e "${GREEN}==================================${NC}"
    echo -e "${YELLOW}Running DownPour...${NC}"
    echo -e "${GREEN}==================================${NC}\n"

    # Run the executable
    ./DownPour
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
