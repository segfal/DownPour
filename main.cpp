#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <optional>
#include <set>

// Queue family indices helper struct
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

class DownPour {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; 

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    void initWindow();
    void initVulkan();
    void createInstance();
    void mainLoop();
    void cleanup();
    void createSurface();
};

void DownPour::initWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);     

    window = glfwCreateWindow(WIDTH, HEIGHT, "DownPour - Rain Simulator", nullptr, nullptr);
}

void DownPour::initVulkan() {
    createInstance();
    createSurface();
}

void DownPour::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)  throw std::runtime_error("Failed to create window surface!");
}

void DownPour::createInstance() {
    
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "DownPour";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    
    std::cout << "Enabled extensions:" << std::endl;
    for (const auto& extension : extensions) std::cout << "  " << extension << std::endl;
    

    
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;
    
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan instance!");

    std::cout << "✅ Vulkan instance created successfully on M3 Mac!" << std::endl;
}

void DownPour::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void DownPour::cleanup() {
    vkDestroyInstance(instance, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {
    DownPour app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

