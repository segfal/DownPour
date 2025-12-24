# DownPour Coding Standards

This document outlines the coding standards for the DownPour project, based on the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

## Table of Contents
- [General Principles](#general-principles)
- [Naming Conventions](#naming-conventions)
- [Formatting](#formatting)
- [Comments and Documentation](#comments-and-documentation)
- [Header Files](#header-files)
- [Classes](#classes)
- [Functions](#functions)
- [Modern C++ Features](#modern-c-features)

---

## General Principles

### Code Organization
- **One class per file**: Each class should have its own `.h` and `.cpp` file
- **Logical grouping**: Related functionality should be grouped in subdirectories (e.g., `vulkan/`)
- **Minimal dependencies**: Keep coupling between modules low
- **Clear separation**: Separate interface (`.h`) from implementation (`.cpp`)

### Best Practices
- Write code that is **readable** and **maintainable**
- Favor **clarity** over cleverness
- Use **modern C++ features** (C++17) appropriately
- Follow the **principle of least surprise**
- Keep functions **short** and **focused** (ideally < 40 lines)

---

## Naming Conventions

### Files
- **Header files**: Use `.h` extension
- **Source files**: Use `.cpp` extension
- **File names**: Match the primary class name (e.g., `DownPour.h`, `DownPour.cpp`)
- **Case**: Use PascalCase for class files, snake_case for utility files

**Examples:**
```
✅ DownPour.h
✅ DownPour.cpp
✅ VulkanTypes.h
✅ utility_functions.h
```

### Types and Classes
- **Classes/Structs**: PascalCase (e.g., `Application`, `QueueFamilyIndices`)
- **Type aliases**: PascalCase (e.g., `using DevicePtr = VkDevice*;`)
- **Enums**: PascalCase for the type, UPPER_SNAKE_CASE for values

**Examples:**
```cpp
class Application { };
struct QueueFamilyIndices { };

enum class RenderMode {
    FORWARD,
    DEFERRED,
    RAYTRACING
};
```

### Variables
- **Local variables**: camelCase (e.g., `windowHandle`, `extensionCount`)
- **Member variables**: camelCase (e.g., `window`, `instance`)
- **Constants**: UPPER_SNAKE_CASE or kCamelCase (e.g., `MAX_FRAMES_IN_FLIGHT` or `kMaxFrames`)
- **Static constants**: kCamelCase (e.g., `kDefaultWidth`)

**Examples:**
```cpp
class Application {
private:
    // Member variables - camelCase
    GLFWwindow* window;
    VkInstance instance;
    
    // Constants - UPPER_SNAKE_CASE or static constexpr with kPrefix
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
};

void someFunction() {
    // Local variables - camelCase
    uint32_t extensionCount = 0;
    const char** extensions = nullptr;
}
```

### Functions and Methods
- **Functions/Methods**: camelCase (e.g., `initWindow()`, `createSurface()`)
- **Should be verb-based**: Describe what the function does

**Examples:**
```cpp
✅ void initWindow();
✅ void createInstance();
✅ bool isComplete() const;
✅ uint32_t getWidth() const;

❌ void window();  // Not a verb
❌ void Instance(); // Should be camelCase
```

### Namespaces
- **Namespace names**: PascalCase (e.g., `DownPour`, `Vulkan`)
- **Avoid deeply nested namespaces**: Maximum 2-3 levels
- **Use meaningful names**: Describe the module's purpose

**Examples:**
```cpp
namespace DownPour {
namespace Vulkan {
    struct QueueFamilyIndices { };
}
}
```

---

## Formatting

### Indentation and Spacing
- **Indentation**: 4 spaces (no tabs)
- **Line length**: Maximum 100 characters (soft limit)
- **Braces**: Opening brace on same line (K&R style)

**Examples:**
```cpp
// ✅ Correct
void Application::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(WIDTH, HEIGHT, "DownPour", nullptr, nullptr);
}

// ❌ Incorrect (Allman style)
void Application::initWindow()
{
    glfwInit();
}
```

### Spacing
- **After commas**: One space
- **Around operators**: One space on each side
- **After keywords**: One space (e.g., `if (condition)`)
- **No space**: Between function name and parentheses

**Examples:**
```cpp
// ✅ Correct
if (condition) {
    result = a + b;
    functionCall(arg1, arg2, arg3);
}

// ❌ Incorrect
if(condition){
    result=a+b;
    functionCall (arg1,arg2,arg3);
}
```

### Class Format
```cpp
class Application {
public:
    // Public methods first
    void run();
    
private:
    // Private members last
    void initWindow();
    void cleanup();
    
    // Member variables at the end
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
};
```

---

## Comments and Documentation

### File Headers
Every file should start with a brief description:

```cpp
/**
 * @file DownPour.h
 * @brief Main application class for the DownPour rain simulator
 * 
 * This file contains the Application class which manages the lifecycle
 * of the DownPour application including window management, Vulkan setup,
 * and the main rendering loop.
 */
```

### Class Documentation
Use Doxygen-style comments for all classes:

```cpp
/**
 * @brief Main application class for the DownPour rain simulator
 * 
 * This class manages the lifecycle of the application including:
 * - Window creation and management
 * - Vulkan initialization and setup
 * - Main rendering loop
 * - Cleanup and resource deallocation
 */
class Application {
    // ...
};
```

### Function Documentation
Document all public methods and complex private methods:

```cpp
/**
 * @brief Run the application
 * 
 * Initializes the window and Vulkan, runs the main loop,
 * and performs cleanup on exit.
 */
void run();

/**
 * @brief Check if all required queue families are found
 * @return true if both graphics and present families have values
 */
bool isComplete() const;
```

### Inline Comments
- Use `//` for single-line comments
- Place comments above the code they describe
- Keep comments concise and meaningful
- Don't state the obvious

**Examples:**
```cpp
// ✅ Good - Explains WHY
// Use portability extension for macOS/MoltenVK compatibility
extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

// ❌ Bad - States the obvious
// Push extension to vector
extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
```

### TODO Comments
```cpp
// TODO(username): Add validation layer support
// TODO: Implement device selection based on features
```

---

## Header Files

### Header Guards
Use `#pragma once` for header guards (simpler and less error-prone):

```cpp
#pragma once

// Header content
```

### Include Order
1. Related header (for `.cpp` files)
2. C system headers
3. C++ standard library headers
4. Third-party library headers
5. Project headers

**Example:**
```cpp
// DownPour.cpp
#include "DownPour.h"           // 1. Related header

#include <cstring>              // 2. C system headers

#include <iostream>             // 3. C++ standard library
#include <stdexcept>
#include <vector>

#include <GLFW/glfw3.h>         // 4. Third-party libraries

#include "vulkan/VulkanTypes.h" // 5. Project headers
```

### Forward Declarations
Use forward declarations when possible to reduce dependencies:

```cpp
// ✅ Good - Forward declaration
class Renderer;

class Application {
private:
    Renderer* renderer;  // Pointer, forward declaration is enough
};
```

---

## Classes

### Constructor Initialization
- Use member initializer lists
- Initialize in declaration when possible (C++11)

```cpp
class Application {
public:
    Application() : window(nullptr), instance(VK_NULL_HANDLE) {
        // Constructor body
    }
    
private:
    // ✅ Best - Initialize in declaration (C++11)
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    
    // Constants
    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
};
```

### Access Specifiers Order
1. `public`
2. `protected`
3. `private`

### Methods Before Data
Declare methods before member variables.

---

## Functions

### Parameter Passing
- **Small types** (int, bool, pointers): Pass by value
- **Large types** (structs, classes): Pass by const reference
- **Output parameters**: Pass by pointer or non-const reference

**Examples:**
```cpp
// ✅ Correct
void processData(const LargeStruct& input, Result* output);
bool validate(int value);
void modify(Settings& settings);

// ❌ Incorrect
void processData(LargeStruct input);  // Expensive copy
```

### Return Values
- Prefer return values over output parameters
- Use `std::optional` for values that may not exist
- Use exceptions for error handling

```cpp
// ✅ Good
std::optional<Device> findDevice(const Criteria& criteria);

// ❌ Avoid
bool findDevice(const Criteria& criteria, Device* output);
```

### Function Length
- Keep functions **short** and **focused**
- Ideally under 40 lines
- Extract complex logic into helper functions

---

## Modern C++ Features

### Use Modern C++ (C++17)
- **`nullptr`** instead of `NULL`
- **`auto`** for complex types (use judiciously)
- **Range-based for loops**
- **Smart pointers** when appropriate
- **`constexpr`** for compile-time constants
- **Structured bindings** (C++17)

**Examples:**
```cpp
// ✅ Modern C++
for (const auto& extension : extensions) {
    std::cout << extension << std::endl;
}

auto deviceProperties = getDeviceProperties();

static constexpr uint32_t MAX_FRAMES = 2;

// ❌ Old style
for (size_t i = 0; i < extensions.size(); ++i) {
    std::cout << extensions[i] << std::endl;
}

const uint32_t MAX_FRAMES = 2;
```

### RAII (Resource Acquisition Is Initialization)
Always use RAII for resource management:

```cpp
// ✅ Resources cleaned up automatically
class VulkanContext {
public:
    VulkanContext() {
        createInstance();
    }
    
    ~VulkanContext() {
        vkDestroyInstance(instance, nullptr);
    }
    
private:
    VkInstance instance;
};
```

### Const Correctness
- Mark methods `const` if they don't modify the object
- Use `const` for variables that don't change
- Use `const` references for read-only parameters

```cpp
class QueueFamilyIndices {
public:
    // ✅ Method marked const
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
    
private:
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
};
```

---

## Vulkan-Specific Guidelines

### Handle Initialization
Always initialize Vulkan handles to `VK_NULL_HANDLE`:

```cpp
VkInstance instance = VK_NULL_HANDLE;
VkDevice device = VK_NULL_HANDLE;
VkSurfaceKHR surface = VK_NULL_HANDLE;
```

### Error Handling
Check all Vulkan function return values:

```cpp
// ✅ Good
if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan instance!");
}

// ❌ Bad
vkCreateInstance(&createInfo, nullptr, &instance);  // Ignoring return value
```

### Structure Initialization
Use aggregate initialization for Vulkan structures:

```cpp
// ✅ Good
VkApplicationInfo appInfo{};
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "DownPour";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
```

---

## Code Review Checklist

Before submitting code, verify:

- [ ] Code follows naming conventions
- [ ] All public APIs are documented
- [ ] Code is properly formatted (4 spaces, K&R braces)
- [ ] No magic numbers (use named constants)
- [ ] All resources are properly cleaned up (RAII)
- [ ] Error conditions are handled
- [ ] Modern C++ features are used appropriately
- [ ] No memory leaks or resource leaks
- [ ] Code compiles without warnings
- [ ] Code is readable and maintainable

---

## References

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [Vulkan Best Practices](https://www.khronos.org/assets/uploads/developers/library/2016-vulkan-devday-uk/4-Vulkan-best-practices.pdf)

---

## Enforcement

These standards should be enforced through:
- Code reviews
- Static analysis tools (e.g., clang-tidy)
- Formatting tools (e.g., clang-format)
- CI/CD pipeline checks

For questions or clarifications, refer to the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
