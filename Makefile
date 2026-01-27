# DownPour Makefile
# Build and run targets for main application and development tools

.PHONY: run run-only clean run-log format build \
        tools tools-monitor tools-editor tools-converter \
        tools-clean install-tools \
        run-monitor run-editor run-converter \
        build-all help

# === MAIN APPLICATION TARGETS ===

# Default target: build and run
run:
	@bash scripts/build_and_run.sh

# Run only (no build - expects application to be already built)
run-only:
	@bash scripts/run.sh

# Clean build and run
clean:
	@bash scripts/run_clean.sh

# Build, run, and log output
run-log:
	@bash scripts/run_with_log.sh

# Format all C++ source files
format:
	@bash scripts/format.sh

# Just build (no run)
build:
	@mkdir -p build
	@cd build && cmake .. && cmake --build .

# === DEVELOPMENT TOOLS TARGETS ===

# Build all development tools
tools: tools-monitor tools-editor tools-converter
	@echo ""
	@echo "========================================"
	@echo "  All tools built successfully!"
	@echo "========================================"

# SystemMonitor (C) - Real hardware detection with color-coded metrics
tools-monitor:
	@echo "Building SystemMonitor (C)..."
	@cd src/tools/SystemMonitor && $(MAKE)
	@echo "✓ SystemMonitor built: src/tools/SystemMonitor/monitor"
	@echo ""

# SceneEditor (Rust) - Interactive GUI for scene manipulation
tools-editor:
	@echo "Building SceneEditor (Rust)..."
	@if ! command -v cargo >/dev/null 2>&1; then \
		echo "⚠ Rust/Cargo not found. Install with: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"; \
		exit 1; \
	fi
	@cd src/tools/SceneEditor && cargo build --release
	@echo "✓ SceneEditor built: src/tools/SceneEditor/target/release/scene_editor"
	@echo ""

# GLTFConverter (Python) - GLTF/GLB to JSON converter
tools-converter:
	@echo "Preparing GLTFTools converter..."
	@if ! command -v python3 >/dev/null 2>&1; then \
		echo "⚠ Python 3 not found"; \
		exit 1; \
	fi
	@echo "✓ GLTFConverter ready: python3 src/tools/GLTFTools/python/converter.py"
	@echo ""

# Clean all tool builds
tools-clean:
	@echo "Cleaning all tools..."
	@cd src/tools/SystemMonitor && $(MAKE) clean 2>/dev/null || true
	@cd src/tools/SceneEditor && cargo clean 2>/dev/null || true
	@echo "✓ All tools cleaned"

# Install Python tools (requires uv)
install-tools: tools-converter
	@echo "Installing Python tools with uv..."
	@if ! command -v uv >/dev/null 2>&1; then \
		echo "⚠ uv not found. Install with: pip install uv"; \
		exit 1; \
	fi
	@uv pip install pygltflib
	@echo "✓ Python tools installed"

# === RUN INDIVIDUAL TOOLS ===

# Run SystemMonitor
run-monitor: tools-monitor
	@echo "Running SystemMonitor..."
	@echo "========================================"
	@src/tools/SystemMonitor/monitor

# Run SceneEditor
run-editor: tools-editor
	@echo "Launching SceneEditor GUI..."
	@echo "========================================"
	@src/tools/SceneEditor/target/release/scene_editor

# Run GLTFConverter (show usage)
run-converter:
	@echo "GLTF/GLB to JSON Converter"
	@echo ""
	@echo "Usage:"
	@echo "  python3 src/tools/GLTFTools/python/converter.py <input.glb> [output.json]"
	@echo ""
	@echo "Example:"
	@echo "  python3 src/tools/GLTFTools/python/converter.py assets/models/bmw/bmw.glb output.json"
	@echo ""

# === COMBINED BUILD ===

# Build everything (main app + all tools)
build-all: build tools
	@echo ""
	@echo "========================================"
	@echo "  Complete build finished!"
	@echo "  - DownPour main application"
	@echo "  - SystemMonitor (C)"
	@echo "  - SceneEditor (Rust)"
	@echo "  - GLTFConverter (Python)"
	@echo "========================================"

# === HELP ===

help:
	@echo "DownPour Makefile - Build System"
	@echo ""
	@echo "Main Application:"
	@echo "  make run        - Build and run DownPour"
	@echo "  make run-only   - Run DownPour (skip build)"
	@echo "  make build      - Build DownPour only"
	@echo "  make clean      - Clean build and rebuild"
	@echo "  make run-log    - Build, run with logging"
	@echo "  make format     - Format C++ source code"
	@echo ""
	@echo "Development Tools:"
	@echo "  make tools            - Build all tools"
	@echo "  make tools-monitor    - Build SystemMonitor (C)"
	@echo "  make tools-editor     - Build SceneEditor (Rust)"
	@echo "  make tools-converter  - Prepare GLTFConverter (Python)"
	@echo "  make tools-clean      - Clean all tool builds"
	@echo ""
	@echo "Run Tools:"
	@echo "  make run-monitor    - Run SystemMonitor"
	@echo "  make run-editor     - Run SceneEditor GUI"
	@echo "  make run-converter  - Show GLTFConverter usage"
	@echo ""
	@echo "Combined:"
	@echo "  make build-all  - Build DownPour + all tools"
	@echo "  make help       - Show this help message"
