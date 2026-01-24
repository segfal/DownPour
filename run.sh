#!/bin/bash
# Run DownPour from the correct directory

cd "$(dirname "$0")"
./build/DownPour "$@"
