# SystemMonitor Makefile
# Compiles C and Objective-C sources for macOS hardware monitoring

CC = clang
CFLAGS = -Wall -Wextra -O2 -std=c11
FRAMEWORKS = -framework IOKit -framework Metal -framework Foundation
VULKAN_SDK = $(shell if [ -d "/usr/local" ]; then echo "/usr/local"; else echo "$(HOME)/VulkanSDK"; fi)
VULKAN_FLAGS = -I$(VULKAN_SDK)/include -L$(VULKAN_SDK)/lib -lvulkan -Wl,-rpath,$(VULKAN_SDK)/lib

# Source files
C_SRCS = monitor.c vulkan_detector.c main.c
OBJC_SRCS = macos_metrics.m

# Object files
C_OBJS = $(C_SRCS:.c=.o)
OBJC_OBJS = $(OBJC_SRCS:.m=.o)
ALL_OBJS = $(C_OBJS) $(OBJC_OBJS)

# Target binary
TARGET = monitor

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) -o $@ $^ $(FRAMEWORKS) $(VULKAN_FLAGS)
	@echo "✓ Build complete: $(TARGET)"

# Compile C files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I$(VULKAN_SDK)/include -c $< -o $@

# Compile Objective-C files
%.o: %.m
	@echo "Compiling $< (Objective-C)..."
	$(CC) $(CFLAGS) $(FRAMEWORKS) -c $< -o $@

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(ALL_OBJS) $(TARGET)
	@echo "✓ Clean complete"

test: $(TARGET)
	@echo "Running SystemMonitor test..."
	./$(TARGET)

help:
	@echo "SystemMonitor Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build the monitor binary"
	@echo "  clean         - Remove build artifacts"
	@echo "  test          - Build and run the monitor"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - Vulkan SDK installed (brew install vulkan-sdk)"
	@echo "  - macOS with Metal support"
	@echo "  - Xcode Command Line Tools"
