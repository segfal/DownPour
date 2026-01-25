# DownPour Makefile
# Simple targets for common build and run tasks

.PHONY: run clean run-log format build

# Default target: build and run
run:
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
