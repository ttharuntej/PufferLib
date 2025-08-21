'''PufferLib Tendril Environment - 3-joint articulated tendril for RL training

Based on mover.md specifications:
- 3-joint tendril with servo actuation
- Hardware dimensions from STL files (Base: 50x50x15mm, Segments: 25x21x30mm)
- Mouse cursor following task
- Real-to-sim data collection ready
'''

import numpy as np
import gymnasium

import pufferlib
try:
    from . import binding
except ImportError:
    # Fallback for when running from different directory
    import binding

class Tendril(pufferlib.PufferEnv):
    def __init__(self, num_envs=1, render_mode='human', report_interval=1, buf=None, seed=0):
        """Initialize PufferLib Tendril Environment
        
        Args:
            num_envs: Number of parallel environments
            render_mode: 'human' for 3D visualization, None for headless
            report_interval: Logging frequency
            buf: Optional pre-allocated buffer
            seed: Random seed
        """
        # Observation space: [joint_sin_cos(6), end_pos(3), target_pos(3), joint_vels(3), pointing_dir(3), angular_error(1), stability_timer(1)] = 20D
        self.single_observation_space = gymnasium.spaces.Box(
            low=-1.0, high=1.0, shape=(20,), dtype=np.float32
        )
        
        # Action space: [joint_angle_deltas(3)] = 3D  
        self.single_action_space = gymnasium.spaces.Box(
            low=-1.0, high=1.0, shape=(3,), dtype=np.float32
        )
        
        self.render_mode = render_mode
        self.num_agents = num_envs
        self.report_interval = report_interval
        
        super().__init__(buf)
        self.actions = np.zeros((num_envs, 3), dtype=np.float32)

        self.c_envs = binding.vec_init(
            self.observations,
            self.actions,
            self.rewards,
            self.terminals,
            self.truncations,
            num_envs,
            seed,
        )
    
    def reset(self, seed=None):
        """Reset environment(s) to initial state"""
        if seed is not None:
            binding.vec_reset(self.c_envs, seed)
        else:
            binding.vec_reset(self.c_envs, 0)
        return self.observations, []
    
    def _canonicalize_info(self, info: dict) -> dict:
        """Normalize metric names and ensure plain Python floats."""
        if not isinstance(info, dict):
            return {}

        # Map synonyms -> canonical keys expected in dashboards
        if 'd_perp_mm_mean' not in info and 'miss_distance_mean_mm' in info:
            info['d_perp_mm_mean'] = info['miss_distance_mean_mm']

        # Ensure JSON-serializable scalars
        clean = {}
        for k, v in info.items():
            if isinstance(v, (int, float)):
                clean[k] = float(v)
            elif hasattr(v, 'item'):  # numpy scalar
                try:
                    clean[k] = float(v.item())
                except Exception:
                    pass
        return clean

    def step(self, actions):
        """Execute one environment step"""
        actions = np.asarray(actions, dtype=np.float32)
        actions = np.clip(actions, -1.0, 1.0)

        self.actions[:] = actions
        binding.vec_step(self.c_envs)

        # 1) Pull metrics from C
        info = binding.vec_log(self.c_envs)

        # 2) Canonicalize names and make plain floats
        info = self._canonicalize_info(info)

        # 3) IMPORTANT: make it a list-of-dicts for vectorized logging
        if not isinstance(info, (list, tuple)):
            infos = [info for _ in range(self.num_agents)]
        else:
            # If your binding ever returns per-env info, still ensure right length
            infos = list(info)
            if len(infos) != self.num_agents:
                infos = (infos * self.num_agents)[:self.num_agents]

        # Optional debug (kept)
        self._step_count = getattr(self, '_step_count', 0) + 1
        
        # SANITY CHECK: Verify episodes are resetting (debug termination issues)
        if self._step_count % 2000 == 0:
            print(f"[terms] {int(self.terminals.sum())} [truncs] {int(self.truncations.sum())}")
            print(f"[episodes] {infos[0].get('episodes', 0)}")
        
        # SANITY CHECK: Verify correct binding is loaded (temporary debug)
        if self._step_count % 500 == 0:
            print("[binding?]", "tendril_binding_version" in infos[0], infos[0].get("tendril_binding_version"))
        if self._step_count % 1000 == 0:
            print(f"[tendril.py] Step {self._step_count}, info[0] keys: {list(infos[0].keys())}")
            if 'hit_rate' in infos[0]:
                print(f"[tendril.py] hit_rate={infos[0].get('hit_rate')}, angular_error_deg_mean={infos[0].get('angular_error_deg_mean')}")
            print(f"[tendril.py] Full info[0]: {infos[0]}")
            import sys; sys.stdout.flush()

        return (self.observations, self.rewards, self.terminals, self.truncations, infos)
    
    def render(self):
        """Render environment visualization"""
        if self.render_mode == 'human':
            binding.vec_render(self.c_envs, 0)
    
    def close(self):
        """Clean up environment resources"""
        if hasattr(self, "c_envs"):
            if isinstance(self.c_envs, int):
                binding.vec_close(self.c_envs)
            elif isinstance(self.c_envs, (list, tuple)):
                for c_env in self.c_envs:
                    binding.env_close(c_env)
    
    def get_hardware_specs(self):
        """Return hardware specifications for real-world deployment"""
        return {
            'num_joints': 3,
            'joint_range_deg': [0, 180],
            'joint_range_rad': [0, np.pi],
            'segment_length_mm': 30.0,
            'base_dimensions_mm': [50, 50, 15],
            'segment_dimensions_mm': [25, 21, 30],
            'workspace_size_mm': 100.0,
            'control_frequency_hz': 50,
            'servo_type': 'SG90',
            'microcontroller': 'ESP32'
        }
    
    def set_target_position(self, target_pos):
        """Set target position for cursor following (for interactive use)
        
        Args:
            target_pos: [x, y, z] coordinates in mm
        """
        # This would be implemented in the C binding for interactive control
        # For now, targets are set randomly in c_reset()
        pass

