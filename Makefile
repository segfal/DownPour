# DownPour Makefile - Minimal Refactored Version
# Build and run targets for the minimal sky/road/camera system

.PHONY: run run-only clean build build-all tools help

# === MAIN APPLICATION TARGETS ===

# Default target: build and run
run:
	@echo "Building and running DownPour..."
	@bash run.sh

# Run only (expects application to be already built)
run-only:
	@echo "Running DownPour (no build)..."
	@./build/DownPour

# Clean build from scratch
clean:
	@echo "Cleaning build directory..."
	@rm -rf build
	@mkdir -p build
	@echo "Building from clean state..."
	@cd build && cmake .. && cmake --build .
	@echo "Build complete!"

# Just build (no run)
build:
	@mkdir -p build
	@cd build && cmake .. && cmake --build .
	@echo "Build complete!"

# === DEVELOPMENT TOOLS ===

# Build available tools
tools:
	@echo "Building available development tools..."
	@if [ -d "src/tools/SystemMonitor" ]; then \
		echo "Building SystemMonitor..."; \
		cd src/tools/SystemMonitor && $(MAKE); \
	fi
	@if [ -d "src/tools/SceneEditor" ] && command -v cargo >/dev/null 2>&1; then \
		echo "Building SceneEditor..."; \
		cd src/tools/SceneEditor && cargo build --release; \
	fi
	@echo "Tools build complete!"

# === HELP ===

help:
	@echo "DownPour Makefile - Minimal Refactored Version"
	@echo ""
	@echo "Available targets:"
	@echo "  make run       - Build and run DownPour (default)"
	@echo "  make run-only  - Run without building"
	@echo "  make build     - Build only"
	@echo "  make clean     - Clean and rebuild from scratch"
	@echo "  make tools     - Build development tools"
	@echo "  make help      - Show this help"
	@echo ""
	@echo "Controls:"
	@echo "  WASD       - Move camera (forward/back/left/right)"
	@echo "  SPACE      - Move up"
	@echo "  LEFT SHIFT - Move down"
	@echo "  Mouse      - Look around"
	@echo "  ESC        - Toggle cursor capture"
	@echo ""

# Default target
.DEFAULT_GOAL := run
