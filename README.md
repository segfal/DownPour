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
│   ├── core/                       # Core Vulkan subsystems (NEW 2026-01-25)
│   │   ├── VulkanContext.h/cpp    # Instance, device, surface, queue management
│   │   ├── SwapChainManager.h/cpp # Swap chain, render pass, framebuffers
│   │   ├── PipelineFactory.h/cpp  # Graphics pipeline creation utility
│   │   └── ResourceManager.h/cpp  # Buffer/image creation and memory management
│   ├── renderer/                   # Rendering components
│   │   ├── Camera.h/cpp           # Camera system (cockpit view)
│   │   ├── Model.h/cpp            # 3D model data container (refactored)
│   │   ├── ModelGeometry.h/cpp    # Vulkan buffer management (NEW)
│   │   ├── GLTFLoader.h/cpp       # GLTF/GLB parsing utility (NEW)
│   │   ├── ModelAdapter.h/cpp     # Model + metadata loader
│   │   ├── MaterialManager.h/cpp  # Material and texture management
│   │   └── Vertex.h/cpp           # Vertex data structures
│   ├── scene/                      # Scene graph system
│   │   ├── Scene.h/cpp            # Scene container and rendering
│   │   ├── SceneNode.h/cpp        # Hierarchical transform nodes
│   │   ├── SceneManager.h/cpp     # Scene lifecycle management
│   │   ├── SceneBuilder.h/cpp     # GLTF to scene graph converter
│   │   ├── Entity.h/cpp           # Base entity class
│   │   ├── CarEntity.h/cpp        # Car-specific entity with roles
│   │   └── RoadEntity.h/cpp       # Road entity
│   ├── simulation/                 # Simulation systems
│   │   ├── WeatherSystem.h/cpp    # Weather state and rain control
│   │   └── WindshieldSurface.h/cpp # Windshield effects and wiper animation
│   ├── logger/                     # Logging system
│   │   └── Logger.h/cpp           # Multi-type logger with color output
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
│   └── models/
│       ├── bmw/                   # BMW car model
│       │   ├── bmw.glb            # Binary GLTF model
│       │   ├── bmw.gltf           # ASCII GLTF model
│       │   ├── bmw.glb.json       # Metadata sidecar (cockpit offset, roles, physics)
│       │   └── textures/          # Model textures
│       └── road/                  # Road model
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

The codebase has been refactored (2026-01-25) to follow strict separation of concerns with focused subsystems:

### Application Layer (`src/DownPour.h/cpp`)
- **Application class**: High-level coordination (~800 lines, reduced from 1,937)
  - GLFW window creation and management
  - Main rendering loop
  - Input handling (keyboard, mouse)
  - Integration of subsystems (VulkanContext, SwapChainManager, Scene, Weather)

### Core Vulkan Subsystems (`src/core/`) **NEW**
- **VulkanContext** (~200 lines): Manages Vulkan instance, physical device, logical device, surface, and queues
  - Centralized initialization and cleanup
  - Device feature selection
  - Queue family management
- **SwapChainManager** (~250 lines): Manages presentation resources
  - Swap chain creation and recreation
  - Render pass management
  - Framebuffer creation
- **PipelineFactory** (~200 lines): Static utility for graphics pipeline creation
  - Pipeline creation from configuration
  - Shader loading and module creation
  - Pipeline layout generation
- **ResourceManager** (~150 lines): Static utility for resource management
  - Buffer creation (vertex, index, uniform)
  - Image creation and memory allocation
  - Memory type finding utilities
  - Depth format selection

### Rendering (`src/renderer/`)
- **Camera**: Cockpit camera with mouse look controls and multiple camera modes
- **Model** (~100 lines, reduced from 710): Pure data container for 3D models
  - Stores geometry, materials, hierarchy, bounds, transform
  - Delegates to GLTFLoader and ModelGeometry
- **GLTFLoader** (~400 lines): Static utility for parsing GLTF/GLB files
  - tinygltf integration
  - Material and texture processing
  - Scene hierarchy parsing
