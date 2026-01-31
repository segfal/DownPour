#pragma once

#include <stdint.h>

// Enum for simulated system resource states (legacy)
typedef enum { RESOURCE_IDLE, RESOURCE_ACTIVE, RESOURCE_STRESSED } ResourceState;

// Enum for usage levels with color coding
typedef enum {
    USAGE_LOW,       // < 40% (Green)
    USAGE_MODERATE,  // 40-80% (Yellow)
    USAGE_HIGH       // > 80% (Red)
} UsageLevel;

// Enum for Vulkan backend type
typedef enum {
    BACKEND_GPU,     // Hardware GPU acceleration
    BACKEND_CPU,     // Software/CPU fallback
    BACKEND_UNKNOWN  // Could not determine
} VulkanBackend;

// Enum for Vulkan pipeline stages
typedef enum {
    STAGE_TOP_OF_PIPE,
    STAGE_VERTEX_INPUT,
    STAGE_VERTEX_SHADER,
    STAGE_FRAGMENT_SHADER,
    STAGE_COLOR_ATTACHMENT_OUTPUT,
    STAGE_COMPUTE_SHADER,
    STAGE_TRANSFER,
    STAGE_BOTTOM_OF_PIPE
} VulkanStage;

// Struct for real system metrics
typedef struct {
    float    cpu_percent;
    float    gpu_percent;
    float    cpu_power_watts;
    float    gpu_power_watts;
    uint32_t cpu_freq_mhz;
    uint32_t gpu_freq_mhz;
} SystemMetrics;

// Legacy functions (simulated states)
ResourceState get_cpu_state(void);
ResourceState get_gpu_state(void);
int           is_display_active(void);

// New hardware detection functions
VulkanBackend detect_vulkan_backend(void);
SystemMetrics get_system_metrics(void);
UsageLevel    classify_usage(float percent);
const char*   get_color_code(UsageLevel level);
void          print_colored_metrics(const SystemMetrics* metrics, VulkanBackend backend);

// Utility functions
void        log_system_status(void);
void        log_vulkan_stage(VulkanStage stage);
const char* state_to_string(ResourceState state);
const char* stage_to_string(VulkanStage stage);
const char* backend_to_string(VulkanBackend backend);
