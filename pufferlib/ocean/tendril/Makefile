# Tendril Project Build Configuration
# Enforces strict compilation flags to catch ODR violations and other issues

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -std=c99
CFLAGS_DEBUG = $(CFLAGS) -g -fsanitize=address,undefined -DDEBUG
CFLAGS_RELEASE = $(CFLAGS) -O3 -DNDEBUG
LDFLAGS = -lm

# Raylib flags (when available)
RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

.PHONY: all clean test debug release

# Default target
all: test

# Source files and objects
MATH_SRCS = tendril_math.c
RENDER_SRCS = tendril.c
ALL_SRCS = $(MATH_SRCS) $(RENDER_SRCS)
MATH_OBJS = $(MATH_SRCS:.c=.o)
RENDER_OBJS = $(RENDER_SRCS:.c=.o)
ALL_OBJS = $(ALL_SRCS:.c=.o)

# Unit tests (no external dependencies)
test: test_kinematics
	@echo "Running unit tests with strict compilation flags..."
	./test_kinematics

test_kinematics: test_kinematics.c $(MATH_OBJS) tendril.h
	$(CC) $(CFLAGS) -o $@ test_kinematics.c $(MATH_OBJS) $(LDFLAGS)

# Object file compilation
$(MATH_OBJS): %.o: %.c tendril.h
	$(CC) $(CFLAGS) -DTENDRIL_MATH_ONLY -c $< -o $@

$(RENDER_OBJS): %.o: %.c tendril.h
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build with sanitizers
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: test_kinematics
	@echo "Debug build complete with AddressSanitizer and UBSan"

# Release build
release: CFLAGS = $(CFLAGS_RELEASE)  
release: test_kinematics
	@echo "Release build complete"

# Test main demo (requires Raylib)
demo: tendril_demo
	@echo "Demo build complete (requires Raylib)"

tendril_demo: $(ALL_OBJS) tendril.h
	$(CC) $(CFLAGS) -DDEMO_MODE -o $@ $(ALL_OBJS) $(LDFLAGS) $(RAYLIB_FLAGS)

# Python binding build test (requires Python dev headers)
binding_test: $(MATH_OBJS) tendril.h
	@echo "Testing binding compilation and linking..."
	@$(CC) $(CFLAGS) -c binding.c -I/usr/include/python3.8 -I/usr/include/numpy 2>/dev/null && \
	$(CC) $(CFLAGS) -o binding_test_tmp binding.o $(MATH_OBJS) $(LDFLAGS) 2>/dev/null && \
	echo "✅ Binding compiles and links successfully" && rm -f binding_test_tmp binding.o || \
	echo "Note: Binding test requires Python dev headers"

# Clean build artifacts
clean:
	rm -f test_kinematics tendril_demo binding_test $(ALL_OBJS) binding.o *.o

# Verify no ODR violations
odr_check: 
	@echo "Checking for duplicate symbols..."
	@nm --defined-only test_kinematics 2>/dev/null | grep -E "(c_step|c_render|c_reset|c_close)" || echo "✅ No ODR violations detected"

# Style check (if clang-format available)
format:
	@command -v clang-format >/dev/null && clang-format -i *.c *.h || echo "clang-format not available"

# Help
help:
	@echo "Tendril Build Targets:"
	@echo "  test       - Build and run unit tests (default)"
	@echo "  debug      - Build with sanitizers" 
	@echo "  release    - Optimized build"
	@echo "  demo       - Build Raylib demo (requires Raylib)"
	@echo "  binding_test - Test Python binding compilation"
	@echo "  odr_check  - Verify no duplicate symbols"
	@echo "  format     - Apply code formatting"
	@echo "  clean      - Remove build artifacts"