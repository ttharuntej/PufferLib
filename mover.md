# RL-Powered Tendril Project Plan - Implementation Guide
Prompt to catch up

I'm working on an RL-Powered Tendril robotics project using
  PufferLib. Please read the file mover.md in the current directory
   to understand the full project context, requirements, and
  current status.

  Key context:
  - Building a 3-joint tendril that learns to follow mouse cursor
  using RL
  - Using PufferLib ocean environment with C++ physics (like
  drone_race/cartpole)
  - Hardware: ESP32 + 3x SG90 servos (parts ordered, on the way)
  - Real-to-sim data collection approach for accurate physics 
  modeling
  - Ready to start Phase 1A: C physics simulation implementation

  After reading mover.md, confirm you understand the project and 
  tell me what we should work on next. We were about to start 
  implementing the C physics simulation files while waiting for 
  hardware delivery.




## Project Overview

**Goal**: Create a research-grade tendril system demonstrating advanced real-to-sim data collection and PufferLib integration for robust RL policy training and deployment.

**Final Deliverable**: A 3-joint articulated tendril that learns mouse cursor following through real-to-sim methodology, serving as a reference implementation for RL-driven robotics.

**Project Scope**: This document focuses on the **implementation execution** while the PRD defines the **product requirements**. Use both documents together for complete project guidance.

## PufferLib Integration Strategy

**Critical Gap Identified**: PRD doesn't specify how this integrates with existing PufferLib architecture.

### Integration Approach - OCEAN ENVIRONMENT (C++ Physics)
```python
# Following PufferLib ocean environment pattern for performance
pufferlib/ocean/tendril/
├── tendril.c                      # C physics simulation (like drone_race.c)
├── tendril.h                      # Physics engine interface  
├── tendril.py                     # Python wrapper (like drone_race.py)
├── binding.c                      # Python-C binding
├── binding.cpython-311-darwin.so  # Compiled extension
└── dronelib.h                     # Shared utilities (if needed)

pufferlib/config/ocean/
└── tendril.ini                    # Training configuration

# Real hardware interface (separate from simulation)
hardware_interface/
├── esp32_firmware/                # Arduino code
├── wifi_bridge.py                 # PC-ESP32 communication
└── hardware_controller.py         # Real robot control
```

### PufferLib API Compliance
- Must inherit from `pufferlib.PufferEnv`
- Follow existing observation/action space patterns
- Compatible with `puffer train tendril` command structure
- Integrate with existing PufferLib training pipeline

## Real-to-Sim Data Collection Specifications

**Missing from PRD**: Specific data formats and collection protocols

### Data Storage Format
```python
# HDF5 structure for efficient storage and retrieval
tendril_dataset.h5
├── /calibration/
│   ├── servo_response_curves    # [n_samples, servo_id, commanded, actual, timestamp]
│   ├── position_accuracy       # [n_samples, joint_angles, end_effector_pos]
│   └── thermal_drift          # [n_samples, temperature, position_error]
├── /dynamics/
│   ├── trajectories           # [n_episodes, timesteps, state_action_pairs]
│   ├── servo_loads           # [n_episodes, timesteps, servo_currents]
│   └── frequency_response    # [n_frequencies, amplitude, phase]
└── /episodes/
    ├── training_data         # [n_episodes, timesteps, full_state_vector]
    └── validation_data       # [n_episodes, timesteps, full_state_vector]
```

### Collection Requirements
- **Minimum dataset size**: 10,000 movement episodes
- **Sampling frequency**: 50Hz state logging
- **Calibration frequency**: Daily recalibration runs
- **Storage requirements**: ~2GB for full dataset

---

## Project Phases

## Timeline Reconciliation

**PRD Timeline**: 6 weeks methodical development  
**mover.md Timeline**: Rapid prototyping approach  
**Recommended**: Hybrid approach - solid foundation with rapid iteration cycles

---

### Phase 1: Hardware Foundation + C Physics (PRD Weeks 1-2)
**Status**: Components ordered, ready to implement C physics simulation ✅  
**Goal**: Hardware platform + initial C simulation following ocean patterns  
**Duration**: 2 weeks for robust foundation  

