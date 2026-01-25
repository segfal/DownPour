# Material System Migration Guide

## Overview

We're transitioning from the old monolithic Material structure (now `LegacyMaterial`) to a clean, modular system with separation of concerns.

## Architecture Changes

### Old System (Current - DEPRECATED)
```cpp
struct Material {
    VkImage textureImage;           // ❌ Couples data with GPU resources
    VkDeviceMemory textureMemory;   // ❌ Implementation detail leaking
    // ... 20+ Vulkan handles per material
    bool isTransparent;
    float alphaValue;
};
```

**Problems:**
- Material data mixed with GPU resources
- Can't query material properties without Vulkan
- Hard to test, hard to serialize
- Giant struct with 30+ fields

### New System (Target)
```cpp
// Pure data - no Vulkan dependencies
struct Material {
    uint32_t id;
    std::string name;
    MaterialProperties props;
    std::string baseColorTexture;  // Path, not GPU handle
    uint32_t indexStart, indexCount;
};

// GPU resources managed separately
class MaterialManager {
    uint32_t createMaterial(const Material& mat);
    void bindMaterial(uint32_t id, VkCommandBuffer cmd);
};
```

**Benefits:**
- Clean separation: Data vs. Implementation
- Easy to test, serialize, and debug
- Type-safe texture handles
- Centralized resource management

## Migration Status

### ✅ Completed
1. Created new `Material`, `MaterialProperties`, `TextureHandle` structures
2. Implemented `MaterialManager` with full texture loading
3. Renamed old struct to `LegacyMaterial` for backward compatibility
4. Updated `Model.h` and `Model.cpp` to use `LegacyMaterial` temporarily
5. Added `MaterialManager.cpp` to build system
6. Project compiles and runs successfully

### 🔄 In Progress
- Model.cpp still uses LegacyMaterial
- Existing rendering code still uses LegacyMaterial
- No migration path for existing materials yet

### 📋 TODO
1. Create material conversion utilities
2. Update rendering code to use MaterialManager
3. Migrate Model.cpp to populate new Material structs
4. Remove LegacyMaterial once migration complete

## Migration Steps (Gradual Approach)

### Phase 1: Parallel Systems (Current)
```cpp
// Old code continues to work
Model model;
model.loadFromFile("car.glb", ...);
auto& materials = model.getMaterials();  // Returns LegacyMaterial

// New system can be used alongside
MaterialManager matMgr(device, ...);
Material newMat;
newMat.name = "glass";
newMat.props.isTransparent = true;
newMat.props.alphaValue = 0.3f;
uint32_t id = matMgr.createMaterial(newMat);
```

### Phase 2: Add Conversion Layer
```cpp
// Helper to convert LegacyMaterial -> Material
Material convertToNewMaterial(const LegacyMaterial& legacy) {
    Material mat;
    mat.props.isTransparent = legacy.isTransparent;
    mat.props.alphaValue = legacy.alphaValue;
    mat.indexStart = legacy.indexStart;
    mat.indexCount = legacy.indexCount;
    // Extract texture paths if available
    return mat;
}

// Use in rendering code
for (const auto& legacyMat : model.getMaterials()) {
    Material newMat = convertToNewMaterial(legacyMat);
    uint32_t id = materialMgr.createMaterial(newMat);
    // Render using MaterialManager
}
```

### Phase 3: Update Model.cpp
```cpp
// In Model::loadFromFile()
void Model::loadFromFile(...) {
    // OLD: Create LegacyMaterial with Vulkan handles
    // NEW: Create Material with just data/paths
    
    Material newMaterial;
    newMaterial.name = gltfMaterial.name;
    newMaterial.props.isTransparent = /* ... */;
    newMaterial.props.alphaValue = /* ... */;
    newMaterial.baseColorTexture = texturePath;  // Just the path!
    
    materials.push_back(newMaterial);
    // No Vulkan calls here anymore!
}
```

### Phase 4: Update Rendering
```cpp
// In Application::recordCommandBuffer()
void Application::recordCommandBuffer(...) {
    // OLD:
    for (const auto& material : carModel.getMaterials()) {
        vkCmdBindDescriptorSets(..., material.descriptorSet, ...);
        vkCmdDrawIndexed(..., material.indexCount, ...);
    }
    
    // NEW:
    for (const auto& material : carModel.getMaterials()) {
        materialManager.bindMaterial(material.id, cmd, pipelineLayout);
        vkCmdDrawIndexed(..., material.indexCount, ...);
    }
}
```

### Phase 5: Remove Legacy Code
- Delete `LegacyMaterial` struct
- Remove deprecated texture loading methods from Model.cpp
- Remove `#include <fstream>` and `<chrono>` added for debug logs
- Clean up Material.h comments

## Code Locations

### New System
- `src/renderer/Material.h` - Material definitions, MaterialManager interface
- `src/renderer/MaterialManager.cpp` - MaterialManager implementation

### Old System (To be removed)
- `src/renderer/Material.h` - `LegacyMaterial` struct (lines 93-151)
- `src/renderer/Model.cpp` - Legacy texture loading methods (lines 439-531)

### Compatibility Layer
- `src/renderer/Model.h` - Uses `LegacyMaterial` temporarily (line 72)
- `src/renderer/Model.cpp` - Methods take `LegacyMaterial&` parameter

## Testing During Migration

```cpp
// Test 1: Verify old code still works
Model carModel;
carModel.loadFromFile("assets/models/bmw.glb", ...);
assert(carModel.getMaterials().size() > 0);  // Should work as before

// Test 2: Verify new system works
MaterialManager matMgr(device, physicalDevice, commandPool, queue);
Material testMat;
testMat.name = "test";
testMat.baseColorTexture = "assets/textures/test.png";
uint32_t id = matMgr.createMaterial(testMat);
assert(id > 0);
assert(matMgr.getDescriptorSet(id) != VK_NULL_HANDLE);

// Test 3: Verify they don't interfere
// Old and new systems should coexist peacefully
```

## Benefits After Migration

1. **Easier Debugging**
   - Can inspect material properties without GPU
   - Can mock MaterialManager for testing
   - Clear separation of concerns

2. **Transparency Fix**
   - Material alpha values cleanly separated
   - Easy to pass to shader via push constants
   - No confusion about where alpha comes from

3. **Future Features**
   - Material variants (LODs, quality levels)
   - Material hot-reloading
   - Shared texture pools
   - Async texture loading

## Timeline

- ✅ **Week 1**: Create new structures, maintain compatibility
- 🔄 **Week 2**: Add conversion utilities, test both systems
- 📋 **Week 3**: Migrate Model.cpp
- 📋 **Week 4**: Migrate rendering code
- 📋 **Week 5**: Remove legacy code, clean up

## Questions?

If you encounter issues during migration:
1. Check if LegacyMaterial is being used correctly
2. Verify MaterialManager initialization
3. Test conversion from Legacy -> New
4. Check descriptor set bindings

## Next Steps

To continue migration, see: `docs/PHASE2_MIGRATION.md` (to be created)
