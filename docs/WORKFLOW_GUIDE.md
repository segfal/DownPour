# DownPour Workflow Guide

This guide explains all the different ways to build, run, and work with the DownPour project.

## Quick Start

The fastest way to get DownPour running:

```bash
# Clone and setup
git clone https://github.com/segfal/DownPour.git
cd DownPour
git submodule update --init --recursive

# Build and run (choose one)
./run.sh        # Option 1: Direct script
make run        # Option 2: Using Make
```

Both methods automatically:
1. Create the build directory
2. Configure CMake
3. Build the project
4. Run the application

## Build Methods

### Method 1: Root Script (Recommended for Quick Start)

```bash
./run.sh [arguments]
```

**What it does:**
- Creates `build/` directory if needed
- Runs CMake configuration (only if not already configured)
- Builds the project
- Runs the application
- Passes any arguments to the application

**When to use:**
- First time setup
- Quick iteration during development
- When you want a single command workflow

### Method 2: Make Targets (Recommended for Development)

```bash
make run        # Build and run
make run-only   # Run without building (faster if no changes)
make build      # Build only, don't run
make clean      # Clean build from scratch, then run
make run-log    # Build, run, and save output to downpour_output.log
```

**When to use:**
- `make run` - Standard development workflow
- `make run-only` - Testing without code changes
- `make build` - Checking for compile errors
- `make clean` - After changing CMake configuration or dependencies
- `make run-log` - Debugging issues or capturing output

### Method 3: Manual CMake (Advanced)

```bash
# First time setup
mkdir build
cd build
cmake ..
cmake --build .

# Subsequent builds
cd build
cmake --build .

# Run from project root
cd ..
./build/DownPour
```

**When to use:**
- Custom CMake configuration
- IDE integration
- Understanding the build process
- Debugging build issues

## Scripts Reference

### Root Level Scripts

**`./run.sh`**
- Full build and run workflow
- Creates build directory
- Configures CMake if needed
- Builds project
- Runs application
- Color-coded output

### Scripts Directory

**`scripts/build_and_run.sh`**
- Same as root `run.sh` but from scripts directory
- Used by `make run`

**`scripts/run.sh`**
- Run only (no build)
- Expects application to be already built
- Used by `make run-only`
- Lightweight execution script

**`scripts/run_clean.sh`**
- Full clean build
- Removes build directory
- Fresh CMake configuration
- Complete rebuild
- Used by `make clean`

**`scripts/run_with_log.sh`**
- Build and run with output logging
- Saves console output to `downpour_output.log`
- Useful for debugging
- Used by `make run-log`

**`scripts/format.sh`**
- Format C++ code with clang-format
- Used by `make format`

## Common Workflows

### First Time Setup

```bash
git clone https://github.com/segfal/DownPour.git
cd DownPour
git submodule update --init --recursive
./run.sh
```

### Daily Development

```bash
# Make code changes...

# Build and test
make run

# If only testing (no code changes)
make run-only

# View build output
make build
```

### Clean Rebuild

Use when:
- CMakeLists.txt changed
- Dependencies updated
- Strange build errors
- Switching branches

```bash
make clean
```

### Debugging Build Issues

```bash
# Capture full output
make run-log

# View the log
cat downpour_output.log

# Or manual verbose build
cd build
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON
cmake --build . --verbose
```

### Code Formatting

```bash
make format
```

## Build Outputs

### Build Directory Structure

```
build/
├── DownPour              # Main executable
├── CMakeCache.txt        # CMake configuration
├── CMakeFiles/           # CMake build files
├── compile_commands.json # For IDE/tools
└── ...                   # Additional build artifacts
```

### Assets Location

The application expects assets to be in the project root:

```
DownPour/
├── assets/
│   └── models/
│       ├── bmw/
│       └── road/
├── shaders/
└── build/
    └── DownPour  # Executable runs from here
```

**Important:** Always run `./build/DownPour` from the project root directory so assets can be found.

## Troubleshooting

### "No such file or directory: build/DownPour"

**Problem:** Application not built yet

**Solution:**
```bash
# Build first
make build
# Then run
make run-only

# Or use combined command
make run
```

### "Failed to find wayland-scanner" or Similar CMake Errors

**Problem:** Missing system dependencies

**Solution:**
```bash
# Ubuntu/Debian
sudo apt-get install libwayland-dev wayland-protocols libxkbcommon-dev

# macOS (usually works out of box with Vulkan SDK)
brew install vulkan-sdk

# Check Vulkan installation
vulkaninfo
```

### "Cannot open shared object file"

**Problem:** Missing Vulkan runtime

**Solution:**
```bash
# Ensure Vulkan SDK is installed and sourced
source /path/to/vulkansdk/setup-env.sh

# Or add to .bashrc/.zshrc
echo 'source /path/to/vulkansdk/setup-env.sh' >> ~/.bashrc
```

### Build Succeeds but Application Crashes

**Problem:** Asset files not found

**Solution:**
```bash
# Always run from project root
cd /path/to/DownPour
./build/DownPour

# Not from build directory:
# cd build && ./DownPour  # ❌ Wrong - assets not found
```

### Submodules Not Initialized

**Problem:** Missing third-party dependencies (GLFW, GLM, etc.)

**Solution:**
```bash
git submodule update --init --recursive
```

## Development Tools

DownPour includes additional development tools:

```bash
# Build all tools
make tools

# Build specific tools
make tools-monitor     # System monitor (C)
make tools-editor      # Scene editor (Rust)
make tools-converter   # GLTF converter (Python)

# Run tools
make run-monitor
make run-editor
make run-converter  # Shows usage

# Clean tools
make tools-clean
```

## Advanced: Custom Build Options

### Debug Build

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
```

### Release Build

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Specify Compiler

```bash
cd build
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..
cmake --build .
```

## Summary: Which Method to Use?

| Scenario | Recommended Method | Command |
|----------|-------------------|---------|
| First time setup | Root script | `./run.sh` |
| Daily development | Make | `make run` |
| Quick test (no changes) | Make | `make run-only` |
| After CMake changes | Make | `make clean` |
| Debugging output | Make | `make run-log` |
| IDE integration | Manual CMake | `cmake ..` |
| Custom configuration | Manual CMake | `cmake -D... ..` |

## See Also

- [README.md](../README.md) - Project overview and quick start
- [ARCHITECTURE.md](../ARCHITECTURE.md) - System architecture
- [CODING_STANDARDS.md](../CODING_STANDARDS.md) - Code style guidelines
- [Makefile](../Makefile) - All available make targets
