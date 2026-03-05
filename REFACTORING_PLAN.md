# REFACTORING_PLAN.md

## Executive Summary

**Current Status**: ~9,000 lines across 50+ files. Over-abstracted scene graph, premature adapter pattern with 10+ unused config structs, stubbed simulation systems, and no actual physics implementation.

**Goal**: Reduce to ~5,000 lines. Eliminate premature abstractions, implement explicit FSMs where needed, separate physics (compute shaders) from rendering (graphics pipelines), focus ruthlessly on rain simulation.

**Max File Size**: 800 lines per file
**Core Principle**: Delete what doesn't serve rain physics. Add only what enables computational simulation.

---

## 1. What's Overcomplicated

### 1.1 ModelAdapter Configuration Explosion (543 lines)
**Problem**: 10+ nested config structs for features that don't exist
```
ModelAdapter.h:     229 lines
ModelAdapter.cpp:   314 lines
Total:              543 lines
```

**Structs that serve no purpose** (8 of 10):
- `WindshieldConfig` - WindshieldSurface is stubbed
- `WheelConfig` - CarEntity is empty
- `SteeringWheelConfig` - No car physics
- `DoorsConfig` - No doors system
- `LightsConfig` - No lighting system
- `AnimationConfig` - No animations
- `SpawnConfig` - Used once, could be vec3 + quat
- `DebugConfig` - Unused boolean flags

**Kept** (2 of 10):
- `CameraConfig` - Actually used by CameraEntity
- `PhysicsConfig` - Will be needed for rain physics

**Evidence**: CarEntity is 17 lines (9 cpp + 8 header), entirely stubbed. WeatherSystem is 38 lines, all empty methods returning defaults.

### 1.2 Over-Engineered Scene Graph (920 lines)
**Problem**: Enterprise-grade ECS for 2 entities (road + camera)
```
Entity.cpp:         187 lines
Scene.cpp:          294 lines
SceneManager.cpp:   115 lines
SceneBuilder.cpp:   106 lines
SceneNode.cpp:       55 lines
Total:              920 lines (excluding headers)
```

**Complexity for nothing**:
- SceneManager supports multiple scenes, only "main" exists
- Entity has role-based node lookups via unordered_map
- CarEntity/RoadEntity subclasses with no specialized behavior
- SceneBuilder converts GLTF hierarchy but adds indirection
- Generational handles in SceneNode for memory safety we don't need

**Reality**: Road is rendered via legacy loop in `recordCommandBuffer()`. Scene graph is bypassed.

### 1.3 Stubbed Simulation Systems (76 lines of lies)
**Problem**: Classes that pretend to do physics but return nulls/zeros
```cpp
// WeatherSystem.h - 38 lines
void toggleWeather() {}
void update(float deltaTime) { (void)deltaTime; }
WeatherState getState() const { return WeatherState::Sunny; }

// WindshieldSurface.h - 38 lines
void update(float deltaTime, const std::vector<Raindrop>& raindrops) {
    (void)deltaTime; (void)raindrops;
}
VkImageView getWetnessMapView() const { return VK_NULL_HANDLE; }
float getWiperAngle() const { return 0.0f; }
```

**Total dead code**: 76 lines that exist to satisfy interfaces but do nothing.

---

## 2. FSM Gaps (Boolean Flags vs State Pattern)

### 2.1 Camera State (Current: Boolean Soup)
```cpp
// DownPour.h - boolean flags scattered
bool cursorCaptured = true;
bool firstMouse = true;
float lastX, lastY;

// mainLoop() - manual state checks
if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !escPressed) {
    cursorCaptured = !cursorCaptured;
    // ... more manual logic
}
```

**Should be**:
```cpp
enum class CameraState { FreeFly, Cockpit, Chase, Paused };
class CameraStateMachine {
    CameraState currentState;
    void transition(CameraState newState);
    void update(float dt);
};
```

### 2.2 Weather State (Current: Enum with No Logic)
```cpp
// WeatherSystem.h - enum exists but methods empty
enum class WeatherState { Sunny, Rainy };
WeatherState currentState = WeatherState::Sunny;
void toggleWeather() {}  // Does nothing!
```

**Should be**:
```cpp
class WeatherStateMachine {
    WeatherState currentState;
    float transitionProgress;  // 0.0 (sunny) to 1.0 (rainy)

    void startTransition(WeatherState target);
    void update(float dt);
    bool isRaining() const;
    float getRainIntensity() const;  // 0.0-1.0 for compute shader
};
```

