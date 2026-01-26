#!/bin/bash

# DownPour Build and Run Script
# This script builds the project and runs the application

# Forward to the scripts directory version
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
exec bash "$SCRIPT_DIR/scripts/build_and_run.sh" "$@"
