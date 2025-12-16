#!/bin/bash

# DownPour Clean Build and Run Script
# This script performs a clean build from scratch

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== DownPour Clean Build and Run ===${NC}"

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"

# Remove existing build directory
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Removing old build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create fresh build directory
echo -e "${YELLOW}Creating fresh build directory...${NC}"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Running CMake configuration...${NC}"
cmake ..

# Build the project
echo -e "${YELLOW}Building project...${NC}"
cmake --build .

# Check if build was successful
if [ $? -eq 0 ]; then
    echo -e "${GREEN}Clean build successful!${NC}"
    echo -e "${GREEN}==================================${NC}"
    echo -e "${YELLOW}Running DownPour...${NC}"
    echo -e "${GREEN}==================================${NC}\n"

    # Run the executable from the project root (where assets are)
    cd "$PROJECT_ROOT"
    "$BUILD_DIR/DownPour"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
