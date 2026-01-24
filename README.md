# DownPour - Driving Simulator with Realistic Rain

A Vulkan-based driving simulator focused on realistic rain and windshield interaction, built with C++17.

## Core Features

- **Cockpit View Driving**: First-person perspective from inside the car
- **Weather System**: Toggle between sunny and rainy conditions (press 'R')
- **Rain Simulation**: Foundation for realistic rain particles and windshield effects (in development)
- **Vulkan Rendering**: High-performance graphics using modern Vulkan API

## Project Structure

```
DownPour/
├── main.cpp                        # Application entry point
├── src/
│   ├── DownPour.h/cpp             # Main application (window, input, main loop)
│   ├── renderer/                   # Rendering components
│   │   ├── Camera.h/cpp           # Camera system (cockpit view)
│   │   ├── Model.h/cpp            # 3D model loading (glTF)
│   │   ├── Vertex.h/cpp           # Vertex data structures
│   │   ├── Renderer.h/cpp         # Vulkan renderer (placeholder for refactor)
│   │   └── Scene.h/cpp            # Scene management (placeholder for refactor)
│   ├── simulation/                 # Simulation systems
│   │   └── WeatherSystem.h/cpp    # Weather state and rain control
│   └── vulkan/
│       └── VulkanTypes.h          # Vulkan-specific type definitions
├── shaders/                        # GLSL shaders
│   ├── basic.*                    # Basic rendering
│   ├── car.*                      # Car model rendering
│   ├── world.*                    # Road/environment rendering
│   ├── skybox.*                   # Sky rendering
│   ├── rain_particles.*           # Rain droplets (placeholder)
│   └── windshield_rain.frag       # Windshield water effects (placeholder)
├── assets/                         # 3D models, textures
├── headers/                        # Third-party dependencies (git submodules)
│   ├── glfw/                      # Window management
│   ├── glm/                       # Math library
│   └── tinygltf/                  # glTF model loader
└── tools/
    ├── archive/                    # Archived development tools
    └── skybox_colors.py           # Skybox utility

```

### Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - System architecture with Mermaid diagrams (state machines, object relationships)
- **[MATH.md](MATH.md)** - Mathematical reference with all formulas and equations used in the codebase
- **[CODING_STANDARDS.md](CODING_STANDARDS.md)** - Code style guidelines and best practices

## Code Organization

The codebase follows a clear separation of concerns:

### Application Layer (`src/DownPour.h/cpp`)
- **Application class**: High-level coordination
  - GLFW window creation and management
  - Main rendering loop
  - Input handling (keyboard, mouse)
  - Integration of Renderer, Scene, and WeatherSystem

### Rendering (`src/renderer/`)
- **Camera**: Cockpit camera with mouse look controls
- **Model**: 3D model loading and management (glTF format)
- **Vertex**: Vertex data structures and layouts
- **Renderer/Scene**: (Placeholders for future Vulkan refactoring)

### Simulation (`src/simulation/`)
- **WeatherSystem**: Weather state management
  - Toggle between Sunny/Rainy states
  - Integration points for rain particles and windshield effects
  - Future: Rain physics, droplet simulation

### Vulkan Core (`src/vulkan/`)
- **VulkanTypes**: Common Vulkan type definitions and helpers
  - Queue family management
  - Swapchain support queries

## Controls

- **Mouse**: Look around (cockpit view)
- **ESC**: Toggle cursor capture
- **R**: Toggle weather (Sunny ↔ Rainy)

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

### Quick Build Script

```bash
./run.sh  # Builds and runs the application
```

## Platform Support

Currently optimized for macOS (M3 Mac) with Metal backend support via MoltenVK.
Linux and Windows support planned.

## Architecture Goals

This refactor prioritizes:

1. **Readability**: Each class has a single, focused responsibility
2. **Modularity**: Clear separation between rendering, simulation, and application logic
3. **Rain-focused**: WeatherSystem provides clear integration points for rain effects
4. **Maintainability**: New features have obvious locations in the codebase

## Next Steps

- Implement rain particle rendering using `rain_particles.*` shaders
- Add windshield droplet effects using `windshield_rain.frag`
- Expand WeatherSystem with particle physics
- Add wiper blade simulation
- Implement droplet coalescence and sliding effects

## Code Style

This project follows simplified C++ best practices:

- **Namespaces**: `DownPour` (main), `DownPour::Simulation`, `DownPour::Renderer`
- **Naming**: PascalCase for classes, camelCase for functions/variables
- **Documentation**: Doxygen-style comments for public APIs
- **Formatting**: 4 spaces indentation, K&R brace style
- **Modern C++**: C++17 features (auto, nullptr, constexpr, etc.)

For detailed guidelines, see [CODING_STANDARDS.md](CODING_STANDARDS.md).

## Technical Documentation

The project includes comprehensive technical documentation:

- **Architecture Diagrams:** [ARCHITECTURE.md](ARCHITECTURE.md) provides Mermaid state and object diagrams documenting system behavior, component relationships, and rendering pipelines.

- **Mathematical Reference:** [MATH.md](MATH.md) documents all formulas and equations used throughout the codebase, including camera transformations, physics simulations, and graphics algorithms with LaTeX notation.

## Contributing

Contributions are welcome! Please ensure your code:
- Compiles without warnings
- Follows the project's code style
- Includes documentation for public APIs
- Is tested thoroughly
