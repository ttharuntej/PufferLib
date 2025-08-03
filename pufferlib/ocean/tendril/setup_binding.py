#!/usr/bin/env python3
"""
Setup script to compile tendril C bindings for Python
"""

from distutils.core import setup, Extension
import numpy as np
import os

# Get paths
current_dir = os.path.dirname(os.path.abspath(__file__))
raylib_include = "/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/include"
raylib_lib = "/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/lib"

# Define the extension module
binding_module = Extension(
    'binding',
    sources=['binding.c'],
    include_dirs=[
        np.get_include(),
        raylib_include,
        current_dir
    ],
    library_dirs=[raylib_lib],
    libraries=['raylib'],
    extra_compile_args=['-DPLATFORM_DESKTOP'],
    extra_link_args=[
        '-framework', 'Cocoa',
        '-framework', 'IOKit', 
        '-framework', 'CoreVideo',
        '-framework', 'OpenGL',
        f'-Wl,-rpath,{raylib_lib}'
    ]
)

if __name__ == '__main__':
    setup(
        name='tendril_binding',
        version='1.0',
        description='PufferLib Tendril Environment C Bindings',
        ext_modules=[binding_module]
    )