# File Structure Mapping: Flappy vs Tendril

## Your Flappy Bird Environment (Python-only)

**Single File Approach:**
```
pufferlib/environments/flappy/environment.py (121 lines)
├── Import statements (lines 1-5)
├── Environment creator functions (lines 6-11) 
├── Action constants (lines 14-16)
├── class Flappy(pufferlib.PufferEnv):
│   ├── __init__() method (lines 22-49)      → Game setup, spaces definition
│   ├── reset() method (lines 52-62)         → Initialize game state  
│   ├── step() method (lines 65-91)          → Game logic, rewards, termination
│   ├── render() method (lines 94-118)       → ASCII visualization
│   └── close() method (lines 120-121)       → Cleanup
```

**What Each Section Does:**
- **Lines 1-16**: Imports and helper functions
- **Lines 22-49**: Constructor - defines observation/action spaces, initializes game state
- **Lines 52-62**: Reset - agent position to 0, update observations array
- **Lines 65-91**: Step - process action, update position, calculate reward, check termination
- **Lines 94-118**: Render - create ASCII grid representation
- **Lines 120-121**: Close - cleanup (empty in this case)

## Our Tendril Environment (C+Python)

**Multi-File Approach:**

### File 1: `tendril.h` (C Interface - 7,100 lines)
```c
// Hardware specifications from your STL files
#define BASE_WIDTH 50.0f        // Your base.stl dimensions
#define SEGMENT_LENGTH 30.0f    // Your segment.stl dimensions  
#define NUM_JOINTS 3            // 3-joint tendril

// Main structure (equivalent to your Flappy's self.agent_pos)
typedef struct Tendril {
    float joint_angles[3];       // Current servo positions
    float joint_velocities[3];   // Joint speeds
    float target_pos[3];         // Target XYZ coordinates
    float end_effector_pos[3];   // Tip position
    float* observations;         // Python interface
    float* actions;              // Python interface
    // ... hardware calibration parameters
} Tendril;

// Function declarations (equivalent to your Flappy's methods)
void c_reset(Tendril* env);     // → Your reset() method
void c_step(Tendril* env);      // → Your step() method  
void c_render(Tendril* env);    // → Your render() method
```

### File 2: `tendril.c` (C Implementation - 11,778 lines)
```c
// Forward kinematics (replaces your simple position update)
void compute_forward_kinematics(Tendril* env) {
    // Calculate end effector position from joint angles
    // This is like your: self.agent_pos = 0 or 1
    // But with complex 3D math instead of simple assignment
}

// Reset function (equivalent to your reset() method)
void c_reset(Tendril* env) {
    // Initialize joint angles to center positions
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_angles[i] = JOINT_LIMIT_RAD / 2.0f; // 90 degrees
    }
    // Generate random target
    env->target_pos[0] = randf(-WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
    // Update observations
    compute_forward_kinematics(env);
    compute_observations(env);
}

// Step function (equivalent to your step() method)  
void c_step(Tendril* env) {
    // Apply servo actions (like your UP/DOWN logic)
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_delta = env->actions[i] * (5.0f * M_PI / 180.0f);
        env->joint_angles[i] += angle_delta;
        // Enforce servo limits (0-180°)
        env->joint_angles[i] = clampf(env->joint_angles[i], 0, JOINT_LIMIT_RAD);
    }
    
    // Update physics
    compute_forward_kinematics(env);
    
    // Calculate reward (like your self.rewards[0] = 0)
    float reward = compute_reward(env);
    env->rewards[0] = reward;
    
    // Check termination (like your self.terminals[0] = True)
    bool terminated = check_if_target_reached(env);
    env->terminals[0] = terminated ? 1 : 0;
}

// Render function (equivalent to your render() method)
void c_render(Tendril* env) {
    // 3D graphics instead of ASCII
    BeginDrawing();
    ClearBackground(PUFF_BACKGROUND);
    
    // Draw 3D coordinate axes
    DrawLine3D((Vector3){-50, 0, 0}, (Vector3){50, 0, 0}, PUFF_RED);
    
    // Draw tendril segments (like your grid[agent_pos][0] = 'A')
    Vector3 current_pos = {0, 0, BASE_DEPTH};
    for (int i = 0; i < NUM_JOINTS; i++) {
        // Calculate segment position using forward kinematics
        Vector3 segment_end = calculate_segment_end(current_pos, env->joint_angles[i]);
        DrawCylinderEx(current_pos, segment_end, 3.0f, 3.0f, 8, PUFF_CYAN);
        current_pos = segment_end;
    }
    
    // Draw target (like your grid visualization)
    DrawSphere(target_position, 4.0f, PUFF_RED);
    
    EndDrawing();
}
```

