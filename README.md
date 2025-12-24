# DownPour - Rain Simulator

A Vulkan-based rain simulation application built with C++17.

## Project Structure

```
DownPour/
├── main.cpp                      # Application entry point
├── src/
│   ├── DownPour.h               # Main application class header
│   ├── DownPour.cpp             # Main application class implementation
│   └── vulkan/
│       └── VulkanTypes.h        # Vulkan-specific type definitions
├── headers/                      # Third-party dependencies (git submodules)
│   ├── glfw/                    # GLFW library
│   ├── glm/                     # GLM math library
│   └── glad/                    # Glad OpenGL loader
└── CMakeLists.txt               # CMake build configuration
```

## Code Organization

The codebase is organized into modular components for better readability and maintainability:

### Main Application (`src/DownPour.h` and `src/DownPour.cpp`)
- **Application class**: Manages the entire application lifecycle
  - Window creation and management (GLFW)
  - Vulkan initialization
  - Main rendering loop
  - Resource cleanup

### Vulkan Types (`src/vulkan/VulkanTypes.h`)
- **QueueFamilyIndices**: Helper struct for managing Vulkan queue families
- Centralizes Vulkan-specific type definitions for reusability

### Entry Point (`main.cpp`)
- Minimal entry point that creates and runs the application
- Exception handling for graceful error reporting

## Building the Project

### Prerequisites
- C++17 compatible compiler
- CMake 3.10 or higher
- Vulkan SDK
- Git (for submodules)

### Build Steps

```bash
# Clone the repository
git clone <repository-url>
cd DownPour

# Initialize and update git submodules
git submodule update --init --recursive

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the project
cmake --build .

# Run the application
./DownPour
```

## Platform Support

Currently optimized for macOS (M3 Mac) with Metal backend support via MoltenVK.

## Code Style Guidelines

- **Namespaces**: All application code is within the `DownPour` namespace
- **Documentation**: Classes and methods include Doxygen-style comments
- **Naming**: Clear, descriptive names following C++ conventions
- **Modularity**: Each component has a single, well-defined responsibility

## Future Enhancements

As the project grows, consider:
- Separating window management into its own module
- Creating a dedicated Vulkan renderer class
- Adding shader management utilities
- Implementing resource management systems
