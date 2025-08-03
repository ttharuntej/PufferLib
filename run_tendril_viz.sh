#!/bin/bash

echo "=== PufferLib Tendril 3D Visualization ==="
echo "Starting tendril simulation with your hardware dimensions..."
echo ""

cd pufferlib/ocean/tendril

# Set library path and run
export DYLD_LIBRARY_PATH="/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/lib"

echo "Opening 3D window... (ESC to exit, mouse to rotate camera)"
./tendril_demo_fixed