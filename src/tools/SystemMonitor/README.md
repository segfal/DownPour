# SystemMonitor - Real-Time Hardware Metrics

C-based hardware monitoring tool with color-coded performance display.

## What It Does

Real-time detection of:
- **Vulkan Backend Type** - GPU (hardware accelerated) vs CPU (software rendering)
- **CPU Usage** - Percentage from macOS kernel APIs
- **GPU Usage** - Estimated from Metal framework
- **CPU/GPU Frequencies** - Current clock speeds
- **Power Consumption** - Estimated watts

## Color Coding

- 🟢 **Green** - Low usage (<40%)
- 🟡 **Yellow** - Moderate usage (40-80%)
- 🔴 **Red** - High usage (>80%)

## Usage

```bash
# Via Makefile (from project root)
make run-monitor

# Direct execution
./monitor
```

## Output Example

```
==============================================
  SYSTEM MONITOR - Hardware Metrics Report
==============================================

Vulkan Backend: GPU (Hardware Accelerated)

CPU Usage:      14.1%
CPU Frequency:  3000 MHz
CPU Power:      6.9 W

GPU Usage:      45.2%
GPU Frequency:  1400 MHz
GPU Power:      8.7 W

----------------------------------------------
Color Legend:
  █ Green   = Low usage (<40%)
  █ Yellow  = Moderate usage (40-80%)
  █ Red     = High usage (>80%)
==============================================
```

## Building

```bash
make        # Build monitor binary
make clean  # Clean build artifacts
make test   # Build and run
```

### Build Requirements
- Vulkan SDK (`brew install vulkan-sdk`)
- Xcode Command Line Tools
- macOS 14+ (M3 Mac tested)

## Technical Details

### Files
- `monitor.h` - API definitions and data structures
- `monitor.c` - Core logic and color output
- `macos_metrics.m` - Objective-C macOS API integration
- `vulkan_detector.c` - Vulkan backend detection
- `main.c` - Entry point (5-iteration test loop)
- `Makefile` - Build system

### macOS APIs Used
- **mach/mach_host.h** - CPU statistics via `host_processor_info()`
- **Metal framework** - GPU memory allocation tracking
- **IOKit framework** - Power and frequency data
- **Vulkan API** - Physical device enumeration

### How It Works

1. **Vulkan Detection:**
   - Creates minimal Vulkan instance
   - Queries `VkPhysicalDevice` properties
   - Checks device type: `VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU` = GPU

2. **CPU Metrics:**
   - Samples CPU tick counters (user, system, idle)
   - Calculates usage percentage from delta
   - Uses sysctl for frequency (or defaults to 3000 MHz for M3)

3. **GPU Metrics:**
   - Reads Metal device allocated memory vs recommended working set
   - Estimates usage as percentage (scaled 0-100%)
   - Defaults to 1400 MHz for M3 GPU

4. **Power Estimation:**
   - Scales linearly with usage (CPU: 5-20W, GPU: 5-15W)
   - Not actual hardware readings (IOReport API is private)

## Limitations

- GPU usage is estimated from memory allocation, not actual utilization
- Power consumption is calculated, not measured
- Frequency readings may be nominal values on Apple Silicon
- macOS-specific (uses IOKit, Metal, mach kernel APIs)

## Future Enhancements

- IOReport integration for real power readings
- Support for multiple GPUs
- Historical graphs
- JSON output mode
