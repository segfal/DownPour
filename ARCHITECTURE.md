# DownPour Architecture Documentation

This document provides comprehensive architectural diagrams documenting the structure and behavior of the DownPour rain simulator.

## Table of Contents

1. [State Diagrams](#state-diagrams)
   - [Weather System](#weather-system-state-machine)
   - [Wiper System](#wiper-system-state-machine)
   - [Application Lifecycle](#application-lifecycle)
   - [Frame Rendering](#frame-rendering-state-machine)
2. [Object Diagrams](#object-diagrams)
   - [Core Application Architecture](#core-application-architecture)
   - [Rendering Pipeline Architecture](#rendering-pipeline-architecture)
   - [Model Loading Architecture](#model-loading-architecture)
   - [Simulation System Architecture](#simulation-system-architecture)

---

## State Diagrams

State diagrams document the behavior and transitions of various systems within the application.

### Weather System State Machine

The weather system controls environmental conditions and rain particle spawning.

```mermaid
stateDiagram-v2
    [*] --> Sunny
    
    Sunny --> Rainy: Toggle (R key)
    Rainy --> Sunny: Toggle (R key)
    
    state Rainy {
        [*] --> SpawningDrops
        SpawningDrops --> SpawningDrops: spawnRaindrop()
        SpawningDrops --> UpdatePhysics: Every frame
        UpdatePhysics --> CleanupInactive
        CleanupInactive --> SpawningDrops
    }
    
    state Sunny {
        [*] --> Idle
        Idle --> ClearDrops: On transition from Rainy
        ClearDrops --> Idle
    }
    
    note right of Rainy
        Max 5000 raindrops
        Spawn rate: 0.01s
        Gravity: -9.8 m/s²
    end note
```

**Implementation:** [`src/simulation/WeatherSystem.h`](src/simulation/WeatherSystem.h), [`src/simulation/WeatherSystem.cpp`](src/simulation/WeatherSystem.cpp)

**Key Points:**
- Simple two-state system toggled by user input
- Rainy state manages particle lifecycle: spawn → update → cleanup
- Particles deactivate when hitting ground (y < 0) or exceeding lifetime (10s)
- State transition immediately clears all raindrops when switching to Sunny

---

### Wiper System State Machine

The windshield wiper system oscillates between angular bounds when active.

```mermaid
stateDiagram-v2
    [*] --> Inactive
    
    Inactive --> Active: setWiperActive(true)
    Active --> Inactive: setWiperActive(false)
    
    state Active {
        [*] --> MovingRight
        
        MovingRight --> MovingLeft: angle >= 45°
        MovingLeft --> MovingRight: angle <= -45°
        
        note right of MovingRight
            angle += wiperSpeed * deltaTime
            wiperSpeed = 90°/s
        end note
        
        note right of MovingLeft
            angle -= wiperSpeed * deltaTime
        end note
    }
    
    state Inactive {
        [*] --> Parked
        note right of Parked
            angle = current position
            (no movement)
        end note
    }
```

**Implementation:** [`src/simulation/WindshieldSurface.h`](src/simulation/WindshieldSurface.h), [`src/simulation/WindshieldSurface.cpp`](src/simulation/WindshieldSurface.cpp)

**Key Points:**
- Wiper oscillates between -45° and +45° when active
- Movement speed: 90°/s (4 seconds for full sweep)
- Direction reverses at boundary conditions
- When inactive, wiper stays at current angle (parked position)

---

### Application Lifecycle

The main application progresses through distinct initialization and execution phases.

```mermaid
stateDiagram-v2
    [*] --> Uninitialized
    
    Uninitialized --> WindowInitialized: initWindow()
    WindowInitialized --> VulkanInitialized: initVulkan()
    VulkanInitialized --> Running: Enter mainLoop()
    
    state VulkanInitialized {
        [*] --> CreateInstance
        CreateInstance --> CreateSurface
        CreateSurface --> PickPhysicalDevice
        PickPhysicalDevice --> CreateLogicalDevice
        CreateLogicalDevice --> CreateSwapChain
        CreateSwapChain --> CreateRenderPass
        CreateRenderPass --> CreatePipelines
        CreatePipelines --> LoadModels
        LoadModels --> CreateSyncObjects
        CreateSyncObjects --> [*]
    }
    
    Running --> Cleanup: Window closed
    Cleanup --> Terminated: cleanup()
    Terminated --> [*]
    
    note right of Running
        Main render loop:
        - Process input
        - Update physics
        - Render frame
    end note
```

**Implementation:** [`src/DownPour.cpp`](src/DownPour.cpp) - `Application::run()` at line 39

**Key Points:**
- Sequential initialization ensures proper resource creation order
- Vulkan initialization is complex with many dependent steps
- Main loop runs until window close requested
- Cleanup releases all Vulkan resources in reverse creation order

---

### Frame Rendering State Machine

Each frame cycles through acquisition, recording, submission, and presentation phases.

```mermaid
stateDiagram-v2
    [*] --> WaitForFence
    
    WaitForFence --> AcquireImage: vkWaitForFences()
    AcquireImage --> RecordCommands: vkAcquireNextImageKHR()
    RecordCommands --> SubmitCommands: vkBeginCommandBuffer()
    SubmitCommands --> Present: vkQueueSubmit()
    Present --> WaitForFence: vkQueuePresentKHR()
    
    AcquireImage --> WaitForFence: Swapchain out of date
    Present --> WaitForFence: Next frame
    
    state RecordCommands {
        [*] --> BeginRenderPass
        BeginRenderPass --> RenderRoad
        RenderRoad --> RenderCarOpaque
        RenderCarOpaque --> RenderCarTransparent
        RenderCarTransparent --> RenderDebugMarkers
        RenderDebugMarkers --> EndRenderPass
        EndRenderPass --> [*]
        
        note right of BeginRenderPass
            Clear color + depth
            Set viewport/scissor
        end note
    }
    
    note right of SubmitCommands
        Synchronization:
        - Wait: imageAvailable
        - Signal: renderFinished
        - Fence: inFlightFence
    end note
```

**Implementation:** [`src/DownPour.cpp`](src/DownPour.cpp) - `Application::drawFrame()` and `Application::recordCommandBuffer()`

**Key Points:**
- Double buffering: MAX_FRAMES_IN_FLIGHT = 2
- Fence prevents CPU from getting ahead of GPU
- Semaphores synchronize GPU operations (image acquisition → rendering → presentation)
- Render order: opaque objects first (depth write), then transparent (depth test only)
- Swapchain recreation handled on OUT_OF_DATE error

---

## Object Diagrams

Object diagrams document the structural relationships between components.

### Core Application Architecture

The application has been refactored (2026-01-25) to extract Vulkan subsystems into focused managers, reducing the Application class from 1,937 lines to ~800 lines.

```mermaid
classDiagram
    class Application {
        -GLFWwindow* window
        -Camera camera
        -VulkanContext vulkanContext
        -SwapChainManager swapChainManager
        -SceneManager sceneManager
        -Model* carModel
        -Model* roadModel
        -WeatherSystem weatherSystem
        -WindshieldSurface windshield
        -MaterialManager* materialManager
        +run()
        +initWindow()
        +initVulkan()
        +mainLoop()
        +cleanup()
    }

    class VulkanContext {
        <<Core Subsystem>>
        -VkInstance instance
        -VkPhysicalDevice physicalDevice
        -VkDevice device
        -VkSurfaceKHR surface
        -VkQueue graphicsQueue
        -VkQueue presentQueue
        +initialize(window)
        +cleanup()
        +getDevice() VkDevice
        +getPhysicalDevice() VkPhysicalDevice
        +getGraphicsQueue() VkQueue
        +findQueueFamilies()
    }

    class SwapChainManager {
        <<Core Subsystem>>
        -VkSwapchainKHR swapchain
        -VkRenderPass renderPass
        -vector~VkFramebuffer~ framebuffers
        -vector~VkImageView~ imageViews
        +initialize(device, surface, window)
        +cleanup(device)
        +getSwapChain() VkSwapchainKHR
        +getRenderPass() VkRenderPass
        +getExtent() VkExtent2D
    }

    class PipelineFactory {
        <<Static Utility>>
        +createPipeline(config, renderPass) VkPipeline$
        +createPipelineLayout(layouts) VkPipelineLayout$
        -loadShaderModule(filepath) VkShaderModule$
        -readFile(filepath) vector~char~$
    }

    class ResourceManager {
        <<Static Utility>>
        +createBuffer(size, usage) VkBuffer$
        +copyBuffer(src, dst, size)$
        +createImage(width, height, format) VkImage$
        +findMemoryType(filter) uint32_t$
        +findDepthFormat() VkFormat$
    }

    class Camera {
        -vec3 position
        -vec3 forward
        -float pitch
        -float yaw
        +getViewMatrix() mat4
        +getProjectionMatrix() mat4
        +processInput()
    }

    class Model {
        -ModelGeometry geometry
        -vector~Material~ materials
        -vector~glTFNode~ nodes
        -mat4 modelMatrix
        +loadFromFile()
        +cleanup()
        +getVertexBuffer() VkBuffer
        +getMeshByName() NamedMesh
    }

    class ModelGeometry {
        -VkBuffer vertexBuffer
        -VkBuffer indexBuffer
        -VkDeviceMemory vertexMemory
        -VkDeviceMemory indexMemory
        +createBuffers(vertices, indices)
        +cleanup(device)
    }

    class GLTFLoader {
        <<Static Utility>>
        +load(filepath, Model&) bool$
        -resolveTexturePath()$
        -processGLTFTexture()$
    }

    Application *-- VulkanContext: contains
    Application *-- SwapChainManager: contains
    Application *-- Camera: contains
    Application *-- Model: contains 2
    Application ..> PipelineFactory: uses
    Application ..> ResourceManager: uses
    Model *-- ModelGeometry: contains
    Model ..> GLTFLoader: loaded by

    note for Application "Reduced from 1937 to ~800 lines\nby extracting subsystems"

    note for VulkanContext "~200 lines\nManages instance, device,\nsurface, queues"

    note for SwapChainManager "~250 lines\nManages swap chain,\nrender pass, framebuffers"

    note for PipelineFactory "~200 lines\nStatic pipeline creation\nwith shader loading"

    note for ResourceManager "~150 lines\nStatic buffer/image\ncreation utilities"
```

**Key Architectural Changes (2026-01-25):**
- **Separation of Concerns:** Vulkan subsystems extracted into focused managers
- **Static Utilities:** PipelineFactory, ResourceManager, GLTFLoader provide reusable creation logic
- **Reduced Complexity:** Application class now coordinates high-level flow, delegates resource management
- **Model Refactoring:** Model reduced from 710 to ~100 lines by extracting GLTFLoader and ModelGeometry
- **Maintained API Compatibility:** All existing code continues to work with new architecture

---

### Rendering Pipeline Architecture

Multiple graphics pipelines handle different rendering passes with shared resources.

```mermaid
classDiagram
    class VkRenderPass {
        <<Vulkan Resource>>
        Attachments: Color + Depth
    }
    
    class DescriptorSetLayout {
        <<Vulkan Resource>>
        Camera UBO binding
    }
    
    class WorldPipeline {
        -VkPipeline pipeline
        -VkPipelineLayout layout
        Purpose: Render road
        Depth: Write + Test
        Blend: Disabled
    }
    
    class CarPipeline {
        -VkPipeline pipeline
        -VkPipelineLayout layout
        Purpose: Render opaque car parts
        Depth: Write + Test
        Blend: Disabled
    }
    
    class CarTransparentPipeline {
        -VkPipeline pipeline
        -VkPipelineLayout layout
        Purpose: Render glass windows
        Depth: Test only
        Blend: SRC_ALPHA
    }
    
    class DebugPipeline {
        -VkPipeline pipeline
        -VkPipelineLayout layout
        Purpose: Visualize transforms
        Topology: LINE_LIST
        Depth: Test only
    }
    
    class WindshieldPipeline {
        -VkPipeline pipeline
        -VkPipelineLayout layout
        Purpose: Rain effects
        Status: TODO
    }
    
    VkRenderPass <-- WorldPipeline: uses
    VkRenderPass <-- CarPipeline: uses
    VkRenderPass <-- CarTransparentPipeline: uses
    VkRenderPass <-- DebugPipeline: uses
    VkRenderPass <-- WindshieldPipeline: uses
    
    DescriptorSetLayout <-- WorldPipeline: uses
    DescriptorSetLayout <-- CarPipeline: uses
    DescriptorSetLayout <-- CarTransparentPipeline: uses
    DescriptorSetLayout <-- DebugPipeline: uses
    
    note for VkRenderPass "Single render pass\nwith multiple pipelines\nfor different geometry"
    
    note for CarTransparentPipeline "Rendered AFTER opaque\nto ensure correct blending\nwith depth buffer"
```

**Key Points:**
- **Single Render Pass:** All pipelines use same render pass for efficiency
- **Render Order:** World → Car (opaque) → Car (transparent) → Debug
- **Depth Strategy:** Opaque objects write depth, transparent only test
- **Descriptor Sharing:** Camera UBO shared across most pipelines
- **Material System:** Car pipeline has per-material descriptor sets for textures

---

### Model Loading Architecture

The Model system has been refactored (2026-01-25) to separate concerns: GLTF parsing, data storage, and GPU resource management.

```mermaid
classDiagram
    class Model {
        <<Data Container>>
        -vector~Vertex~ vertices
        -vector~uint32_t~ indices
        -vector~Material~ materials
        -vector~NamedMesh~ namedMeshes
        -vector~glTFNode~ nodes
        -ModelGeometry geometry
        -glm::mat4 modelMatrix
        -glm::vec3 minBounds
        -glm::vec3 maxBounds
        +loadFromFile(filepath)
        +cleanup(device)
        +getMeshByName(name)
        +getMeshesByPrefix(prefix)
        +getVertexBuffer() VkBuffer
        +getIndexBuffer() VkBuffer
    }

    class GLTFLoader {
        <<Static Utility>>
        +load(filepath, Model&) bool$
        -resolveTexturePath(path, uri) string$
        -processGLTFTexture(...)$
    }

    class ModelGeometry {
        <<GPU Resources>>
        -VkBuffer vertexBuffer
        -VkBuffer indexBuffer
        -VkDeviceMemory vertexMemory
        -VkDeviceMemory indexMemory
        -uint32_t indexCount
        +createBuffers(vertices, indices)
        +cleanup(device)
        +getVertexBuffer() VkBuffer
        +getIndexBuffer() VkBuffer
        +getIndexCount() uint32_t
    }

    class Material {
        +VkImage textureImage
        +VkImageView textureImageView
        +VkSampler textureSampler
        +VkImage normalMap
        +VkImage metallicRoughnessMap
        +VkImage emissiveMap
        +uint32_t indexStart
        +uint32_t indexCount
        +bool isTransparent
        +float alphaValue
    }

    class NamedMesh {
        +string name
        +string nodeName
        +uint32_t meshIndex
        +uint32_t primitiveIndex
        +uint32_t indexStart
        +uint32_t indexCount
        +glm::mat4 transform
    }

    class Vertex {
        +glm::vec3 position
        +glm::vec3 normal
        +glm::vec2 texCoord
    }

    class tinygltf {
        <<External Library>>
        TinyGLTF loader
    }

    Model *-- ModelGeometry: contains
    Model *-- "0..*" Material: contains
    Model *-- "0..*" NamedMesh: contains
    Model *-- "0..*" Vertex: contains
    GLTFLoader ..> Model: populates
    GLTFLoader ..> tinygltf: uses
    ModelGeometry ..> ResourceManager: uses

    note for Model "Reduced from 710 to ~100 lines\nPure data container\nDelegates to subsystems"

    note for GLTFLoader "~400 lines\nParses GLTF/GLB files\nPopulates Model data structures"

    note for ModelGeometry "~180 lines\nManages Vulkan buffers\nSeparate from parsing logic"

    note for Material "PBR material system\nwith multiple texture maps\nand transparency support"

    note for NamedMesh "Allows querying specific\ncar parts by name\n(steering wheel, wipers, etc)"
```

**Implementation:**
- [`src/renderer/Model.h`](src/renderer/Model.h) / [`.cpp`](src/renderer/Model.cpp) - Pure data container (~100 lines, was 710)
- [`src/renderer/GLTFLoader.h`](src/renderer/GLTFLoader.h) / [`.cpp`](src/renderer/GLTFLoader.cpp) - GLTF parsing (~400 lines)
- [`src/renderer/ModelGeometry.h`](src/renderer/ModelGeometry.h) / [`.cpp`](src/renderer/ModelGeometry.cpp) - GPU buffers (~180 lines)

**Key Architectural Changes (2026-01-25):**
- **Separation of Concerns:** Parsing (GLTFLoader) vs. Storage (Model) vs. GPU (ModelGeometry)
- **Testability:** Can test GLTFLoader without Vulkan, can test geometry without parsing
- **Reusability:** GLTFLoader can be used with different data structures if needed
- **Simplicity:** Model.cpp reduced by 72%, easier to understand and maintain

**Loading Flow:**
1. `Model::loadFromFile(filepath, ...)` called
2. `GLTFLoader::load(filepath, model)` parses GLTF, populates vertices/indices/materials
3. `ModelGeometry::createBuffers(...)` creates Vulkan buffers from parsed data
4. Model ready for rendering with `getVertexBuffer()` / `getIndexBuffer()`

**Key Points:**
- **GLTF Support:** Loads both ASCII (.gltf) and binary (.glb) formats via tinygltf
- **Material System:** Each material tracks texture resources and index range
- **PBR Textures:** Base color, normal, metallic-roughness, emissive maps
- **Named Meshes:** Query by name for animation (steering wheel, wipers)
- **Transparency Detection:** Reads glTF alphaMode and baseColorFactor
- **Bounding Box:** Calculated during load for physics and camera positioning
- **Friend Access:** GLTFLoader declared as friend of Model to access private data directly

---

### Simulation System Architecture

The simulation systems manage dynamic environmental effects.

```mermaid
classDiagram
    class WeatherSystem {
        -WeatherState currentState
        -vector~Raindrop~ raindrops
        -float spawnTimer
        -float spawnRate
        +toggleWeather()
        +update(deltaTime)
        +render(cmd)
        +getActiveDrops()
        -spawnRaindrop()
        -updateRaindrops(deltaTime)
        -cleanupInactiveDrops()
    }
    
    class Raindrop {
        +glm::vec3 position
        +glm::vec3 velocity
        +float lifetime
        +float size
        +bool active
    }
    
    class WindshieldSurface {
        -bool wiperActive
        -float wiperAngle
        -float wiperSpeed
        -bool wiperDirection
        -VkImage wetnessMap
        -VkDeviceMemory wetnessMapMemory
        -VkImageView wetnessMapView
        -VkImage flowMap
        -VkDeviceMemory flowMapMemory
        -VkImageView flowMapView
        +initialize(device)
        +cleanup(device)
        +update(deltaTime, raindrops)
        +setWiperActive(bool)
        +getWiperAngle() float
        -updateWiper(deltaTime)
        -updateWetness(raindrops)
    }
    
    class Application {
        <<coordinator>>
    }
    
    WeatherSystem *-- "0..5000" Raindrop: manages
    Application *-- WeatherSystem: contains
    Application *-- WindshieldSurface: contains
    WeatherSystem ..> WindshieldSurface: provides raindrops to
    
    note for WeatherSystem "Spawns and updates\nrain particles\nMax 5000 active drops"
    
    note for WindshieldSurface "Manages windshield effects\nwetness/flow maps\nwiper animation"
    
    note for Raindrop "Simple particle:\ngravity physics\nlifetime management"
```

**Implementation:**
- [`src/simulation/WeatherSystem.h`](src/simulation/WeatherSystem.h) / [`.cpp`](src/simulation/WeatherSystem.cpp)
- [`src/simulation/WindshieldSurface.h`](src/simulation/WindshieldSurface.h) / [`.cpp`](src/simulation/WindshieldSurface.cpp)

**Key Points:**
- **Loose Coupling:** Weather and windshield are separate systems
- **Data Flow:** WeatherSystem provides active raindrops to WindshieldSurface
- **Future Integration:** Wetness map will be updated based on raindrop impacts
- **Wiper State:** Oscillates independently of rain state
- **Particle Pool:** Fixed maximum prevents unbounded memory growth
- **Spawn Strategy:** Uniform random distribution in 40m × 30m volume

---

## Diagram Conventions

### State Diagrams
- **Rounded boxes:** States
- **Arrows:** Transitions with conditions
- **Notes:** Implementation details and constants
- **Nested states:** Sub-state machines

### Object Diagrams
- **Classes:** Components or data structures
- **Solid arrows:** Composition (ownership)
- **Dashed arrows:** Dependency (uses)
- **Multiplicity:** 0..*, 1, 2, etc.
- **Stereotypes:** `<<Vulkan Resource>>`, `<<coordinator>>`

---

## Related Documentation

- **Mathematical Formulas:** See [MATH.md](MATH.md) for all equations and transformations
- **Code Standards:** See [CODING_STANDARDS.md](CODING_STANDARDS.md) for style guidelines
- **Project Overview:** See [README.md](README.md) for building and running

---

*Last Updated: January 2026*
