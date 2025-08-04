# Tendril Project: Unified Critical Execution Plan (v4 - Final)

## Executive Summary & Critical Update

This plan is the final, definitive roadmap and supersedes all previous versions. Based on a final technical review, this version incorporates critical clarifications to the mathematical foundation, hardware validation, and software engineering process. The core "Mathematics First" principle remains, but the tasks within each phase are now specified with the necessary rigor to eliminate ambiguity and ensure a successful sim-to-real transfer.

**Guiding Principle**: Define & Validate Mathematics → Enforce Software Discipline → Characterize & Test Hardware.

## 🚨 **CRITICAL PRIORITY: Mathematical Foundation Fixes**

### The Fundamental Problem
The current implementation contains **coordinate system inconsistencies** between:
1. Forward kinematics calculations
2. Inverse kinematics validation  
3. Action space interpretation
4. Servo command generation

This creates a simulation that "works" internally but **cannot transfer to physical hardware**.

### Evidence of the Problem
```c
// INCONSISTENT COORDINATE MAPPINGS:
// Forward kinematics (tendril.h:181-183):
float base_yaw = servo1_yaw - M_PI/2;         // Centers yaw at 0°
float shoulder_pitch = servo2_pitch - M_PI/2; // -90° to +90° range  
float elbow_pitch = servo3_pitch - M_PI/2;    // -90° to +90° range

// But servo validation (tendril.h:503-504):
float servo1_angle = base_yaw_rad + M_PI/2;   // Different transformation!

// And visualization (tendril.c:341-342):
float display_angle = servo1_angle - M_PI/2;  // Yet another mapping!
```

**Result**: Training succeeds in the corrupted simulation but fails completely on hardware.

## 🎯 **REVISED EXECUTION PLAN - MATHEMATICS FIRST**

## PHASE 0: Foundational Mathematics & Simulation Integrity
*Priority: CRITICAL - All subsequent work depends on this. Start 3D printing of STLs in parallel on Day 1.*

**Rationale**: To create a simulation that is a true, unambiguous digital twin of the hardware. This phase establishes a single, consistent mathematical reality for the entire project.

### Task 0.1: Define and Document World Coordinate System
**Problem**: The zero-point and direction of the robot's axes are undefined.

**Solution**: Establish a clear, documented convention for the robot's coordinate system.

**Implementation** (Add to tendril.h documentation):
```c
/**
 * ============================================================================
 * TENDRIL COORDINATE SYSTEM
 * ============================================================================
 * - The robot base is centered at the world origin (0, 0, 0).
 * - The arm points along the POSITIVE X-AXIS when all servos are at 90° (SERVO_CENTER_RAD).
 * - The Z-axis points UP, away from the table.
 * - The Y-axis follows the right-hand rule.
 *
 * SERVO CONVENTIONS (0 to PI radians):
 * - Servo 1 (Base Yaw):
 *   - 0 rad: Points arm along the NEGATIVE Y-axis.
 *   - PI/2 rad (90°): Points arm along the POSITIVE X-axis (FORWARD).
 *   - PI rad: Points arm along the POSITIVE Y-axis.
 * - Servo 2 (Shoulder Pitch):
 *   - An INCREASING angle (e.g., 90° -> 120°) causes the arm segment to pitch UPWARDS (towards +Z).
 * - Servo 3 (Elbow Pitch):
 *   - An INCREASING angle causes the elbow to bend UPWARDS (relative to the first segment).
 * ============================================================================
 */
#define SERVO_MIN_RAD 0.0f
#define SERVO_MAX_RAD (M_PI)
#define SERVO_CENTER_RAD (M_PI/2)
```

### Task 0.2: Refactor & Unit Test Forward Kinematics (FK)
**Problem**: The FK code uses incorrect offsets and must be updated to match the new unified coordinate system.

**Solution**: Rewrite the compute_forward_kinematics function and validate it with a unit test against the now-defined coordinate system.

**Implementation** (test_kinematics.c):
```c
void test_fk() {
    // Test Case: All servos centered (90 degrees).
    // Expected: Arm points straight forward along +X axis.
    // Expected Position: x = SEGMENT_LENGTH * 2, y = 0, z = BASE_DEPTH
    // ...
    // Test Case: Shoulder pitch up, elbow straight.
    // Expected: End effector moves in X-Z plane.
    // ...
}
```

### Task 0.3: Refactor & Unit Test Inverse Kinematics (IK)
**Problem**: The existing validate_target_reachability (IK) function is incompatible with the new FK model and will produce incorrect results.

**Solution**: Rewrite the IK function to be the mathematical inverse of the new FK function. It must not contain any legacy offsets. Create a corresponding unit test.

