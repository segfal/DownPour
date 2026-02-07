#!/bin/bash

# DownPour - Simple Build and Run Script
# Minimal refactored version

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}=== DownPour Build and Run ===${NC}"

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p build
fi

# Build the project
echo -e "${YELLOW}Building project...${NC}"
cd build
cmake .. && cmake --build .

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

cd ..

echo -e "${GREEN}Build successful!${NC}"
echo -e "${CYAN}==================================${NC}"
echo -e "${YELLOW}Running DownPour...${NC}"
echo -e "${CYAN}==================================${NC}"
echo ""

# Run the application
./build/DownPour
