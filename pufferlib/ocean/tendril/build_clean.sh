#!/bin/bash
# Quick build script for Tendril Clean Architecture
# Usage: ./build_clean.sh [demo|python|all]

set -e  # Exit on any error

echo "🎯 Tendril Clean Architecture - Quick Build Script"
echo "=================================================="

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    RAYLIB_PATH="../../../raylib-5.5_macos"  # Project local raylib
    DYLIB_VAR="DYLD_LIBRARY_PATH"
    PLATFORM_LIBS="-framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    RAYLIB_PATH="/usr/local"
    DYLIB_VAR="LD_LIBRARY_PATH"
    PLATFORM_LIBS="-lGL -lm -lpthread -ldl -lrt -lX11"
else
    echo "❌ Unsupported platform: $OSTYPE"
    exit 1
fi

echo "🖥️  Platform: $PLATFORM"

# Check if raylib exists
if [ ! -f "$RAYLIB_PATH/lib/libraylib.a" ] && [ ! -f "$RAYLIB_PATH/lib/libraylib.so" ] && [ ! -f "$RAYLIB_PATH/lib/libraylib.dylib" ]; then
    echo "⚠️  Warning: raylib not found at $RAYLIB_PATH"
    echo "   Please install raylib or adjust RAYLIB_PATH in this script"
    echo "   macOS: brew install raylib"
    echo "   Linux: sudo apt-get install libraylib-dev"
fi

# Get Python configuration
PYTHON_INCLUDE=$(python3 -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())" 2>/dev/null) || {
    echo "❌ Error: python3-dev not found"
    echo "   Install with: sudo apt-get install python3-dev python3-numpy"
    exit 1
}

NUMPY_INCLUDE=$(python3 -c "import numpy; print(numpy.get_include())" 2>/dev/null) || {
    echo "❌ Error: numpy not found"
    echo "   Install with: pip3 install numpy"
    exit 1
}

echo "🐍 Python include: $PYTHON_INCLUDE"
echo "📊 NumPy include: $NUMPY_INCLUDE"

# Common compiler flags
CFLAGS="-std=c99 -Wall -Wextra -O3 -fPIC -DPLATFORM_DESKTOP"
INCLUDES="-I. -I$PYTHON_INCLUDE -I$NUMPY_INCLUDE -I$RAYLIB_PATH/include"
RAYLIB_LIBS="-L$RAYLIB_PATH/lib -lraylib"

# Build target
TARGET=${1:-all}

build_demo() {
    echo "🔨 Building standalone demo..."
    gcc $CFLAGS $INCLUDES -o tendril_demo_clean \
        tendril_core.c render_2d.c demo_main.c \
        $RAYLIB_LIBS $PLATFORM_LIBS
    echo "✅ Demo built: tendril_demo_clean"
}

build_python() {
    echo "🔨 Building Python extension..."
    
    # Get Python version for linking
    PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
    
    gcc $CFLAGS $INCLUDES -shared -o binding.so \
        tendril_core.c render_2d.c binding_clean.c \
        $RAYLIB_LIBS $PLATFORM_LIBS -lpython$PYTHON_VERSION
    echo "✅ Python extension built: binding.so"
    
    # Test import
    echo "🧪 Testing Python import..."
    export $DYLIB_VAR="$RAYLIB_PATH/lib:$$DYLIB_VAR"
    if python3 -c "import binding; print('✅ Import successful')"; then
        echo "✅ Python binding works correctly"
    else
        echo "⚠️  Python import test failed - check library paths"
    fi
}

# Execute based on target
case $TARGET in
    "demo")
        build_demo
        echo "🚀 Run with: $DYLIB_VAR=$RAYLIB_PATH/lib ./tendril_demo_clean"
        ;;
    "python")
        build_python
        ;;
    "all")
        build_demo
        build_python
        echo ""
        echo "🎉 Build complete! Usage:"
        echo "   Demo: $DYLIB_VAR=$RAYLIB_PATH/lib ./tendril_demo_clean"
        echo "   Python: python3 -c 'import binding; print(\"Success!\")'"
        ;;
    *)
        echo "❌ Invalid target: $TARGET"
        echo "   Usage: ./build_clean.sh [demo|python|all]"
        exit 1
        ;;
esac

echo "✅ Build script completed successfully!"