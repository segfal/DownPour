# SceneEditor Makefile
# Build Rust GUI tool for scene manipulation

.PHONY: build run clean release test

# Default target
all: build

# Build in debug mode
build:
	@echo "Building SceneEditor (debug)..."
	cargo build
	@echo "✓ Build complete: target/debug/scene_editor"

# Build in release mode (optimized)
release:
	@echo "Building SceneEditor (release)..."
	cargo build --release
	@echo "✓ Build complete: target/release/scene_editor"

# Run the editor with example config
run: build
	@echo "Launching SceneEditor..."
	cargo run

# Run release build
run-release: release
	@echo "Launching SceneEditor (release)..."
	./target/release/scene_editor

# Clean build artifacts
clean:
	@echo "Cleaning SceneEditor build artifacts..."
	cargo clean
	@echo "✓ Clean complete"

# Run tests
test:
	@echo "Running SceneEditor tests..."
	cargo test

# Check code without building
check:
	cargo check

# Format code
format:
	cargo fmt

help:
	@echo "SceneEditor Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default) - Build in debug mode"
	@echo "  build         - Build in debug mode"
	@echo "  release       - Build optimized release binary"
	@echo "  run           - Build and run the editor"
	@echo "  run-release   - Run optimized build"
	@echo "  clean         - Remove build artifacts"
	@echo "  test          - Run tests"
	@echo "  check         - Check code without building"
	@echo "  format        - Format code with rustfmt"
	@echo "  help          - Show this help message"
