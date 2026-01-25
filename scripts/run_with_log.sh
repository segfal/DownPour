#!/bin/bash

# DownPour Build, Run and Log Script
# This script builds, runs, and saves console output to a file

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== DownPour Build, Run and Log ===${NC}"

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"
LOG_FILE="$PROJECT_ROOT/downpour_output.log"

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
    echo -e "${YELLOW}Output will be saved to: $LOG_FILE${NC}"
    echo -e "${GREEN}==================================${NC}\n"

    # Run the executable and save output to log file
    ./DownPour 2>&1 | tee "$LOG_FILE"

    echo -e "\n${GREEN}Output saved to: $LOG_FILE${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