# Convenience function for creating environment
def make_env(num_envs=1, **kwargs):
    """Create Tendril environment with default settings"""
    return Tendril(num_envs=num_envs, **kwargs)

# Demo script
if __name__ == '__main__':
    import time
    
    print("PufferLib Tendril Environment Demo")
    print("Hardware-based 3-joint tendril simulation")
    print("-" * 40)
    
    # Create environment
    env = Tendril(num_envs=1, render_mode='human')
    specs = env.get_hardware_specs()
    
    print("Hardware Specifications:")
    for key, value in specs.items():
        print(f"  {key}: {value}")
    
    print("\\nStarting demo with random actions...")
    print("Close window or press ESC to exit")
    
    # Reset environment
    obs, info = env.reset()
    
    steps = 0
    episodes = 0
    start_time = time.time()
    
    try:
        while episodes < 10:  # Run 10 episodes
            # Generate random actions (smooth movements)
            actions = np.random.normal(0, 0.3, size=(1, 3))
            actions = np.clip(actions, -1.0, 1.0)
            
            # Step environment
            obs, rewards, terminals, truncations, info = env.step(actions)
            
            # Render
            env.render()
            
            steps += 1
            
            # Check for episode end
            if terminals[0] or truncations[0]:
                episodes += 1
                print(f"Episode {episodes} completed in {steps} steps, reward: {rewards[0]:.2f}")
                obs, info = env.reset()
                steps = 0
            
            # Small delay for visualization
            time.sleep(0.02)  # 50 FPS
            
    except KeyboardInterrupt:
        print("\\nDemo interrupted by user")
    
    finally:
        elapsed = time.time() - start_time
        print(f"\\nDemo completed in {elapsed:.2f} seconds")
        env.close()
        
    print("\\nReady for RL training!")
    print("Use: puffer train tendril --config pufferlib/config/ocean/tendril.ini")