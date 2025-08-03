#!/usr/bin/env python3
"""
Read STL files to get actual hardware dimensions for accurate simulation
"""

import struct
import numpy as np

def read_stl_binary(filename):
    """Read binary STL file and return vertices"""
    with open(filename, 'rb') as f:
        # Skip 80-byte header
        f.read(80)
        
        # Read number of triangles
        num_triangles = struct.unpack('<I', f.read(4))[0]
        
        vertices = []
        for _ in range(num_triangles):
            # Skip normal vector (3 floats)
            f.read(12)
            
            # Read 3 vertices (9 floats total)
            for _ in range(3):
                x, y, z = struct.unpack('<fff', f.read(12))
                vertices.append([x, y, z])
            
            # Skip attribute byte count
            f.read(2)
    
    return np.array(vertices)

def analyze_stl_dimensions(filename):
    """Analyze STL file and return bounding box dimensions"""
    try:
        vertices = read_stl_binary(filename)
        
        # Calculate bounding box
        min_coords = np.min(vertices, axis=0)
        max_coords = np.max(vertices, axis=0)
        dimensions = max_coords - min_coords
        
        print(f"\n📏 {filename}")
        print(f"   Vertices: {len(vertices)}")
        print(f"   Min: [{min_coords[0]:.2f}, {min_coords[1]:.2f}, {min_coords[2]:.2f}] mm")
        print(f"   Max: [{max_coords[0]:.2f}, {max_coords[1]:.2f}, {max_coords[2]:.2f}] mm")
        print(f"   Dimensions: [{dimensions[0]:.2f} × {dimensions[1]:.2f} × {dimensions[2]:.2f}] mm")
        
        return {
            'min': min_coords,
            'max': max_coords, 
            'dimensions': dimensions,
            'vertices': vertices
        }
        
    except Exception as e:
        print(f"❌ Error reading {filename}: {e}")
        return None

if __name__ == "__main__":
    print("🔧 Reading STL Hardware Specifications")
    print("=" * 50)
    
    stl_files = [
        "pufferlib/resources/tendril/Base.stl",
        "pufferlib/resources/tendril/Segment.stl", 
        "pufferlib/resources/tendril/End_Cap.stl"
    ]
    
    dimensions = {}
    for stl_file in stl_files:
        result = analyze_stl_dimensions(stl_file)
        if result:
            name = stl_file.split('/')[-1].replace('.stl', '')
            dimensions[name] = result
    
    print("\n🎯 Hardware Construction Analysis")
    print("=" * 50)
    
    if 'Base' in dimensions:
        base_dims = dimensions['Base']['dimensions']
        print(f"Base Platform: {base_dims[0]:.1f} × {base_dims[1]:.1f} × {base_dims[2]:.1f} mm")
    
    if 'Segment' in dimensions:
        seg_dims = dimensions['Segment']['dimensions']  
        print(f"Segment: {seg_dims[0]:.1f} × {seg_dims[1]:.1f} × {seg_dims[2]:.1f} mm")
    
    if 'End_Cap' in dimensions:
        cap_dims = dimensions['End_Cap']['dimensions']
        print(f"End Cap: {cap_dims[0]:.1f} × {cap_dims[1]:.1f} × {cap_dims[2]:.1f} mm")
    
    print("\n🔗 Kinematic Chain:")
    print("   Base.stl → Servo1(yaw) → Segment.stl → Servo2(pitch) → Segment.stl → Servo3(pitch) → End_Cap.stl")
    print("\n✅ Ready to update simulation with exact hardware dimensions!")