### 2.3 Wiper State (Current: Doesn't Exist)
```cpp
// WindshieldSurface.h - wiper stubbed
bool isWiperActive() const { return false; }
float getWiperAngle() const { return 0.0f; }
```

**Should be**:
```cpp
class WiperStateMachine {
    enum State { Parked, Wiping, Parking };
    State currentState;
    float angle;          // Current blade angle
    float angularVel;     // For smooth motion

    void activate();
    void deactivate();
    void update(float dt);
    float getBladePosition() const;  // For windshield compute shader
};
```

### 2.4 Missing: Droplet Lifecycle FSM
**No current implementation**

**Should exist**:
```cpp
class DropletStateMachine {
    enum State { Spawning, Falling, OnWindshield, Sliding, Wiped, Evaporating };
    void handleImpact(const Collision& collision);
    void handleWiper(float wiperPosition);
    void update(float dt);
};
```

---

## 3. Delete List

### Immediate Deletions (Serve No Purpose)

1. **src/scene/CarEntity.h/cpp** (26 lines)
   - Reason: Empty stub. Use Entity directly.

2. **src/scene/RoadEntity.h** (30 lines)
   - Reason: Empty stub. Use Entity directly.

3. **src/scene/SceneBuilder.h/cpp** (155 lines)
   - Reason: Converts GLTF to SceneNode but road bypasses scene graph anyway.
   - Action: Inline into Scene if hierarchy needed later.

4. **ModelAdapter: 8 config structs** (~300 lines)
   - Delete: WindshieldConfig, WheelConfig, SteeringWheelConfig, DoorsConfig, LightsConfig, AnimationConfig, SpawnConfig, DebugConfig
   - Keep: CameraConfig, PhysicsConfig
   - Result: ModelAdapter.h shrinks from 229 → ~100 lines

5. **Stubbed simulation files** (76 lines)
   - Delete: src/simulation/WeatherSystem.h/cpp (stubbed)
   - Delete: src/simulation/WindshieldSurface.h/cpp (stubbed)
   - Reason: Replace with proper FSM + compute shader implementations

**Total deleted**: ~587 lines of dead/premature code

### Conditional Deletions (Simplify First, Delete If Unused)

6. **SceneManager multi-scene support** (~50 lines)
   - Only "main" scene exists. Remove scene map, active scene tracking.
   - Simplify to single scene instance.

7. **Entity role-based lookup** (~30 lines in Entity.cpp)
   - unordered_map<string, NodeHandle> namedNodes
   - If only camera uses it, inline into CameraEntity.

**Potential total deleted**: ~670 lines

---

## 4. Keep & Simplify

