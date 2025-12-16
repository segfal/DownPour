#include "monitor.h"
#include "vulkan_detector.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Forward declarations for Objective-C functions
extern SystemMetrics get_system_metrics_impl(void);

ResourceState get_cpu_state(void) {
    int val = rand() % 10;
    if (val < 2)
        return RESOURCE_IDLE;
    if (val < 8)
        return RESOURCE_ACTIVE;
    return RESOURCE_STRESSED;
}

ResourceState get_gpu_state(void) {
    int val = rand() % 10;
    if (val < 4)
        return RESOURCE_IDLE;
    if (val < 9)
        return RESOURCE_ACTIVE;
    return RESOURCE_STRESSED;
}

int is_display_active(void) {
    return 1;
}

const char* state_to_string(ResourceState state) {
    switch (state) {
        case RESOURCE_IDLE:
            return "IDLE";
        case RESOURCE_ACTIVE:
            return "ACTIVE";
        case RESOURCE_STRESSED:
            return "STRESSED";
        default:
            return "UNKNOWN";
    }
}

const char* stage_to_string(VulkanStage stage) {
    switch (stage) {
        case STAGE_TOP_OF_PIPE:
            return "TOP_OF_PIPE";
        case STAGE_VERTEX_INPUT:
            return "VERTEX_INPUT";
        case STAGE_VERTEX_SHADER:
            return "VERTEX_SHADER";
        case STAGE_FRAGMENT_SHADER:
            return "FRAGMENT_SHADER";
        case STAGE_COLOR_ATTACHMENT_OUTPUT:
            return "COLOR_ATTACHMENT_OUTPUT";
        case STAGE_COMPUTE_SHADER:
            return "COMPUTE_SHADER";
        case STAGE_TRANSFER:
            return "TRANSFER";
        case STAGE_BOTTOM_OF_PIPE:
            return "BOTTOM_OF_PIPE";
        default:
            return "UNKNOWN_STAGE";
    }
}

void log_system_status(void) {
    printf("\n[FIRMWARE MONITOR-C] System Status Report:\n");
    printf("  CPU: %s\n", state_to_string(get_cpu_state()));
    printf("  GPU: %s\n", state_to_string(get_gpu_state()));
    printf("  Display: %s\n", is_display_active() ? "ON" : "OFF");
}

void log_vulkan_stage(VulkanStage stage) {
    printf("[VULKAN INTROSPECTION-C] Current Stage: %s\n", stage_to_string(stage));
}

// ===== NEW HARDWARE DETECTION FUNCTIONS =====

VulkanBackend detect_vulkan_backend(void) {
    return detect_vulkan_backend_impl();
}

SystemMetrics get_system_metrics(void) {
    return get_system_metrics_impl();
}

UsageLevel classify_usage(float percent) {
    if (percent < 40.0f) {
        return USAGE_LOW;
    } else if (percent < 80.0f) {
        return USAGE_MODERATE;
    } else {
        return USAGE_HIGH;
    }
}

const char* get_color_code(UsageLevel level) {
    switch (level) {
        case USAGE_LOW:
            return "\033[32m";  // Green
        case USAGE_MODERATE:
            return "\033[33m";  // Yellow
        case USAGE_HIGH:
            return "\033[31m";  // Red
        default:
            return "\033[0m";   // Reset
    }
}

const char* backend_to_string(VulkanBackend backend) {
    switch (backend) {
        case BACKEND_GPU:
            return "GPU (Hardware Accelerated)";
        case BACKEND_CPU:
            return "CPU (Software Rendering)";
        default:
            return "Unknown";
    }
}

void print_colored_metrics(const SystemMetrics* metrics, VulkanBackend backend) {
    const char* reset = "\033[0m";

    printf("\n");
    printf("==============================================\n");
    printf("  SYSTEM MONITOR - Hardware Metrics Report\n");
    printf("==============================================\n\n");

    // Vulkan backend
    printf("Vulkan Backend: %s%s%s\n",
           backend == BACKEND_GPU ? "\033[32m" : "\033[33m",
           backend_to_string(backend),
           reset);
    printf("\n");

    // CPU metrics
    UsageLevel cpu_level = classify_usage(metrics->cpu_percent);
    printf("CPU Usage:      %s%5.1f%%%s\n",
           get_color_code(cpu_level),
           metrics->cpu_percent,
           reset);
    printf("CPU Frequency:  %u MHz\n", metrics->cpu_freq_mhz);
    printf("CPU Power:      %.1f W\n", metrics->cpu_power_watts);
    printf("\n");

    // GPU metrics
    UsageLevel gpu_level = classify_usage(metrics->gpu_percent);
    printf("GPU Usage:      %s%5.1f%%%s\n",
           get_color_code(gpu_level),
           metrics->gpu_percent,
           reset);
    printf("GPU Frequency:  %u MHz\n", metrics->gpu_freq_mhz);
    printf("GPU Power:      %.1f W\n", metrics->gpu_power_watts);
    printf("\n");

    // Legend
    printf("----------------------------------------------\n");
    printf("Color Legend:\n");
    printf("  %s█ Green%s   = Low usage (<40%%)\n", get_color_code(USAGE_LOW), reset);
    printf("  %s█ Yellow%s  = Moderate usage (40-80%%)\n", get_color_code(USAGE_MODERATE), reset);
    printf("  %s█ Red%s     = High usage (>80%%)\n", get_color_code(USAGE_HIGH), reset);
    printf("==============================================\n\n");
}