### File 3: `tendril.py` (Python Wrapper - 6,125 lines)
```python
class Tendril(pufferlib.PufferEnv):
    def __init__(self, num_envs=1, render_mode='human', ...):
        # Same structure as your Flappy __init__
        self.single_observation_space = gymnasium.spaces.Box(
            low=-1.0, high=1.0, shape=(12,), dtype=np.float32  # More complex than your (1,)
        )
        self.single_action_space = gymnasium.spaces.Box(
            low=-1.0, high=1.0, shape=(3,), dtype=np.float32   # Continuous vs your Discrete(2)
        )
        
        # Instead of self.agent_pos = 0, we create C environments
        c_envs = []
        for i in range(num_envs):
            c_envs.append(binding.env_init(
                self.observations[i],    # Connect to C memory
                self.actions[i],         # Connect to C memory  
                self.rewards[i],         # Connect to C memory
                ...
            ))
        self.c_envs = binding.vectorize(*c_envs)
    
    def reset(self, seed=None):
        # Instead of: self.agent_pos = 0
        binding.vec_reset(self.c_envs, seed)  # Calls c_reset()
        return self.observations, []
    
    def step(self, actions):
        # Instead of: if action == UP: self.agent_pos = 0
        self.actions[:] = actions
        binding.vec_step(self.c_envs)  # Calls c_step()
        
        # C code automatically updates:
        # - self.observations (via shared memory)
        # - self.rewards (via shared memory)  
        # - self.terminals (via shared memory)
        
        return (self.observations, self.rewards, 
                self.terminals, self.truncations, info)
    
    def render(self):
        # Instead of: return ascii_grid_string
        binding.vec_render(self.c_envs)  # Calls c_render() for 3D graphics
```

### File 4: `binding_simple.c` (Python-C Bridge - 83 lines)
```c
// This file connects Python calls to C functions
// Currently placeholder, needs to call real C functions

static PyObject* vec_step(PyObject* self, PyObject* args) {
    // CURRENT (placeholder):
    Py_RETURN_NONE;
    
    // NEEDED (real implementation):
    // VectorizedTendril* vec = extract_from_args(args);
    // for (int i = 0; i < vec->num_envs; i++) {
    //     c_step(vec->envs[i]);  // Call your C function!
    // }
    // Py_RETURN_NONE;
}
```

## Key Mapping:

| Flappy Python | Tendril C | Tendril Python | Purpose |
|---------------|-----------|----------------|---------|
| `self.agent_pos = 0` | `env->joint_angles[3]` | `self.observations` | State storage |
| `if action == UP:` | `float angle_delta = actions[i]` | `self.actions[:]` | Action processing |
| `self.terminals[0] = True` | `env->terminals[0] = 1` | `self.terminals` | Episode end |
| `grid[pos][0] = 'A'` | `DrawSphere(pos, radius, color)` | `binding.vec_render()` | Visualization |
| 121 lines total | 18,878 lines total | 6,125 lines | Complexity scale |

## The Same Foundation:

Both environments have **identical PufferLib structure**:
```python
# Your Flappy AND Our Tendril:
class MyEnv(pufferlib.PufferEnv):
    def __init__(self): # Define spaces, initialize state
    def reset(self):    # Reset to initial state  
    def step(self):     # Process action, update state, calculate reward
    def render(self):   # Show current state
    def close(self):    # Cleanup resources
```

**The difference is WHERE the logic lives:**
- **Flappy**: All logic in Python methods
- **Tendril**: Logic in C functions, Python just calls them