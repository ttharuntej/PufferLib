#!/usr/bin/env python3

import time
import numpy as np

def run_cartpole_demo():
    """Run Ocean Cartpole with visualization"""
    print("Starting Ocean Cartpole demo...")
    
    try:
        # Import the cartpole environment
        import pufferlib.ocean.cartpole.cartpole as cartpole_env
        
        # Create environment with rendering
        env = cartpole_env.Cartpole(num_envs=1, render_mode='human')
        
        # Reset the environment
        obs, info = env.reset()
        print(f"Environment created. Observation shape: {obs.shape}")
        print("Use LEFT/RIGHT arrows to control. ESC to exit.")
        
        step_count = 0
        episode_count = 0
        
        while True:
            # Take random actions for demo
            actions = np.random.randint(0, 2, size=1)  # 0 or 1 (left/right)
            
            # Step the environment
            obs, rewards, terminals, truncations, info = env.step(actions)
            step_count += 1
            
            # Render the environment
            env.render()
            
            # Check if episode ended
            if terminals[0] or truncations[0]:
                episode_count += 1
                print(f"Episode {episode_count} ended after {step_count} steps")
                obs, info = env.reset()
                step_count = 0
                
                if episode_count >= 5:  # Run 5 episodes
                    break
            
            # Small delay
            time.sleep(0.02)  # 50 FPS
            
    except ImportError as e:
        print(f"Could not import cartpole environment: {e}")
        print("Trying direct C approach...")
        run_c_cartpole_demo()
    except Exception as e:
        print(f"Error running cartpole demo: {e}")
    
def run_c_cartpole_demo():
    """Run the C version directly if Python wrapper fails"""
    import os
    import subprocess
    
    print("Attempting to run C cartpole demo...")
    
    # Try to compile and run the C version
    cartpole_dir = "pufferlib/ocean/cartpole"
    if os.path.exists(cartpole_dir):
        try:
            # Simple test without the complex dependencies
            print("C demo would require compilation with raylib")
            print("For now, showing the environment structure...")
            
            # Show the observation/action space info from the header file
            with open(f"{cartpole_dir}/cartpole.h", 'r') as f:
                lines = f.readlines()
                print("\\nCartpole Environment Structure:")
                for i, line in enumerate(lines[40:60]):  # Show the struct definition
                    print(f"{i+40:3d}: {line.rstrip()}")
                    
        except Exception as e:
            print(f"Could not read cartpole files: {e}")

def run_simple_ocean_demo():
    """Try to run any available ocean environment"""
    print("Looking for available ocean environments...")
    
    import os
    ocean_dir = "pufferlib/ocean"
    
    if os.path.exists(ocean_dir):
        envs = [d for d in os.listdir(ocean_dir) 
                if os.path.isdir(os.path.join(ocean_dir, d)) and not d.startswith('__')]
        print(f"Available environments: {envs}")
        
        # Try a few simple ones
        for env_name in ['cartpole', 'grid', 'snake']:
            if env_name in envs:
                print(f"\\nTrying {env_name}...")
                try:
                    env_path = os.path.join(ocean_dir, env_name, f"{env_name}.py")
                    if os.path.exists(env_path):
                        print(f"Found {env_name}.py")
                        with open(env_path, 'r') as f:
                            first_lines = f.readlines()[:10]
                            print("Environment structure:")
                            for line in first_lines:
                                print(f"  {line.rstrip()}")
                except Exception as e:
                    print(f"Could not read {env_name}: {e}")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        if sys.argv[1] == 'cartpole':
            run_cartpole_demo()
        elif sys.argv[1] == 'c':
            run_c_cartpole_demo()
        else:
            run_simple_ocean_demo()
    else:
        print("Ocean Environment Demo")
        print("Usage:")
        print("  python run_ocean_demo.py cartpole  # Run cartpole with visualization")
        print("  python run_ocean_demo.py c         # Show C environment structure")
        print("  python run_ocean_demo.py list      # List available environments")
        print()
        run_simple_ocean_demo()