- **ModelGeometry** (~180 lines): Manages Vulkan vertex/index buffers
  - Buffer creation and memory allocation
  - Separated from Model for testability
- **ModelAdapter**: Loads model + metadata from sidecar JSON files
- **MaterialManager**: GPU texture resources and descriptor sets
- **Vertex**: Vertex data structures and layouts

### Scene Graph (`src/scene/`)
- **SceneManager**: Scene lifecycle management
- **Scene**: Scene container and rendering coordination
- **SceneNode**: Hierarchical transform nodes with generational handles
- **SceneBuilder**: Converts GLTF hierarchy to SceneNode graph
- **Entity**: Base entity class with role-based node lookups
- **CarEntity**: Car-specific entity with semantic roles (wheels, steering, wipers)
- **RoadEntity**: Road entity

### Simulation (`src/simulation/`)
- **WeatherSystem**: Weather state management
  - Toggle between Sunny/Rainy states
  - Rain particle spawning and physics (max 5000 drops)
  - Integration with WindshieldSurface
- **WindshieldSurface**: Windshield effects and wiper animation
  - Wiper oscillation (±45°)
  - Wetness and flow map management

### Logger (`src/logger/`)
- **Logger**: Multi-type logging system with color output
  - Types: info, warning, error, debug, position, bug, fatal, critical
  - Non-singleton design (create instances as needed)

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

## Architecture Refactoring (2026-01-25)

The codebase underwent a major refactoring to improve maintainability, testability, and code clarity through strict separation of concerns.

### Model System Refactoring

**Problem:** Model.cpp was a 710-line "god object" handling GLTF parsing, Vulkan buffer management, material storage, hierarchy management, and more.

**Solution:** Extracted into focused classes:
- **GLTFLoader** (~400 lines): Static utility for parsing GLTF/GLB files
  - Uses tinygltf to load model data
  - Processes materials, textures, and scene hierarchy
  - Populates Model data structures
- **ModelGeometry** (~180 lines): Manages Vulkan vertex/index buffers
  - Buffer creation and memory allocation
  - Separated from parsing logic for testability
- **Model** (~100 lines): Pure data container
  - Stores geometry, materials, hierarchy, bounds, transform
  - 72% size reduction from original

**Benefits:**
- Can test GLTF parsing without Vulkan setup
- Can test buffer management without loading real models
- Easier to debug with smaller, focused files
- Clear data flow: GLTFLoader → Model → ModelGeometry

### Application Subsystem Extraction

**Problem:** Application.cpp was a 1,937-line monolithic class managing window, input, Vulkan initialization, swap chain, pipelines, resources, rendering, and more.

**Solution:** Extracted core Vulkan subsystems:
- **VulkanContext** (~200 lines): Instance, device, surface, queue management
  - Centralized Vulkan core initialization
  - Device selection and feature queries
  - Queue family management
- **SwapChainManager** (~250 lines): Presentation resources
  - Swap chain creation and recreation
  - Render pass management
  - Framebuffer lifecycle
- **PipelineFactory** (~200 lines): Graphics pipeline creation
  - Static utility for pipeline creation from config
  - Shader loading and module creation
  - Reusable across different pipeline types
- **ResourceManager** (~150 lines): Resource and memory management
  - Buffer creation (vertex, index, uniform)
  - Image creation and allocation
  - Memory type finding utilities

**Application.cpp** reduced to ~800 lines (60% reduction), now focuses on high-level coordination.

**Benefits:**
- Each subsystem has single, clear responsibility
- Easier to locate and modify specific functionality
- Reduced coupling between components
- Maintained 100% API compatibility (no breaking changes)
- All existing code continues to work

### Documentation Updates

All documentation updated to reflect new architecture:
- **CLAUDE.md**: PROJECT CONTEXT section updated with subsystem details
- **GEMINI.md**: Matching updates for consistency
- **ARCHITECTURE.md**: New Mermaid diagrams showing refactored architecture
- **README.md**: Updated Project Structure and Code Organization sections

## Contributing

Contributions are welcome! Please ensure your code:
- Compiles without warnings
- Follows the project's code style
- Includes documentation for public APIs
- Is tested thoroughly
