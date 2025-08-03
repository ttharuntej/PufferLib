"""
Utility functions for C extension compilation setup
"""

import os
import platform
import subprocess
import numpy

def is_macos():
    """Check if running on macOS"""
    return platform.system() == 'Darwin'

def is_linux():
    """Check if running on Linux"""
    return platform.system() == 'Linux'

def get_numpy_include():
    """Get numpy include directory"""
    return numpy.get_include()

def find_raylib_installation():
    """Find raylib installation path"""
    possible_paths = []
    
    if is_macos():
        # Common homebrew paths
        possible_paths.extend([
            '/opt/homebrew',  # Apple Silicon
            '/usr/local',     # Intel Mac
        ])
        
        # Check if raylib is installed via homebrew
        try:
            result = subprocess.run(['brew', '--prefix', 'raylib'], 
                                  capture_output=True, text=True, timeout=10)
            if result.returncode == 0:
                possible_paths.insert(0, result.stdout.strip())
        except:
            pass
            
    elif is_linux():
        possible_paths.extend([
            '/usr',
            '/usr/local',
        ])
    
    # Check each path for raylib
    for path in possible_paths:
        include_path = os.path.join(path, 'include', 'raylib.h')
        lib_path = os.path.join(path, 'lib')
        
        if os.path.exists(include_path) and os.path.exists(lib_path):
            return path
    
    return None

def get_platform_compile_args():
    """Get platform-specific compile arguments"""
    args = []
    
    if is_macos():
        args.extend(['-Wno-deprecated-declarations'])
    elif is_linux():
        args.extend(['-fPIC'])
        
    return args

def get_platform_link_args():
    """Get platform-specific link arguments"""
    args = []
    
    if is_macos():
        # macOS frameworks for raylib
        args.extend([
            '-framework', 'OpenGL',
            '-framework', 'Cocoa', 
            '-framework', 'IOKit',
            '-framework', 'CoreFoundation',
            '-framework', 'CoreVideo'
        ])
    elif is_linux():
        # Linux libraries for raylib
        args.extend(['-lGL', '-lm', '-lpthread', '-ldl', '-lrt', '-lX11'])
        
    return args