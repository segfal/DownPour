# VulkanDispatch Usage Guide

## Overview

The `VulkanDispatch` system provides a centralized way to access Vulkan GPU resources throughout your application. It uses a hybrid approach:

- **Core Context** (struct): Fast, type-safe access to essential Vulkan resources
- **Dynamic Resources** (map): Flexible storage for pipeline-specific or optional resources

## Basic Setup

### 1. Create and Initialize

```cpp
#include "vulkan/VulkanDispatch.h"

// Create the dispatch
Vulkan::VulkanDispatch vulkanDispatch;

// Initialize core resources
vulkanDispatch.core.instance = myVkInstance;
vulkanDispatch.core.physical_device = myPhysicalDevice;
vulkanDispatch.core.device = myDevice;
vulkanDispatch.core.graphics_queue = myGraphicsQueue;
vulkanDispatch.core.present_queue = myPresentQueue;
vulkanDispatch.core.command_pool = myCommandPool;
vulkanDispatch.core.descriptor_pool = myDescriptorPool;
vulkanDispatch.core.surface = mySurface;
vulkanDispatch.core.swapchain = mySwapchain;
vulkanDispatch.core.render_pass = myRenderPass;

// Validate initialization
if (vulkanDispatch.core.isValid()) {
    std::cout << "VulkanDispatch initialized successfully\n";
}
```

### 2. Register Dynamic Resources

Use snake_case for all resource keys:

```cpp
// Pipelines
vulkanDispatch.set_resource("car_pipeline", carPipeline);
vulkanDispatch.set_resource("car_pipeline_layout", carPipelineLayout);
vulkanDispatch.set_resource("world_pipeline", worldPipeline);
vulkanDispatch.set_resource("debug_pipeline", debugPipeline);

// Descriptor layouts
vulkanDispatch.set_resource("car_descriptor_layout", carDescriptorLayout);
vulkanDispatch.set_resource("windshield_descriptor_layout", windshieldDescriptorLayout);

// Buffers
vulkanDispatch.set_resource("debug_vertex_buffer", debugVertexBuffer);
vulkanDispatch.set_resource("debug_vertex_memory", debugVertexMemory);

// Other resources
vulkanDispatch.set_resource("depth_image_view", depthImageView);
```

## Accessing Resources

### Core Resources (Direct Access)

```cpp
// Fast, type-safe access - no lookup overhead
VkDevice device = vulkanDispatch.core.device;
VkQueue queue = vulkanDispatch.core.graphics_queue;
VkCommandPool pool = vulkanDispatch.core.command_pool;

// Use in function calls
vkQueueSubmit(vulkanDispatch.core.graphics_queue, 1, &submitInfo, fence);
```

### Dynamic Resources (Map Access)

```cpp
// Retrieve with type specification
auto carPipeline = vulkanDispatch.get_resource<VkPipeline>("car_pipeline");
auto carLayout = vulkanDispatch.get_resource<VkPipelineLayout>("car_pipeline_layout");
auto debugBuffer = vulkanDispatch.get_resource<VkBuffer>("debug_vertex_buffer");

// Check existence before accessing
if (vulkanDispatch.has_resource("car_pipeline")) {
    auto pipeline = vulkanDispatch.get_resource<VkPipeline>("car_pipeline");
    // Use pipeline...
}
```

## Refactoring MaterialManager Example

### Before (Old Constructor):
```cpp
MaterialManager(VkDevice device, 
                VkPhysicalDevice physicalDevice, 
                VkCommandPool commandPool,
                VkQueue graphicsQueue);
```

### After (Using VulkanDispatch):
```cpp
// In Material.h
MaterialManager(const Vulkan::VulkanCoreContext& context);

// In MaterialManager.cpp
MaterialManager::MaterialManager(const Vulkan::VulkanCoreContext& context)
    : device(context.device),
      physicalDevice(context.physical_device),
      commandPool(context.command_pool),
      graphicsQueue(context.graphics_queue) {
    std::cout << "MaterialManager initialized\n";
}

// Usage in DownPour.cpp
materialManager = new MaterialManager(vulkanDispatch.core);
```

## Common Patterns

### 1. Passing Context to Subsystems

```cpp
// Instead of passing 4-5 individual parameters
Model(vulkanDispatch.core);
MaterialManager(vulkanDispatch.core);
WeatherSystem(vulkanDispatch.core);
```

### 2. Dynamic Resource Management

```cpp
// Store pipeline variants
vulkanDispatch.set_resource("car_pipeline_opaque", opaquePipeline);
vulkanDispatch.set_resource("car_pipeline_transparent", transparentPipeline);

// Later, retrieve based on material type
bool isTransparent = material.props.isTransparent;
std::string pipelineKey = isTransparent ? "car_pipeline_transparent" : "car_pipeline_opaque";
auto pipeline = vulkanDispatch.get_resource<VkPipeline>(pipelineKey);
```

### 3. Debugging Resources

```cpp
// List all registered resources
auto keys = vulkanDispatch.get_all_keys();
std::cout << "Registered resources:\n";
for (const auto& key : keys) {
    std::cout << "  - " << key << "\n";
}
```

### 4. Safe Resource Access

```cpp
try {
    auto pipeline = vulkanDispatch.get_resource<VkPipeline>("car_pipeline");
    // Use pipeline...
} catch (const std::runtime_error& e) {
    std::cerr << "Error: " << e.what() << "\n";
}
```

## Resource Naming Conventions

Use **snake_case** for all resource keys:

### Good Examples:
- `car_pipeline`
- `car_pipeline_layout`
- `world_descriptor_layout`
- `debug_vertex_buffer`
- `windshield_descriptor_layout`
- `depth_image_view`

### Bad Examples:
- `carPipeline` (camelCase)
- `CarPipeline` (PascalCase)
- `car-pipeline` (kebab-case)

## Integration Checklist

- [ ] Create `VulkanDispatch` instance in main Application class
- [ ] Initialize core context after Vulkan setup
- [ ] Register pipelines after pipeline creation
- [ ] Register descriptor layouts after layout creation
- [ ] Update MaterialManager constructor to use VulkanCoreContext
- [ ] Update Model class constructor (if needed)
- [ ] Replace individual handle passing with dispatch reference
- [ ] Add error handling for dynamic resource access

## Benefits

1. **Reduced Parameter Passing**: Pass one context instead of 4-5 parameters
2. **Centralized Resource Management**: All Vulkan resources in one place
3. **Type Safety**: Core resources have compile-time type checking
4. **Flexibility**: Dynamic resources can be added/removed at runtime
5. **Debugging**: Easy to inspect all registered resources
6. **Maintainability**: Adding new resources doesn't change function signatures

## Notes

- The dispatch does **NOT** own or manage Vulkan resource lifetimes
- Cleanup of actual Vulkan handles should still be done in your cleanup methods
- Core resources should be initialized early in your Vulkan setup
- Dynamic resources are optional and can be added as needed
