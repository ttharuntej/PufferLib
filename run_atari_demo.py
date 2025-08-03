#!/usr/bin/env python3

import time
try:
    import cv2
    opencv_available = True
except ImportError:
    opencv_available = False
    print("OpenCV not available, will use text mode")

import pufferlib.environments.atari as atari_env

def run_atari_pong(render_mode='rgb_array'):
    """Run Atari Pong with visual display"""
    print(f"Starting Atari Pong in {render_mode} mode...")
    
    # Create the environment
    env_creator = atari_env.env_creator('pong')
    if render_mode == 'human':
        env = env_creator(render_mode='human')
    else:
        env = env_creator(render_mode='rgb_array')
    
    # Reset the environment
    obs, info = env.reset()
    print(f"Environment created. Observation shape: {obs.shape}")
    print(f"Action space: {env.action_space}")
    
    terminal = False
    truncated = False
    step_count = 0
    
    try:
        while True:
            if terminal or truncated:
                print(f"Episode ended after {step_count} steps. Resetting...")
                obs, info = env.reset()
                step_count = 0
            
            # Take a random action
            action = env.action_space.sample()
            obs, reward, terminal, truncated, info = env.step(action)
            step_count += 1
            
            # Render the environment
            if render_mode == 'rgb_array' and opencv_available:
                frame = env.render()
                if frame is not None:
                    # Convert RGB to BGR for OpenCV
                    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
                    cv2.imshow('Atari Pong', frame)
                    
                    # Exit if 'q' is pressed
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
            elif render_mode == 'human':
                env.render()
            else:
                # Just print some info without visual display
                if step_count % 60 == 0:  # Print every 60 steps
                    print(f"Step {step_count}, Reward: {reward}, Terminal: {terminal}")
            
            # Small delay to control speed
            time.sleep(0.016)  # ~60 FPS
            
            # Auto-exit after a while for demo purposes
            if step_count > 3000:
                print("Demo complete!")
                break
                
    except KeyboardInterrupt:
        print("Interrupted by user")
    finally:
        if opencv_available:
            cv2.destroyAllWindows()
        env.close()

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1 and sys.argv[1] == 'human':
        run_atari_pong('human')
    else:
        run_atari_pong('rgb_array')