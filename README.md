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

This project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). For detailed coding standards, see [CODING_STANDARDS.md](CODING_STANDARDS.md).

**Quick Reference:**
- **Namespaces**: All application code is within the `DownPour` namespace
- **Naming**: PascalCase for classes, camelCase for functions/variables
- **Documentation**: Doxygen-style comments for all public APIs
- **Formatting**: 4 spaces indentation, K&R brace style
- **Modularity**: Each component has a single, well-defined responsibility
- **Modern C++**: Use C++17 features (auto, nullptr, constexpr, etc.)

## Future Enhancements

As the project grows, consider:
- Separating window management into its own module
- Creating a dedicated Vulkan renderer class
- Adding shader management utilities
- Implementing resource management systems

## Contributing

Contributions are welcome! Please ensure your code follows the [coding standards](CODING_STANDARDS.md) before submitting a pull request.

### Before Submitting
- Read and follow [CODING_STANDARDS.md](CODING_STANDARDS.md)
- Ensure code compiles without warnings
- Add documentation for public APIs
- Test your changes thoroughly
- Run code formatting and linting tools
