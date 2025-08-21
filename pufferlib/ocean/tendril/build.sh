#!/bin/bash
# Build script for Tendril environment bindings

set -e  # Exit on error

echo "🔧 Building Tendril Environment Bindings"
echo "========================================"

# Check for raylib
if ! command -v pkg-config &> /dev/null || ! pkg-config --exists raylib; then
    echo "Warning: pkg-config or raylib not found via pkg-config"
    echo "Trying to detect raylib manually..."
    
    # Check if RAYLIB_PATH is already set
    RAYLIB_FOUND=false
    if [[ -n "$RAYLIB_PATH" && -f "$RAYLIB_PATH/include/raylib.h" && -d "$RAYLIB_PATH/lib" ]]; then
        echo "Found raylib at: $RAYLIB_PATH (from environment)"
        RAYLIB_FOUND=true
    else
        # Check common locations
        for path in /usr/local /opt/homebrew /usr ~/raylib; do
            if [[ -f "$path/include/raylib.h" && -d "$path/lib" ]]; then
                echo "Found raylib at: $path"
                export RAYLIB_PATH="$path"
                RAYLIB_FOUND=true
                break
            fi
        done
    fi
    
    if [[ "$RAYLIB_FOUND" == "false" ]]; then
        echo "❌ ERROR: raylib not found!"
        echo "Please install raylib:"
        echo "  macOS: brew install raylib"
        echo "  Ubuntu: sudo apt install libraylib-dev"
        echo "  Or set RAYLIB_PATH environment variable"
        exit 1
    fi
fi

# Get Python/NumPy paths
echo "📦 Detecting Python environment..."
PYTHON_INC=$(python -c "import sysconfig; print(sysconfig.get_paths()['include'])")
NUMPY_INC=$(python -c "import numpy; print(numpy.get_include())")
EXT_SUFFIX=$(python -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")
PYTHON_LIBDIR=$(python -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR'))")
PYTHON_LIB=$(python -c "import sysconfig; print(sysconfig.get_config_var('LDLIBRARY'))")

echo "  Python include: $PYTHON_INC"
echo "  NumPy include:  $NUMPY_INC"
echo "  Extension suffix: $EXT_SUFFIX"

# Platform detection
OS=$(uname -s)
echo "🖥️  Platform: $OS"

# Compile
echo "🔨 Compiling binding.c..."

if [[ "$OS" == "Darwin" ]]; then
    # macOS
    cc -O3 -fPIC -std=c99 -DTENDRIL_WITH_RAYLIB -DPLATFORM_DESKTOP \
       -I"$PYTHON_INC" -I"$NUMPY_INC" -I"${RAYLIB_PATH:-/opt/homebrew}/include" \
       -c binding.c -o binding.o
    
    echo "🔗 Linking for macOS..."
    cc -bundle -undefined dynamic_lookup binding.o -o "binding$EXT_SUFFIX" \
       -L"${RAYLIB_PATH:-/opt/homebrew}/lib" \
       -lraylib -lm -lpthread -ldl \
       -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
       
elif [[ "$OS" == "Linux" ]]; then
    # Linux
    cc -O3 -fPIC -std=c99 -DTENDRIL_WITH_RAYLIB -DPLATFORM_DESKTOP \
       -I"$PYTHON_INC" -I"$NUMPY_INC" -I"${RAYLIB_PATH:-/usr/local}/include" \
       -c binding.c -o binding.o
    
    echo "🔗 Linking for Linux..."
    cc -shared binding.o -o "binding$EXT_SUFFIX" \
       -L"${RAYLIB_PATH:-/usr/local}/lib" \
       -lraylib -lm -lpthread -ldl -lX11 -lGL
else
    echo "❌ Unsupported platform: $OS"
    exit 1
fi

# Cleanup
rm -f binding.o

echo "✅ Build complete!"
echo "   Output: binding$EXT_SUFFIX"

# Quick test
echo "🧪 Testing import..."
if python -c "import binding; print('✅ Import successful!')"; then
    echo "🎉 Build and test successful!"
else
    echo "❌ Import test failed!"
    exit 1
fi