#### Hardware Components (Per PRD Specs)
- [x] ESP32 DevKit V1 microcontroller  
- [x] 3x SG90 9g micro servos (0-180° range, ~1° accuracy)
- [x] 5V 3A power supply
- [x] 3D printed components (PRD dimensions: 50×50×15mm base, 25×21×30mm segments)
- [x] M3 bolts, M2 screws for assembly

#### C Physics Implementation (Parallel with Hardware)
Following `drone_race.c` and `cartpole.c` patterns:
```c
// tendril.h - Physics interface
typedef struct Tendril {
    float joint_angles[3];      // Current joint positions
    float joint_velocities[3];  // Joint angular velocities  
    float target_pos[3];        // Target XYZ coordinates
    float end_effector_pos[3];  // Current end effector position
    float observations[12];     // Full observation vector
    float actions[3];           // Joint angle commands
    // Hardware calibration parameters
    float servo_backlash[3];
    float friction_coeffs[3];
    float inertia_matrix[3][3];
} Tendril;

// Core functions (matching ocean pattern)
void tendril_allocate(Tendril* env);
void tendril_reset(Tendril* env, int seed);
void tendril_step(Tendril* env);
void tendril_render(Tendril* env);
void tendril_close(Tendril* env);
```

#### Phase 1 Deliverables
- [ ] **Assembled 3-joint tentacle** with all servos mounted
- [ ] **ESP32 firmware** with basic servo control
- [ ] **Test script** confirming each joint moves independently
- [ ] **Hardware specifications document** (servo ranges, joint lengths, workspace dimensions)

#### Phase 1 Tasks
1. **Physical Assembly**
   - [ ] Mount servos in 3D printed brackets
   - [ ] Connect servo horns and linkages
   - [ ] Wire servos to ESP32 (specify pins)
   - [ ] Connect power supply

2. **Basic Firmware** (Arduino IDE)
   ```cpp
   // ESP32 Test Code Structure
   #include <Servo.h>
   
   Servo joint1, joint2, joint3;
   
   void setup() {
     // Attach servos to pins
     // Initialize WiFi for future Phase 4
   }
   
   void loop() {
     // Test sequence: move each joint through full range
     // Verify mechanical limits and constraints
   }
   ```

3. **Hardware Documentation**
   - [ ] Measure actual joint lengths
   - [ ] Record servo angle ranges (min/max degrees)
   - [ ] Document mechanical constraints
   - [ ] Calculate workspace dimensions
   - [ ] Photo documentation of assembly

---

### Phase 2: The Digital Twin (Simulation)
**Status**: Ready to Start  
**Goal**: Create physics-based simulation matching real hardware  
**Duration**: 2-3 days  

#### Phase 2 Deliverables
- [ ] **PufferLib environment** (`tentacle_env.py`) 
- [ ] **Forward kinematics simulation** matching real tentacle
- [ ] **Target reaching task** with mouse cursor input
- [ ] **Visualization tools** for debugging
- [ ] **Unit tests** verifying physics accuracy

#### Technical Architecture
```python
# File Structure
tentacle_project/
├── environments/
│   ├── tentacle_env.py          # Main PufferLib environment
│   ├── kinematics.py           # Forward/inverse kinematics
│   └── visualization.py        # Pygame/matplotlib display
├── config/
│   ├── tentacle.ini            # PufferLib training config
│   └── hardware_specs.yaml    # Real hardware parameters
└── tests/
    ├── test_kinematics.py      # Physics validation
    └── test_environment.py     # Environment testing
```

#### Environment Specifications
```python
class TentacleEnv(pufferlib.PufferEnv):
    def __init__(self):
        # Observation Space (9 dimensions):
        # [joint1_angle, joint2_angle, joint3_angle,     # Current joint positions
        #  end_x, end_y,                                  # End effector position  
        #  target_x, target_y,                           # Target position
        #  joint1_vel, joint2_vel]                       # Joint velocities
        
        # Action Space (3 dimensions):
        # [joint1_delta, joint2_delta, joint3_delta]     # Joint angle changes
        
        # Reward Function:
        # primary_reward = -distance_to_target
        # movement_penalty = -0.1 * sum(abs(action))
        # joint_limit_penalty = -10.0 if outside limits
```

