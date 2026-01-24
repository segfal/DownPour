# DownPour Coding Standards

This document outlines the essential coding standards for the DownPour project. For comprehensive guidelines, refer to the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

## Quick Reference

### Naming Conventions
- **Classes/Structs**: `PascalCase` (e.g., `Application`, `WeatherSystem`)
- **Functions/Methods**: `camelCase` (e.g., `initWindow()`, `toggleWeather()`)
- **Variables**: `camelCase` (e.g., `windowHandle`, `rainIntensity`)
- **Constants**: `UPPER_SNAKE_CASE` or `kCamelCase` (e.g., `MAX_FRAMES_IN_FLIGHT`)
- **Namespaces**: `PascalCase` (e.g., `DownPour`, `Simulation`)
- **Files**: Match class name (e.g., `WeatherSystem.h`, `WeatherSystem.cpp`)

### Formatting

All code is formatted using clang-format with the configuration in `.clang-format`.

Key formatting rules:
- **Indentation**: 4 spaces (no tabs)
- **Braces**: K&R style (opening brace on same line)
- **Line length**: 120 characters maximum
- **Spacing**: One space after keywords, around operators
- **Alignment**: Consecutive declarations and assignments are aligned for readability

```cpp
// Aligned member variables
struct TextureHandle {
    VkImage        image   = VK_NULL_HANDLE;
    VkImageView    view    = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
};

// Function formatting
void Application::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        float deltaTime = calculateDeltaTime();
        updateSimulation(deltaTime);
    }
}
```

To format code automatically, run:
```bash
clang-format -i path/to/file.cpp
```

## Code Organization

### File Structure
- **One class per file**: `WeatherSystem.h` and `WeatherSystem.cpp`
- **Logical grouping**: Related code in subdirectories (`renderer/`, `simulation/`)
- **Header guards**: Use `#pragma once`

### Include Order
1. Related header (for .cpp files)
2. C/C++ standard library headers
3. Third-party library headers
4. Project headers

```cpp
#include "simulation/WeatherSystem.h"  // 1. Related header
#include <iostream>                     // 2. Standard library
#include <GLFW/glfw3.h>                // 3. Third-party
#include "renderer/Camera.h"           // 4. Project
```

### Class Layout
```cpp
class Application {
public:
    // Public methods
    void run();
    
private:
    // Private methods
    void initWindow();
    void cleanup();
    
    // Member variables
    GLFWwindow* window = nullptr;
    WeatherSystem weatherSystem;
};
```

## Documentation

### Public APIs
Document all public classes and methods with Doxygen-style comments:

```cpp
/**
 * @brief Weather system managing rain simulation
 * 
 * Manages weather state and provides integration points
 * for rain particles and windshield effects.
 */
class WeatherSystem {
public:
    /**
     * @brief Toggle between sunny and rainy weather
     */
    void toggleWeather();
};
```

### Private Implementation
Skip detailed documentation for obvious private methods. Focus on complex logic:

```cpp
private:
    // Good - Complex logic deserves a comment
    // Calculate droplet trajectory accounting for wind and gravity
    void calculateDropletPhysics(float deltaTime);
    
    // No need to document obvious things
    void cleanup();
```

### Inline Comments
Use sparingly, explain **why** not **what**:

```cpp
// ✅ Good - Explains WHY
// Use portability extension for MoltenVK compatibility on macOS
extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

// ❌ Bad - States the obvious
// Add extension to vector
extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
```

## Modern C++ (C++17)

### Prefer Modern Features
```cpp
// ✅ Use modern C++
auto deviceProps = getDeviceProperties();
for (const auto& ext : extensions) { }
static constexpr int MAX_FRAMES = 2;
if (auto result = findDevice(); result.has_value()) { }

// ❌ Avoid old-style C++
DeviceProperties deviceProps = getDeviceProperties();
for (size_t i = 0; i < extensions.size(); ++i) { }
#define MAX_FRAMES 2
```

### Resource Management (RAII)
Always use RAII for resource cleanup:

```cpp
// Resources automatically cleaned up in destructor
class WeatherSystem {
public:
    WeatherSystem() { /* initialize */ }
    ~WeatherSystem() { /* cleanup */ }
};
```

### Const Correctness
```cpp
class Camera {
public:
    glm::mat4 getViewMatrix() const;  // Doesn't modify object
    void setPosition(const glm::vec3& pos);  // Large type by const ref
};
```

## Vulkan-Specific

### Handle Initialization
```cpp
VkInstance instance = VK_NULL_HANDLE;
VkDevice device = VK_NULL_HANDLE;
```

### Error Checking
```cpp
if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create instance");
}
```

### Structure Initialization
```cpp
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "DownPour";
```

## Best Practices

### Keep It Simple
- Write **readable** code over clever code
- Keep functions **short** (<40 lines ideally)
- One function = one clear purpose
- Avoid deep nesting (max 3-4 levels)

### Separation of Concerns
- Rendering logic → `renderer/` classes
- Simulation logic → `simulation/` classes  
- Application coordination → `Application` class
- Utilities → `vulkan/` helpers

### Error Handling
- Use exceptions for unexpected errors
- Check return values from Vulkan API calls
- Provide meaningful error messages

## Pre-Commit Checklist

Before submitting code:
- [ ] Follows naming conventions
- [ ] Public APIs documented
- [ ] Code formatted with clang-format
- [ ] No compiler warnings
- [ ] Resources properly managed (no leaks)
- [ ] Modern C++ features used appropriately
- [ ] Code is readable and maintainable

## References

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Vulkan Best Practices](https://www.khronos.org/vulkan/)
