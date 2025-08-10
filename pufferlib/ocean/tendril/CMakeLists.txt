# PufferLib Tendril Environment - Clean Multi-File Architecture
# CMake build system for the refactored tendril environment

cmake_minimum_required(VERSION 3.16)
project(tendril_clean C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Python3 COMPONENTS Interpreter Development NumPy REQUIRED)

# Find raylib (adjust path as needed for your system)
find_path(RAYLIB_INCLUDE_DIR raylib.h 
    PATHS /opt/homebrew/include /usr/local/include /usr/include
    PATH_SUFFIXES raylib
)

find_library(RAYLIB_LIBRARY raylib
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)

# Set compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O3 -fPIC")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${Python3_INCLUDE_DIRS})
include_directories(${Python3_NumPy_INCLUDE_DIRS})
include_directories(${RAYLIB_INCLUDE_DIR})
include_directories(.)

# Source files for the clean architecture
set(TENDRIL_CORE_SOURCES
    tendril_core.c
    render_2d.c
)

set(TENDRIL_BINDING_SOURCES
    ${TENDRIL_CORE_SOURCES}
    binding_clean.c
)

# Create the Python extension module
add_library(binding SHARED ${TENDRIL_BINDING_SOURCES})

# Set extension properties
set_target_properties(binding PROPERTIES
    PREFIX ""
    SUFFIX ".so"
    OUTPUT_NAME "binding"
    LINKER_LANGUAGE C
)

# Link libraries
target_link_libraries(binding 
    ${Python3_LIBRARIES}
    ${RAYLIB_LIBRARY}
)

# macOS specific linking (adjust for your platform)
if(APPLE)
    target_link_libraries(binding
        "-framework Cocoa"
        "-framework IOKit" 
        "-framework CoreVideo"
        "-framework OpenGL"
    )
endif()

# Linux specific linking
if(UNIX AND NOT APPLE)
    target_link_libraries(binding
        GL
        m
        pthread
        dl
        rt
        X11
    )
endif()

# Installation (optional)
install(TARGETS binding
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

# Build standalone demo (optional)
add_executable(tendril_demo_clean
    ${TENDRIL_CORE_SOURCES}
    demo_main.c
)

target_link_libraries(tendril_demo_clean 
    ${RAYLIB_LIBRARY}
    m
)

if(APPLE)
    target_link_libraries(tendril_demo_clean
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo" 
        "-framework OpenGL"
    )
endif()

# Print configuration info
message(STATUS "Python3 found: ${Python3_FOUND}")
message(STATUS "Python3 include: ${Python3_INCLUDE_DIRS}")
message(STATUS "NumPy include: ${Python3_NumPy_INCLUDE_DIRS}")
message(STATUS "Raylib include: ${RAYLIB_INCLUDE_DIR}")
message(STATUS "Raylib library: ${RAYLIB_LIBRARY}")