#### Phase 2 Tasks
1. **Environment Implementation**
   - [ ] Create PufferLib-compatible environment class
   - [ ] Implement forward kinematics (joint angles → end position)
   - [ ] Add realistic joint limits and constraints
   - [ ] Implement reward function for target reaching

2. **Physics Validation**
   - [ ] Compare simulation to real hardware measurements
   - [ ] Validate workspace boundaries
   - [ ] Test edge cases (joint limits, unreachable targets)

3. **Visualization System**
   - [ ] Real-time display of tentacle position
   - [ ] Target visualization (mouse cursor)
   - [ ] Training progress monitoring

---

### Phase 3: The First Spark of Learning (RL Training)
**Status**: Waiting for Phase 2  
**Goal**: Train AI brain to control simulated tentacle  
**Duration**: 1-2 days  

#### Phase 3 Deliverables
- [ ] **Trained RL model** (`tentacle_model.zip`)
- [ ] **Training monitoring scripts** 
- [ ] **Performance analysis** (success rate, learning curves)
- [ ] **Model evaluation tools**

#### Training Configuration
```ini
# config/tentacle.ini
[base]
package = tentacle
env_name = tentacle_reach
policy_name = TentaclePolicy

[vec]
num_envs = 16
batch_size = 16
backend = Serial

[train]
device = cpu
total_timesteps = 1_000_000
batch_size = 2048
learning_rate = 0.0003
gamma = 0.99
clip_coef = 0.2
```

#### Phase 3 Tasks
1. **Training Pipeline Setup**
   ```bash
   # Install dependencies
   pip install -e .
   
   # Start training
   puffer train tentacle --train.total-timesteps 1000000
   ```

2. **Curriculum Learning**
   - [ ] Start with close targets (easy reach)
   - [ ] Gradually increase target distance
   - [ ] Add random target positions
   - [ ] Measure learning progression

3. **Model Evaluation**
   - [ ] Success rate on test targets
   - [ ] Average time to reach target
   - [ ] Movement efficiency metrics
   - [ ] Generalization to new target positions

---

### Phase 4: The Brain Transplant (Deployment)
**Status**: Waiting for Phase 3  
**Goal**: Deploy trained AI to physical robot  
**Duration**: 2-3 days  

#### Phase 4 Deliverables
- [ ] **PC control script** loading trained model
- [ ] **WiFi communication protocol** between PC and ESP32
- [ ] **Real-time mouse cursor following**
- [ ] **Safety systems** and error handling
- [ ] **Performance comparison** (sim vs real)

#### System Architecture
```
PC (Python) ←→ WiFi ←→ ESP32 (Arduino) ←→ Servos
    ↓
[Mouse Position] → [RL Model] → [Servo Commands] → [Physical Movement]
```

#### Communication Protocol
```python
# PC Side (tentacle_controller.py)
class TentacleController:
    def __init__(self, model_path="tentacle_model.zip", esp32_ip="192.168.1.100"):
        self.model = load_pufferlib_model(model_path)
        self.esp32 = WiFiConnection(esp32_ip)
        
    def follow_mouse(self):
        while True:
            mouse_x, mouse_y = get_mouse_position()
            target = screen_to_workspace(mouse_x, mouse_y)
            action = self.model.predict(current_state, target)
            self.send_servo_commands(action)
            time.sleep(0.05)  # 20Hz control loop
```

```cpp
// ESP32 Side (Updated Firmware)
#include <WiFi.h>
#include <WebServer.h>

void setup() {
  // Connect to WiFi
  // Initialize servos
  // Start web server for receiving commands
}

void loop() {
  // Listen for servo commands from PC
  // Execute servo movements with safety limits
  // Send position feedback to PC
}
```

#### Phase 4 Tasks
1. **PC Control Software**
   - [ ] Model loading and inference
   - [ ] Mouse position tracking
   - [ ] Coordinate transformation (screen → workspace)
   - [ ] WiFi communication to ESP32

2. **ESP32 Firmware Update**
   - [ ] WiFi connectivity and web server
   - [ ] Command parsing and execution
   - [ ] Safety limits and error handling
   - [ ] Position feedback system

3. **System Integration**
   - [ ] End-to-end testing
   - [ ] Latency optimization
   - [ ] Error recovery systems
   - [ ] Performance benchmarking

