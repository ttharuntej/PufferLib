#!/usr/bin/env python3
"""Setup script for Tendril environment Python bindings."""

import os
import sys
import platform
import subprocess
import sysconfig
from pathlib import Path
from setuptools import setup, Extension
import numpy as np

def find_raylib():
    """Find raylib installation paths."""
    # Common raylib installation locations
    search_paths = [
        "/usr/local",
        "/opt/homebrew",  # M1 Mac
        "/usr",
        "~/raylib",
        str(Path.home() / "raylib"),
    ]
    
    # Check environment variables first
    if "RAYLIB_PATH" in os.environ:
        raylib_root = Path(os.environ["RAYLIB_PATH"])
        if raylib_root.exists():
            return str(raylib_root / "include"), str(raylib_root / "lib")
    
    # Search common locations
    for path in search_paths:
        raylib_path = Path(path).expanduser()
        include_path = raylib_path / "include"
        lib_path = raylib_path / "lib"
        
        # Check if raylib.h exists
        if (include_path / "raylib.h").exists() and lib_path.exists():
            return str(include_path), str(lib_path)
    
    # Try pkg-config as fallback
    try:
        include_dirs = subprocess.check_output(
            ["pkg-config", "--cflags-only-I", "raylib"], 
            text=True
        ).strip().replace("-I", "").split()
        
        lib_dirs = subprocess.check_output(
            ["pkg-config", "--libs-only-L", "raylib"], 
            text=True
        ).strip().replace("-L", "").split()
        
        if include_dirs and lib_dirs:
            return include_dirs[0], lib_dirs[0]
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    
    return None, None

def get_platform_specific_args():
    """Get platform-specific compiler and linker arguments."""
    system = platform.system()
    
    if system == "Darwin":  # macOS
        return {
            "extra_compile_args": ["-std=c99", "-O3", "-fPIC"],
            "extra_link_args": [
                "-framework", "Cocoa", 
                "-framework", "IOKit", 
                "-framework", "CoreVideo", 
                "-framework", "OpenGL"
            ],
            "libraries": ["raylib", "m", "pthread"]
        }
    elif system == "Linux":
        return {
            "extra_compile_args": ["-std=c99", "-O3", "-fPIC"],
            "extra_link_args": [],
            "libraries": ["raylib", "m", "pthread", "dl", "X11", "GL"]
        }
    elif system == "Windows":
        return {
            "extra_compile_args": ["/O2"],
            "extra_link_args": [],
            "libraries": ["raylib", "winmm", "gdi32", "opengl32"]
        }
    else:
        # Default fallback
        return {
            "extra_compile_args": ["-std=c99", "-O3", "-fPIC"],
            "extra_link_args": [],
            "libraries": ["raylib", "m", "pthread"]
        }

def main():
    # Check for headless mode
    headless = "--headless" in sys.argv or os.environ.get("TENDRIL_HEADLESS", "").lower() in ("1", "true", "yes")
    if "--headless" in sys.argv:
        sys.argv.remove("--headless")
    
    if headless:
        print("Building in headless mode (no raylib required)")
        raylib_include, raylib_lib = None, None
    else:
        # Find raylib
        raylib_include, raylib_lib = find_raylib()
        
        if not raylib_include or not raylib_lib:
            print("ERROR: Could not find raylib installation!")
            print("Please install raylib or set RAYLIB_PATH environment variable.")
            print("On macOS: brew install raylib")
            print("On Ubuntu: sudo apt install libraylib-dev")
            print("Or use --headless flag for training-only build")
            sys.exit(1)
    
    if not headless:
        print(f"Found raylib:")
        print(f"  Include: {raylib_include}")
        print(f"  Library: {raylib_lib}")
    
    # Get Python and NumPy include directories
    python_include = sysconfig.get_paths()["include"]
    numpy_include = np.get_include()
    
    # Setup include directories
    include_dirs = [
        python_include,
        numpy_include,
        "."  # Current directory for tendril.h
    ]
    
    # Setup library directories and platform args based on mode
    if headless:
        # Headless mode - no raylib
        library_dirs = []
        define_macros = [("TENDRIL_DEFER_RESET", "1")]
        libraries = []
        extra_compile_args = ["-O3", "-fPIC"]
        extra_link_args = []
    else:
        # With raylib visualization
        platform_args = get_platform_specific_args()
        include_dirs.append(raylib_include)
        library_dirs = [raylib_lib]
        define_macros = [
            ("TENDRIL_WITH_RAYLIB", "1"),
            ("PLATFORM_DESKTOP", "1"),
            ("TENDRIL_DEFER_RESET", "1")
        ]
        libraries = platform_args["libraries"]
        extra_compile_args = platform_args["extra_compile_args"]
        extra_link_args = platform_args["extra_link_args"]
    
    # Create extension
    extension = Extension(
        name="binding",
        sources=["binding.c"],
        include_dirs=include_dirs,
        library_dirs=library_dirs,
        libraries=libraries,
        define_macros=define_macros,
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args
    )
    
    # Setup configuration
    setup(
        name="tendril-binding",
        version="1.0.0",
        description="Tendril laser pointer environment C bindings",
        author="PufferLib Ocean",
        ext_modules=[extension],
        zip_safe=False,
        python_requires=">=3.8",
        install_requires=[
            "numpy>=1.19.0"
        ]
    )

if __name__ == "__main__":
    main()