**Implementation** (test_kinematics.c):
```c
void test_ik_fk_consistency() {
    // Test that IK and FK are inverses of each other.
    for (int i = 0; i < 100; i++) {
        // 1. Generate a random but reachable target position.
        // 2. Use the corrected IK to find the required joint angles.
        ReachabilityResult result = validate_target_reachability_corrected(target_pos);
        // 3. Use the corrected FK on those joint angles.
        compute_forward_kinematics_corrected(env, result.joint_angles);
        // 4. Assert that the final FK position is very close to the initial target position.
        assert(distance(env->end_effector_pos, target_pos) < 0.1f);
    }
}
```

### Task 0.4: Sweep and Refactor All Angle Consumers
**Problem**: Other parts of the code (e.g., visualization) still use incorrect angle offsets.

**Solution**: Systematically search for and refactor every piece of code that uses joint_angles.

**Action**:
- Run `grep -R "joint_angles\[.*\]"` and create a checklist of all usage sites.
- Go through the checklist and update each site (e.g., draw_top_view) to respect the new unified coordinate system.

### ✅ Phase 0 Success Check:
- [ ] The coordinate system convention is documented in tendril.h.
- [ ] All FK, IK, and visualization code is consistent with this convention.
- [ ] All new unit tests (FK, IK-FK consistency) pass in a CI pipeline (e.g., GitHub Actions).

## PHASE 1: Software Engineering & Baseline Training
*Proceed ONLY after Phase 0 is complete and all CI checks are green.*

**Rationale**: With a correct simulation, we can now apply disciplined software engineering to create a stable, measurable, and efficient training baseline.

### Task 1.1 - 1.4: Implement Bug Fixes and PufferLib Integration
Implement the bug fixes and PufferLib EvalCallback integration as detailed in the previous plan (v3). This includes:
- Fixing the last_angular_error update logic.
- Activating and throttling update_evaluation_metrics.
- Implementing a robust, simulation-time-based timeout with an explicit penalty.
- Ensuring the PufferLib binding correctly populates the info dictionary for W&B logging.

### Task 1.5: Re-check Reward Scaling
**Note**: The mathematical audit may have changed the typical range of angular_error. After the fixes, run a short test to observe the new range and confirm that the reward magnitudes are still reasonable. Adjust weights if necessary.

### Task 1.6: Improve Logging Clarity
**Action**: Rename logged metrics to be explicit (e.g., avg_angular_error_deg).

### ✅ Phase 1 Success Check:
- [ ] A training run (using the mathematically correct sim) logs to W&B.
- [ ] The W&B dashboard correctly displays custom evaluation metrics with clear, explicit units.
- [ ] A baseline trained model (model_post_math_fix.zip) is saved for comparison.

## PHASE 2: Hardware Characterization & Validation
*The 3D prints started on Day 1 should be complete.*

**Rationale**: To close the sim-to-real gap, we must replace the simulation's assumed hardware parameters with measured real-world values.

### Task 2.1: Assemble the Physical Tendril Arm
Following exact STL specifications from `tendril_about_me.md`:
- 3D print Base.stl (60×60×20mm)
- 3D print 2× Segment.stl (50×20×50mm) 
- 3D print End_Cap.stl (30×30×15mm conical)
- Assemble with 3× MG996R servos per specifications

### Task 2.2: Servo Characterization
**Action**: Write a simple characterization script. Measure and record:
- **Actual Min/Max Range**: The true operational angles.
- **Backlash**: The measured deadband.
- **Mechanical Zero Offset**: The difference between the servo horn's physical zero and the ideal 0° position. This is critical.

**Goal**: Create a hardware_config.json file containing these measured offsets and parameters for each of the three specific servos. The real-world driver will load this file.

### Task 2.3: Workspace Comparison (as per v3 plan)
- Drive servos through full range with manual commands
- Measure actual end-effector positions with ruler/calipers
- Compare against simulation predictions
- Document discrepancies for simulation correction

### ✅ Phase 2 Success Check:
- [ ] The physical arm is assembled.
- [ ] The hardware_config.json file with measured data for each servo exists.
- [ ] The physical workspace matches the simulation workspace within a 5mm tolerance.

## PHASE 3 & 4: Deployment and Advanced Research

### Phase 3: Sim-to-Real Deployment
The ESP32/hardware driver should load the hardware_config.json to apply the measured zero-offsets, effectively calibrating the physical arm to match the ideal simulation.

Deploy the policy from Phase 1 and validate its performance against the simulation baseline.

### Phase 4: Advanced Research & Optimization
With a validated pipeline, now is the time to implement the Reward & Action Smoothing, Curriculum Learning, Domain Randomization, and explore Advanced Policy Architectures. The bounds for domain randomization should be centered around the measured hardware values from Phase 2.