### Keep As-Is (Well Factored)
✅ **core/** subsystem (800 lines total)
- VulkanContext, SwapChainManager, PipelineFactory, ResourceManager
- These are clean, focused, under 250 lines each

✅ **renderer/** Model system (643 lines: Model + GLTFLoader + ModelGeometry)
- Good separation: loading → data → GPU buffers
- Already refactored (Model was 710 lines, now 100)

✅ **renderer/** MaterialManager (620 lines)
- Could optimize but serves clear purpose (PBR textures)

### Simplify (Keep Core, Trim Fat)

📉 **SceneManager** (83 lines → ~40 lines)
- Remove: Multi-scene support (scenes map, activeSceneName)
- Keep: Single scene instance
- Keep: Entity management (needed for camera, road)
```cpp
class SceneManager {
    Scene scene;  // Single scene, not a map
    EntityMap entities;

    Entity* createEntity(const str& name);
    Entity* getEntity(const str& name);
    void update(float dt);
};
```

📉 **Scene** (294 lines → ~200 lines)
- Keep: SceneNode hierarchy, transform updates
- Remove: Rendering code (move to dedicated renderer)
- Remove: getNodesByPrefix (only used once for debugging)

📉 **Entity** (187 lines → ~120 lines)
- Keep: Node management, root transform
- Remove: Role-based lookups IF only camera uses them
- Remove: Templated animate methods (use direct SceneNode access)

📉 **ModelAdapter** (543 lines → ~250 lines)
- Keep: Model loading, CameraConfig, PhysicsConfig
- Delete: 8 unused config structs
- Result: Half the size, same functionality

**Simplification savings**: ~250 lines

---

## 5. Add List (What's Missing for Rain Physics)

### 5.1 Compute Shaders (NEW - Priority 1)
**Current**: 0 compute shaders. Rain is placeholder graphics only.

**Add**:
1. `shaders/rain_simulation.comp` (~200 lines)
   - Particle spawning based on WeatherStateMachine intensity
   - Gravity + wind + terminal velocity (ODEs)
   - Collision detection with windshield surface
   - Spatial hashing for neighbor queries (coalescence)

2. `shaders/windshield_compute.comp` (~300 lines)
   - Droplet dynamics on glass (sliding, merging)
   - Surface tension forces
   - Wiper interaction (clear wet pixels)
   - Flow map generation (for fragment shader distortion)

3. `shaders/droplet_physics.comp` (~250 lines)
   - Individual droplet shape deformation
   - Contact angle simulation
   - Evaporation rate based on temperature
   - Splatter dynamics on impact

**Integration**: New class `ComputePhysicsSystem` (~300 lines)
```cpp
class ComputePhysicsSystem {
    VkPipeline rainPipeline;
    VkPipeline windshieldPipeline;
    VkPipeline dropletPipeline;

    void dispatchRainSimulation(VkCommandBuffer cmd, float dt);
    void dispatchWindshieldUpdate(VkCommandBuffer cmd, float wiperPos);
    void dispatchDropletPhysics(VkCommandBuffer cmd);
};
```

### 5.2 State Machines (NEW - Priority 2)
**Current**: Boolean flags scattered across Application, no explicit states.

**Add**:
1. `src/simulation/WeatherStateMachine.h/cpp` (~150 lines)
   - States: Sunny, Clouding, Rainy, Clearing
   - Transition logic with interpolation
   - Rain intensity output for compute shaders

2. `src/simulation/WiperStateMachine.h/cpp` (~120 lines)
   - States: Parked, Wiping, Parking
   - Smooth angular motion (integrate velocity)
   - Blade position for windshield compute shader

3. `src/simulation/CameraStateMachine.h/cpp` (~100 lines)
   - States: FreeFly, Cockpit, Chase, Paused
   - Clean transitions, no boolean soup
   - Encapsulate cursor capture logic

4. `src/simulation/DropletStateMachine.h` (~80 lines)
   - States: Spawning, Falling, OnGlass, Sliding, Wiped, Evaporating
   - Per-droplet lifecycle (used in compute shader)
   - Transition conditions (collision, wiper contact, time)

**Total FSM code**: ~450 lines (replacing 0 explicit FSMs currently)

### 5.3 Physics Separation Layer (NEW - Priority 3)
**Current**: simulation/ has 76 lines of stubs.

**Add**: Proper physics-rendering bridge
1. `src/simulation/PhysicsWorld.h/cpp` (~250 lines)
   - Owns compute pipeline dispatches
   - GPU buffer management (particle SSBO, windshield texture)
   - Synchronization (compute → fragment shader)
   - Integration with state machines

2. `src/simulation/RainParticleSystem.h/cpp` (~200 lines)
   - CPU-side particle pool (for GPU upload)
   - Spawning based on WeatherStateMachine
   - Read-back for debugging (optional)

**Total physics layer**: ~450 lines

---

## 6. Refactor Steps (File-by-File)

### Step 1: Delete Dead Weight (Files: 5, Lines: -587)
**Goal**: Remove all stubbed/premature code

1.1. **Delete** `src/scene/CarEntity.h/cpp` (26 lines)
1.2. **Delete** `src/scene/RoadEntity.h` (30 lines)
1.3. **Delete** `src/scene/SceneBuilder.h/cpp` (155 lines)
1.4. **Delete** 8 config structs from `ModelAdapter.h` (~300 lines)
   - Keep only CameraConfig and PhysicsConfig blocks
1.5. **Delete** `src/simulation/WeatherSystem.h/cpp` (50 lines)
1.6. **Delete** `src/simulation/WindshieldSurface.h/cpp` (26 lines)

**Result**: -587 lines, no functionality lost (it was all stubs)

### Step 2: Simplify Scene Graph (Files: 3, Lines: ~200 → ~100)
**Goal**: Single scene, flatten entity hierarchy

2.1. **Simplify** `src/scene/SceneManager.h/cpp`
   - Remove scene map, keep single Scene instance
   - Remove scene name tracking
   - Result: 115 lines → ~60 lines

2.2. **Simplify** `src/scene/Scene.h/cpp`
   - Keep SceneNode hierarchy (needed for camera)
   - Remove getNodesByPrefix (debugging only)
   - Move rendering to dedicated renderer class
   - Result: 294 lines → ~200 lines

2.3. **Simplify** `src/scene/Entity.h/cpp`
   - Keep if camera needs it, else inline
   - Remove templated animate methods
   - Result: 187 lines → ~120 lines

**Result**: -200 lines, cleaner abstractions

### Step 3: Add FSM Infrastructure (Files: 4, Lines: +450)
**Goal**: Replace boolean soup with State Pattern

3.1. **Create** `src/simulation/WeatherStateMachine.h/cpp` (~150 lines)
   - enum State { Sunny, Clouding, Rainy, Clearing }
   - Transition logic with lerp (0.0 = sunny, 1.0 = rainy)
   - `float getRainIntensity()` for compute shader

3.2. **Create** `src/simulation/WiperStateMachine.h/cpp` (~120 lines)
   - enum State { Parked, Wiping, Parking }
   - Integrate angular velocity for smooth motion
   - `float getBladeAngle()` for windshield shader

3.3. **Create** `src/simulation/CameraStateMachine.h/cpp` (~100 lines)
   - enum State { FreeFly, Cockpit, Chase, Paused }
   - Encapsulate cursorCaptured, firstMouse logic
   - Clean state transitions

3.4. **Create** `src/simulation/DropletStateMachine.h` (~80 lines)
   - enum State { Spawning, Falling, OnGlass, Sliding, Wiped, Evaporating }
   - Used per-droplet in compute shader
   - Transition conditions (impact, wiper, time)

**Result**: +450 lines, explicit state management

### Step 4: Implement Compute Shaders (Files: 3, Lines: +750)
**Goal**: Actual rain physics (not rendering hacks)

4.1. **Create** `shaders/rain_simulation.comp` (~200 lines GLSL)
   - Input: WeatherStateMachine intensity
   - Physics: Gravity, drag, wind, terminal velocity
   - Output: Particle positions (SSBO)
   - Collisions: Raycast against windshield plane

4.2. **Create** `shaders/windshield_compute.comp` (~300 lines GLSL)
   - Input: Droplet impacts from rain_simulation
   - Physics: Sliding (surface angle), merging (distance threshold)
   - Wiper: Clear pixels where blade passes
   - Output: Wetness map (R32F texture), flow vectors (RG16F)

4.3. **Create** `shaders/droplet_physics.comp` (~250 lines GLSL)
   - Input: Individual droplet state
   - Physics: Shape deformation, contact angle
   - Evaporation: Time-based decay
   - Output: Updated droplet properties

**Result**: +750 lines of actual physics

### Step 5: Physics-Rendering Bridge (Files: 3, Lines: +700)
**Goal**: CPU-side compute pipeline management

5.1. **Create** `src/simulation/PhysicsWorld.h/cpp` (~250 lines)
   - VkPipeline for each compute shader
   - Descriptor sets (SSBO, textures)
   - `dispatchCompute(cmd, shader, workgroups)`
   - Barrier management (compute → graphics)

5.2. **Create** `src/simulation/RainParticleSystem.h/cpp` (~200 lines)
   - CPU particle pool (up to 5000 particles)
   - Spawning based on WeatherStateMachine
   - Upload to GPU SSBO each frame
   - Optional read-back for debugging

5.3. **Create** `src/simulation/ComputePhysicsSystem.h/cpp` (~250 lines)
   - Owns PhysicsWorld instance
   - Coordinates FSMs → compute shaders
   - Integration point in DownPour.cpp

**Result**: +700 lines, physics separation complete

### Step 6: Integration & Cleanup (Files: 2, Lines: +100/-50)
**Goal**: Wire everything into DownPour.cpp

6.1. **Modify** `src/DownPour.cpp` (+100 lines)
   - Replace boolean flags with CameraStateMachine
   - Add ComputePhysicsSystem initialization
   - Add compute dispatch in drawFrame() before graphics
   - Wire 'R' key to WeatherStateMachine

6.2. **Modify** `src/DownPour.h` (+50 lines, -50 old booleans)
   - Remove: cursorCaptured, firstMouse, escPressed flags
   - Add: CameraStateMachine, WeatherStateMachine, ComputePhysicsSystem members

**Result**: +150 net lines, cleaner main loop

---

## 7. Before & After Comparison

### Line Counts
```
Category              | Before | After  | Change
----------------------|--------|--------|--------
Application (DownPour)|   909  |  1009  | +100
Core (Vulkan)         |   800  |   800  | +0
Renderer              |  2143  |  1893  | -250 (simplify ModelAdapter)
Scene Graph           |  1510  |  1060  | -450 (flatten, remove builder)
Simulation (stubs)    |    76  |     0  | -76 (delete)
Simulation (FSMs)     |     0  |   450  | +450 (add)
Simulation (Physics)  |     0  |   700  | +700 (add)
Shaders (compute)     |     0  |   750  | +750 (add)
----------------------|--------|--------|--------
TOTAL                 | ~5438  | ~5662  | +224
```

### File Counts
```
Before: ~45 files
After:  ~48 files (+7 new, -4 deleted)
```

### Complexity Metrics
```
Metric                        | Before | After
------------------------------|--------|-------
Unused config structs         |   8    |   0
Stubbed classes               |   3    |   0
Boolean flags for state       |   5    |   0
Explicit FSMs                 |   0    |   4
Compute shaders               |   0    |   3
Max file size                 |  719   |  <800
Avg file size                 |  121   |  118
```

---

## 8. Risk Assessment

### Low Risk (Pure Deletion)
- Deleting CarEntity/RoadEntity/SceneBuilder: No code uses them
- Deleting ModelAdapter config structs: Not referenced anywhere
- Deleting stubbed simulation: Returns nulls/defaults

### Medium Risk (Refactoring)
- Simplifying SceneManager: Need to update Entity creation calls
- Simplifying Scene: Need to verify no hidden dependencies
- CameraStateMachine: Input handling spread across DownPour.cpp

### High Risk (New Implementation)
- Compute shaders: First time adding compute to project
- PhysicsWorld: Synchronization barriers critical for correctness
- Integration: Main loop now has compute → graphics pipeline

**Mitigation**:
- Implement compute shaders one at a time (rain → windshield → droplet)
- Test each FSM independently before integration
- Keep old rendering path until compute validated

---

## 9. Success Criteria

### Quantitative
✅ No file exceeds 800 lines
✅ All stubbed classes deleted
✅ Unused config structs removed
✅ At least 3 compute shaders implemented
✅ 4 FSMs replace boolean flags

### Qualitative
✅ Rain particles computed on GPU (not fake rendering)
✅ Windshield wetness based on actual droplet impacts
✅ Wiper clears water using compute, not alpha blend hack
✅ State transitions are explicit (State Pattern)
✅ Physics and rendering cleanly separated

### Performance
✅ 60 FPS with 5000 rain particles (compute shader target)
✅ Windshield compute runs <2ms per frame
✅ No CPU-side particle simulation (all GPU)

---

## 10. Timeline Estimate

**Phase 1: Delete** (1 day)
- Remove dead code, stubbed systems
- Result: -587 lines

**Phase 2: Simplify** (2 days)
- SceneManager, Scene, Entity, ModelAdapter
- Result: -250 lines

**Phase 3: FSMs** (3 days)
- Weather, Wiper, Camera, Droplet state machines
- Result: +450 lines

**Phase 4: Compute** (5 days)
- rain_simulation.comp, windshield_compute.comp, droplet_physics.comp
- Result: +750 lines

**Phase 5: Physics Bridge** (3 days)
- PhysicsWorld, RainParticleSystem, ComputePhysicsSystem
- Result: +700 lines

**Phase 6: Integration** (2 days)
- Wire into DownPour.cpp, test, debug
- Result: +150 lines

**Total**: 16 days for complete refactor + rain physics implementation

---

## 11. Open Questions

1. **Spatial hashing**: Implement in compute shader or CPU?
   - Recommendation: Compute shader (GPU parallel)

2. **Windshield geometry**: Flat plane or curved mesh?
   - Recommendation: Start flat, add curvature later

3. **Droplet count**: 5000 max or dynamic?
   - Recommendation: Fixed 5000 (predictable perf)

4. **Compute dispatch frequency**: Every frame or 60Hz fixed?
   - Recommendation: Every frame (deltaTime in push constants)

5. **Read-back for debug**: CPU access to particle data?
   - Recommendation: Optional staging buffer (disable in release)

---

## End of Plan

**Next Action**: Begin Step 1 (Delete Dead Weight) and validate build after each deletion.
