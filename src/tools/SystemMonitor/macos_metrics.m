#include "monitor.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#include <stdio.h>

// C interface for getting CPU usage
float get_cpu_usage_percent(void) {
    host_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    kern_return_t kr;

    static unsigned long long prev_total = 0;
    static unsigned long long prev_idle = 0;

    kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                         (host_info_t)&cpu_info, &count);

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "[macOS Metrics] Failed to get CPU statistics\n");
        return 0.0f;
    }

    unsigned long long total_ticks = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) {
        total_ticks += cpu_info.cpu_ticks[i];
    }

    unsigned long long idle_ticks = cpu_info.cpu_ticks[CPU_STATE_IDLE];

    // Calculate usage since last call
    unsigned long long total_delta = total_ticks - prev_total;
    unsigned long long idle_delta = idle_ticks - prev_idle;

    prev_total = total_ticks;
    prev_idle = idle_ticks;

    if (total_delta == 0) {
        return 0.0f;
    }

    float usage = 100.0f * (1.0f - ((float)idle_delta / (float)total_delta));
    return usage < 0.0f ? 0.0f : (usage > 100.0f ? 100.0f : usage);
}

// C interface for getting GPU usage (Metal-based estimation)
float get_gpu_usage_percent(void) {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) {
            return 0.0f;
        }

        // Metal doesn't directly expose GPU usage percentage
        // We estimate based on current allocated size vs recommended working set
        uint64_t allocated = device.currentAllocatedSize;
        uint64_t recommended = device.recommendedMaxWorkingSetSize;

        if (recommended == 0) {
            return 0.0f;
        }

        // Rough estimation: usage based on memory allocation
        float usage = (100.0f * allocated) / recommended;

        // Clamp to reasonable values (this is an approximation)
        return usage < 0.0f ? 0.0f : (usage > 100.0f ? 50.0f : usage * 0.5f);
    }
}

// C interface for getting CPU frequency
uint32_t get_cpu_frequency_mhz(void) {
    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
        return (uint32_t)(freq / 1000000);  // Convert to MHz
    }

    // Fallback for Apple Silicon (frequency is fixed/not exposed via sysctl)
    // Return nominal frequency for M3
    return 3000;  // 3 GHz nominal for M3
}

// C interface for getting GPU frequency (not directly available on Metal)
uint32_t get_gpu_frequency_mhz(void) {
    // GPU frequency not exposed via public APIs on macOS
    // Return nominal frequency for M3 GPU cores
    return 1400;  // Approximate 1.4 GHz for M3 GPU
}

// C interface for estimating power consumption
void get_power_estimates(float* cpu_watts, float* gpu_watts) {
    // macOS doesn't expose direct power readings via public APIs
    // Use rough estimates based on usage percentage

    float cpu_usage = get_cpu_usage_percent();
    float gpu_usage = get_gpu_usage_percent();

    // M3 chip power envelope estimates:
    // CPU: 5-20W depending on load
    // GPU: 5-15W depending on load

    *cpu_watts = 5.0f + (cpu_usage / 100.0f) * 15.0f;  // Scale 5W-20W
    *gpu_watts = 5.0f + (gpu_usage / 100.0f) * 10.0f;  // Scale 5W-15W
}

// Implementation of get_system_metrics (called from monitor.c)
SystemMetrics get_system_metrics_impl(void) {
    SystemMetrics metrics;

    metrics.cpu_percent = get_cpu_usage_percent();
    metrics.gpu_percent = get_gpu_usage_percent();
    metrics.cpu_freq_mhz = get_cpu_frequency_mhz();
    metrics.gpu_freq_mhz = get_gpu_frequency_mhz();

    get_power_estimates(&metrics.cpu_power_watts, &metrics.gpu_power_watts);

    return metrics;
}