---

## Success Metrics

### Phase 1 Success Criteria
- [ ] All three joints move smoothly through full range
- [ ] No mechanical binding or interference
- [ ] Stable power supply and connections
- [ ] Documented hardware specifications

### Phase 2 Success Criteria
- [ ] Simulation matches real hardware workspace
- [ ] Environment trains without errors
- [ ] Forward kinematics produces accurate end positions
- [ ] Visualization displays correct tentacle pose

### Phase 3 Success Criteria
- [ ] Model achieves >90% success rate on test targets
- [ ] Average reach time < 2 seconds
- [ ] Smooth, efficient movement trajectories
- [ ] Stable training without divergence

### Phase 4 Success Criteria
- [ ] Real tentacle follows mouse cursor smoothly
- [ ] <100ms latency from mouse movement to servo response
- [ ] No oscillation or instability
- [ ] Graceful handling of unreachable targets

---

## Risk Mitigation

### Hardware Risks
- **Servo failure**: Order backup servos
- **Power issues**: Include voltage monitoring
- **Mechanical binding**: Design with clearances
- **WiFi connectivity**: Include fallback ethernet option

### Software Risks
- **Training divergence**: Implement early stopping and checkpoints
- **Sim-real gap**: Extensive hardware characterization
- **Communication lag**: Optimize protocol and add prediction
- **Safety violations**: Hard-coded joint limits

---

## Timeline

**Total Duration**: 7-10 days after hardware arrival

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1 | 2-3 days | Hardware delivery |
| Phase 2 | 2-3 days | Phase 1 hardware specs |
| Phase 3 | 1-2 days | Phase 2 environment |
| Phase 4 | 2-3 days | Phase 3 trained model |

---

## Next Steps

### Immediate Actions Needed
1. **Hardware Status Update**: Confirm delivery date and component list
2. **Workspace Setup**: Prepare development environment
3. **Specification Gathering**: What servo models were ordered?

### Questions for Clarification
1. **Development Environment**: Do you have Python/Arduino IDE setup?
2. **Hardware Specifications**: What specific servo models were ordered?
3. **Workspace Constraints**: Desk size, mounting options?
4. **Network Setup**: WiFi network details for ESP32 connection?

---

## Contact & Coordination

**Project Lead**: You (Hardware integration, testing, validation)  
**Software Architect**: Claude (Simulation, RL training, deployment software)

**Communication Protocol**:
- Daily progress updates
- Immediate notification of blockers
- Hardware specifications shared after Phase 1
- Code review before each phase completion

---

## **IMMEDIATE NEXT STEPS (Ready to Execute)**

### **Phase 1A: C Physics Simulation (Can Start Now)**
While waiting for hardware delivery, implement C simulation:

```bash
# Create directory structure
mkdir -p pufferlib/ocean/tendril
cd pufferlib/ocean/tendril

# Files to create:
touch tendril.h         # Physics interface  
touch tendril.c         # Physics implementation
touch tendril.py        # Python wrapper
touch binding.c        # Python-C binding
```

**Implementation Priority:**
1. **tendril.h**: Define data structures and function interfaces
2. **tendril.c**: Implement forward kinematics and physics simulation  
3. **binding.c**: Create Python-C interface (copy from cartpole/binding.c)
4. **tendril.py**: Python wrapper class (copy from cartpole.py pattern)

### **Phase 1B: Hardware Assembly (When Parts Arrive)**
Hardware assembly following PRD specifications with immediate integration testing.

### **Phase 1C: Real-to-Sim Calibration (Hardware + Software)**
Use hardware to calibrate C physics parameters for accurate simulation.

---

## **DECISION SUMMARY**

✅ **Architecture**: Ocean environment with C++ physics  
✅ **Scope**: Learning exercise + research platform  
✅ **Timeline**: PRD 6-week approach with rapid iteration  
✅ **Development Environment**: Ready (PufferLib available)  
✅ **Hardware**: Parts ordered and on the way  

**Ready to proceed with Phase 1A C physics implementation immediately.**

---

**Document Status**: Living document, updated as project progresses  
**Last Updated**: January 2025  
**Next Review**: After Phase 1A completion  

**Contact: Claude (Software Architecture) + You (Hardware Integration & Testing)**