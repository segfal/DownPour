#include "monitor.h"

#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
    printf("SystemMonitor - Hardware Detection Tool\n");
    printf("Press Ctrl+C to exit\n\n");

    // Detect Vulkan backend
    printf("Detecting Vulkan backend...\n");
    VulkanBackend backend = detect_vulkan_backend();

    // Run monitoring loop
    for (int i = 0; i < 5; i++) {
        SystemMetrics metrics = get_system_metrics();
        print_colored_metrics(&metrics, backend);

        if (i < 4) {
            printf("Refreshing in 2 seconds...\n");
            sleep(2);
        }
    }

    printf("Monitoring complete.\n");
    return 0;
}
