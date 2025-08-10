# PufferLib Tendril Environment - Clean Architecture Makefile
# Alternative to CMake for systems without cmake

# Detect platform
UNAME_S := $(shell uname -s)

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O3 -fPIC -DPLATFORM_DESKTOP
DEBUG_FLAGS = -g -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG

# Python configuration (adjust paths as needed)
PYTHON_VERSION = $(shell python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PYTHON_INCLUDE = $(shell python3 -c "from distutils.sysconfig import get_python_inc; print(get_python_inc())")
NUMPY_INCLUDE = $(shell python3 -c "import numpy; print(numpy.get_include())")

# Raylib configuration (using project local raylib)
ifeq ($(UNAME_S), Darwin)    # macOS
    RAYLIB_INCLUDE = -I../../../raylib-5.5_macos/include
    RAYLIB_LIB = -L../../../raylib-5.5_macos/lib -lraylib
    PLATFORM_LIBS = -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
    DYLIB_PATH = DYLD_LIBRARY_PATH=../../../raylib-5.5_macos/lib
    SO_EXT = .so
else ifeq ($(UNAME_S), Linux)    # Linux
    RAYLIB_INCLUDE = -I/usr/local/include -I/usr/include
    RAYLIB_LIB = -L/usr/local/lib -L/usr/lib -lraylib
    PLATFORM_LIBS = -lGL -lm -lpthread -ldl -lrt -lX11
    DYLIB_PATH = LD_LIBRARY_PATH=/usr/local/lib:/usr/lib
    SO_EXT = .so
else
    $(error Unsupported platform: $(UNAME_S))
endif

# Include directories
INCLUDES = -I. -I$(PYTHON_INCLUDE) -I$(NUMPY_INCLUDE) $(RAYLIB_INCLUDE)

# Source files
CORE_SOURCES = tendril_core.c render_2d.c
BINDING_SOURCES = $(CORE_SOURCES) binding_clean.c
DEMO_SOURCES = $(CORE_SOURCES) demo_main.c

# Target names
BINDING_TARGET = binding$(SO_EXT)
DEMO_TARGET = tendril_demo_clean

# Default target
all: $(BINDING_TARGET) $(DEMO_TARGET)

# Python extension module
$(BINDING_TARGET): $(BINDING_SOURCES) tendril_clean.h
	@echo "🔨 Building Python extension: $(BINDING_TARGET)"
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(INCLUDES) -shared -o $@ $(BINDING_SOURCES) $(RAYLIB_LIB) $(PLATFORM_LIBS) -lpython$(PYTHON_VERSION)

# Standalone demo
$(DEMO_TARGET): $(DEMO_SOURCES) tendril_clean.h
	@echo "🔨 Building demo executable: $(DEMO_TARGET)"
	$(CC) $(CFLAGS) $(RELEASE_FLAGS) $(INCLUDES) -o $@ $(DEMO_SOURCES) $(RAYLIB_LIB) $(PLATFORM_LIBS)

# Debug builds
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(BINDING_TARGET) $(DEMO_TARGET)

# Test compilation only (no linking)
test-compile: tendril_clean.h
	@echo "🧪 Testing compilation of all source files..."
	$(CC) $(CFLAGS) $(INCLUDES) -c tendril_core.c -o tendril_core.o
	$(CC) $(CFLAGS) $(INCLUDES) -c render_2d.c -o render_2d.o
	$(CC) $(CFLAGS) $(INCLUDES) -c binding_clean.c -o binding_clean.o
	$(CC) $(CFLAGS) $(INCLUDES) -c demo_main.c -o demo_main.o
	@echo "✅ All source files compile successfully"

# Run demo
run-demo: $(DEMO_TARGET)
	@echo "🚀 Running clean architecture demo..."
	$(DYLIB_PATH) ./$(DEMO_TARGET)

# Test Python import
test-python: $(BINDING_TARGET)
	@echo "🐍 Testing Python import..."
	python3 -c "import binding; print('✅ Python binding import successful')"

# Install (copy to parent directory for PufferLib)
install: $(BINDING_TARGET)
	@echo "📦 Installing binding to parent directory..."
	cp $(BINDING_TARGET) ../binding.so
	@echo "✅ Installation complete"

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	rm -f $(BINDING_TARGET) $(DEMO_TARGET) *.o *.so
	@echo "✅ Clean complete"

# Show configuration
config:
	@echo "🔧 Build Configuration:"
	@echo "   Platform: $(UNAME_S)"
	@echo "   Compiler: $(CC)"
	@echo "   Python Version: $(PYTHON_VERSION)"
	@echo "   Python Include: $(PYTHON_INCLUDE)"
	@echo "   NumPy Include: $(NUMPY_INCLUDE)"
	@echo "   Raylib Include: $(RAYLIB_INCLUDE)"
	@echo "   Raylib Library: $(RAYLIB_LIB)"
	@echo "   Platform Libraries: $(PLATFORM_LIBS)"
	@echo "   Extension: $(SO_EXT)"

# Help
help:
	@echo "🎯 Tendril Clean Architecture Build System"
	@echo ""
	@echo "Available targets:"
	@echo "   all           - Build both Python extension and demo (default)"
	@echo "   binding.so    - Build Python extension module only"
	@echo "   demo          - Build standalone demo executable only"
	@echo "   debug         - Build with debug flags"
	@echo "   test-compile  - Test compilation without linking"
	@echo "   run-demo      - Build and run the demo"
	@echo "   test-python   - Test Python import of binding"
	@echo "   install       - Install binding to parent directory"
	@echo "   clean         - Remove all build artifacts"
	@echo "   config        - Show build configuration"
	@echo "   help          - Show this help message"
	@echo ""
	@echo "Example usage:"
	@echo "   make all      # Build everything"
	@echo "   make clean    # Clean and rebuild"
	@echo "   make debug    # Debug build"
	@echo "   make run-demo # Quick test"

.PHONY: all debug test-compile run-demo test-python install clean config help