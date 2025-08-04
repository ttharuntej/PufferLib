#ifndef TENDRIL_MATH_ONLY
#define TENDRIL_MATH_ONLY
#endif
#define _USE_MATH_DEFINES
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "tendril.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// MATHEMATICAL FUNCTIONS - Moved from tendril.h to avoid ODR violations
// ============================================================================

// CORRECTED Forward kinematics: Uses unified coordinate system (NO arbitrary offsets)
void compute_forward_kinematics(Tendril* env) {
    // Hardware construction chain (user specification):
    // Base.stl → Servo1(yaw) → Segment1.stl → Servo2(pitch) → Segment2.stl → Servo3(pitch) → End_Cap.stl
    
    // Use servo angles DIRECTLY in 0-PI range as per coordinate system definition
    float servo1_yaw = env->joint_angles[0];      // 0-PI: 0=(-Y), PI/2=(+X), PI=(+Y)
    float servo2_pitch = env->joint_angles[1];    // 0-PI: increasing = upward pitch
    float servo3_pitch = env->joint_angles[2];    // 0-PI: increasing = upward elbow bend
    
    // CANONICAL ANGLE MAPPING (servo - π/2) - USED EVERYWHERE
    float base_yaw = servo1_yaw - M_PI/2;         // World yaw: 0=(-Y), π/2=(+X), π=(+Y)  
    float shoulder_world = servo2_pitch - M_PI/2; // World pitch: π/2=horizontal, >π/2=up
    float elbow_world = servo3_pitch - M_PI/2;    // World pitch: π/2=straight, >π/2=up
    
    // 1. BASE.STL position (world origin)
    float base_x = 0.0f;
    float base_y = 0.0f; 
    float base_z = BASE_DEPTH;  // Top of base platform (corrected)
    
    // 2. SEGMENT1 end position (first arm segment)
    float segment1_end_x = base_x + SEGMENT_LENGTH * cosf(base_yaw) * cosf(shoulder_world);
    float segment1_end_y = base_y + SEGMENT_LENGTH * sinf(base_yaw) * cosf(shoulder_world);  
    float segment1_end_z = base_z + SEGMENT_LENGTH * sinf(shoulder_world);
    
    // 3. SEGMENT2 end position (second arm segment)
    // Second segment bends relative to first segment's final orientation
    float total_pitch = shoulder_world + elbow_world;  // Cumulative pitch
    float segment2_end_x = segment1_end_x + SEGMENT_LENGTH * cosf(base_yaw) * cosf(total_pitch);
    float segment2_end_y = segment1_end_y + SEGMENT_LENGTH * sinf(base_yaw) * cosf(total_pitch);
    float segment2_end_z = segment1_end_z + SEGMENT_LENGTH * sinf(total_pitch);
    
    // 4. END_CAP tip position (final end effector)
    env->end_effector_pos[0] = segment2_end_x + ENDCAP_LENGTH * cosf(base_yaw) * cosf(total_pitch);
    env->end_effector_pos[1] = segment2_end_y + ENDCAP_LENGTH * sinf(base_yaw) * cosf(total_pitch);
    env->end_effector_pos[2] = segment2_end_z + ENDCAP_LENGTH * sinf(total_pitch);
    
    // 5. POINTING DIRECTION (laser beam direction)
    float pointing_x = cosf(base_yaw) * cosf(total_pitch);
    float pointing_y = sinf(base_yaw) * cosf(total_pitch);
    float pointing_z = sinf(total_pitch);
    
    // Normalize pointing direction vector (should already be normalized)
    float pointing_mag = sqrtf(pointing_x*pointing_x + pointing_y*pointing_y + pointing_z*pointing_z);
    if (pointing_mag > 0.001f) {
        env->pointing_direction[0] = pointing_x / pointing_mag;
        env->pointing_direction[1] = pointing_y / pointing_mag;
        env->pointing_direction[2] = pointing_z / pointing_mag;
    } else {
        // Fallback (shouldn't happen)
        env->pointing_direction[0] = 1.0f;  // Point forward
        env->pointing_direction[1] = 0.0f;
        env->pointing_direction[2] = 0.0f;
    }
    
    // Calculate target direction (from end effector to target)
    float target_dx = env->target_pos[0] - env->end_effector_pos[0];
    float target_dy = env->target_pos[1] - env->end_effector_pos[1];
    float target_dz = env->target_pos[2] - env->end_effector_pos[2];
    float target_mag = sqrtf(target_dx*target_dx + target_dy*target_dy + target_dz*target_dz);
    
    if (target_mag > 0.001f) {
        env->target_direction[0] = target_dx / target_mag;
        env->target_direction[1] = target_dy / target_mag;
        env->target_direction[2] = target_dz / target_mag;
    } else {
        // Target is at end effector position
        env->target_direction[0] = 0.0f;
        env->target_direction[1] = 0.0f;
        env->target_direction[2] = 1.0f;
    }
    
    // Calculate angular error using dot product
    float dot_product = env->pointing_direction[0] * env->target_direction[0] + 
                       env->pointing_direction[1] * env->target_direction[1] + 
                       env->pointing_direction[2] * env->target_direction[2];
    
    // Clamp dot product to avoid numerical errors in acos
    dot_product = fmaxf(-1.0f, fminf(1.0f, dot_product));
    env->angular_error = acosf(dot_product);  // Angular error in radians
    
    // Clamp to reasonable workspace (prevent extreme positions)
    float max_reach = 2.0f * SEGMENT_LENGTH + BASE_DEPTH;
    float current_reach = sqrtf(env->end_effector_pos[0]*env->end_effector_pos[0] + 
                               env->end_effector_pos[1]*env->end_effector_pos[1]);
    if (current_reach > max_reach) {
        float scale = max_reach / current_reach;
        env->end_effector_pos[0] *= scale;
        env->end_effector_pos[1] *= scale;
    }
    
    // Keep end effector above ground
    if (env->end_effector_pos[2] < BASE_DEPTH) {
        env->end_effector_pos[2] = BASE_DEPTH;
    }
}

// Compute observation vector (17D for laser pointing)
void compute_observations(Tendril* env) {
    int idx = 0;
    
    // Joint angles (normalized to [0, 1] from 0-180° MG996R range) [3D]
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->observations[idx++] = env->joint_angles[i] / JOINT_LIMIT_RAD;
    }
    
    // End effector position (normalized to workspace) [3D]
    for (int i = 0; i < 3; i++) {
        env->observations[idx++] = env->end_effector_pos[i] / WORKSPACE_SIZE;
    }
    
    // Target position (normalized to workspace) [3D]
    for (int i = 0; i < 3; i++) {
        env->observations[idx++] = env->target_pos[i] / WORKSPACE_SIZE;
    }
    
    // Joint velocities (normalized) [3D]
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->observations[idx++] = env->joint_velocities[i] / (JOINT_LIMIT_RAD / DT);
    }
    
    // NEW: Pointing direction (already normalized) [3D]
    for (int i = 0; i < 3; i++) {
        env->observations[idx++] = env->pointing_direction[i];
    }
    
    // NEW: Angular error (normalized to [0, 1], 0=perfect, 1=opposite) [1D]
    env->observations[idx++] = env->angular_error / M_PI;
    
    // NEW: Stability timer (normalized to stability duration) [1D]
    env->observations[idx++] = fminf(env->stability_timer / STABILITY_DURATION, 1.0f);
    
    // Total: 3 + 3 + 3 + 3 + 3 + 1 + 1 = 17D
}

// NEW Reward function: encourage accurate laser pointing with stability
float compute_reward(Tendril* env) {
    float reward = 0.0f;

    // 1. Dense Progress Reward (Potential-Based Shaping) - REDUCED SCALING
    // This rewards the agent for reducing its angular error to the target.
    // FIXED: Reduced from 10.0f to 1.0f to prevent over-exploration
    float progress = env->last_angular_error - env->angular_error;
    reward += progress * 1.0f; // Much more conservative scaling

    // 2. Sparse Success Bonus
    // A single, large bonus for achieving the final goal.
    if (env->target_state == TARGET_SUCCESS) {
        reward += 10.0f; // Reduced from 50.0f - less overwhelming
    }

    // 3. Time penalty - creates "cost of living" that encourages efficiency
    reward -= 0.01f;  // Fixed time penalty per timestep
    
    // 4. Action consistency penalty (targets root cause of jerkiness)
    float action_consistency_penalty = 0.0f;
    for (int i = 0; i < NUM_JOINTS; i++) {
        float action_change = fabsf(env->actions[i] - env->last_actions[i]);
        action_consistency_penalty += action_change;
    }
    reward -= action_consistency_penalty * 0.02f;  // Penalize erratic actions

    // 5. Velocity smoothness bonus (reward coordinated movements)  
    float total_smoothed_velocity = 0.0f;
    for (int i = 0; i < NUM_JOINTS; i++) {
        total_smoothed_velocity += fabsf(env->smoothed_velocities[i]);
    }
    // Reward moderate, consistent velocity (not too fast, not too slow)
    float ideal_velocity = 1.0f; // rad/s total across all joints
    float velocity_deviation = fabsf(total_smoothed_velocity - ideal_velocity);
    float smoothness_bonus = fmaxf(0.0f, 1.0f - velocity_deviation) * 0.5f;
    reward += smoothness_bonus;

    // NOTE: State updates moved to c_step() for cleaner separation of concerns
    return reward;
}

// Initialize tendril environment
void init(Tendril* env) {
    env->tick = 0;
    memset(&env->log, 0, sizeof(Log));
    
    // Initialize joint limits (0 to 180 degrees for servos)
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_limits[i][0] = 0.0f;
        env->joint_limits[i][1] = JOINT_LIMIT_RAD;
        env->servo_backlash[i] = 0.02f; // ~1 degree backlash
        env->friction_coeffs[i] = 0.1f;
    }
    
    // Initialize per-environment RNG with default seed
    seed_env_rng(env, 12345);  // Default seed, will be overridden by reset
}

// Allocate memory for PufferLib interface
// Returns 0 on success, -1 on failure
int allocate(Tendril* env) {
    init(env);
    
    env->observations = (float*)calloc(17, sizeof(float));  // 17D observation (pointing + stability)
    if (!env->observations) return -1;
    
    env->actions = (float*)calloc(3, sizeof(float));        // 3D action
    if (!env->actions) {
        free(env->observations);
        return -1;
    }
    
    env->rewards = (float*)calloc(1, sizeof(float));
    if (!env->rewards) {
        free(env->observations);
        free(env->actions);
        return -1;
    }
    
    env->terminals = (bool*)calloc(1, sizeof(bool));
    if (!env->terminals) {
        free(env->observations);
        free(env->actions);
        free(env->rewards);
        return -1;
    }
    
    env->truncations = (bool*)calloc(1, sizeof(bool));
    if (!env->truncations) {
        free(env->observations);
        free(env->actions);
        free(env->rewards);
        free(env->terminals);
        return -1;
    }
    
    return 0;  // Success
}

// Free allocated memory (safe for partially allocated structs)
void free_allocated(Tendril* env) {
    if (env->observations) {
        free(env->observations);
        env->observations = NULL;
    }
    if (env->actions) {
        free(env->actions);
        env->actions = NULL;
    }
    if (env->rewards) {
        free(env->rewards);
        env->rewards = NULL;
    }
    if (env->terminals) {
        free(env->terminals);
        env->terminals = NULL;
    }
    if (env->truncations) {
        free(env->truncations);
        env->truncations = NULL;
    }
}

// SERVO REACHABILITY VALIDATION - First Principles Implementation
// CORRECTED: 2D Table-Mounted Servo Inverse Kinematics (0-180° servos)
ReachabilityResult validate_target_reachability(float target_x, float target_y, float target_z) {
    ReachabilityResult result = {false, {0, 0, 0}, 0.0f, 0.0f};
    
    // CRITICAL: Target must be ABOVE table (servos can't point below table)
    if (target_z < BASE_DEPTH) {
        return result;  // Target below table is unreachable
    }
    
    // Adjust target position relative to base coordinate system
    float adjusted_z = target_z - BASE_DEPTH;
    
    // 1. SOLVE BASE YAW (servo1) - 180° horizontal rotation
    // CRITICAL FIX: Servo can only cover 180 degrees in forward hemisphere
    if (target_x < 0.0f) {
        return result; // Target behind robot is unreachable by hardware constraint
    }
    
    float horizontal_distance = sqrtf(target_x * target_x + target_y * target_y);
    float base_yaw_rad = atan2f(target_y, target_x);
    
    // For forward hemisphere targets (x >= 0), map [-PI/2, +PI/2] to [0, PI]
    float servo1_angle = base_yaw_rad + M_PI/2;
    
    // 2. SOLVE 2DOF ARM IN VERTICAL PLANE (servo2 + servo3)
    // Project target into the vertical plane defined by base_yaw
    float planar_reach = horizontal_distance;  // Distance from base in horizontal plane
    float vertical_reach = adjusted_z;         // Height above base
    
    // CRITICAL: Servos can only reach ABOVE table (positive Z only)
    if (vertical_reach < 0) {
        return result;  // Below table unreachable
    }
    
    // Total distance to target from shoulder position  
    float target_distance = sqrtf(planar_reach * planar_reach + vertical_reach * vertical_reach);
    
    // Check if target is within maximum reach (2 segments)
    float max_reach = 2.0f * SEGMENT_LENGTH;
    float min_reach = 10.0f;  // Minimum reach (avoid singularities)
    
    if (target_distance > max_reach || target_distance < min_reach) {
        return result;  // Target outside reachable workspace
    }
    
    // 3. SOLVE TWO-LINK ARM INVERSE KINEMATICS  
    float L1 = SEGMENT_LENGTH;  // First segment
    float L2 = SEGMENT_LENGTH;  // Second segment
    
    // Elbow angle calculation (internal angle between segments)
    float cos_elbow = (L1*L1 + L2*L2 - target_distance*target_distance) / (2*L1*L2);
    cos_elbow = fmaxf(-1.0f, fminf(1.0f, cos_elbow));  // Clamp for numerical stability
    
    float elbow_internal_angle = acosf(cos_elbow);
    
    // Convert elbow internal angle to servo3 angle using CANONICAL MAPPING
    // elbow_internal_angle of π = fully extended, 0 = fully bent
    // servo3 at π/2 = straight, 0 = bent down, π = bent up
    float elbow_world_angle = M_PI - elbow_internal_angle;  // Convert to world frame
    float servo3_angle = elbow_world_angle + M_PI/2;  // CANONICAL INVERSE: world + π/2
    
    // 4. SOLVE SHOULDER ANGLE (servo2) - Using numerically stable atan2 approach
    float target_angle_from_horizontal = atan2f(vertical_reach, planar_reach);
    
    // More stable calculation using atan2 instead of asin
    float shoulder_correction_sin = (L2 * sinf(elbow_internal_angle)) / target_distance;
    float shoulder_correction_cos = (L1 + L2 * cosf(elbow_internal_angle)) / target_distance;
    float shoulder_correction = atan2f(shoulder_correction_sin, shoulder_correction_cos);
    
    float shoulder_angle_rad = target_angle_from_horizontal - shoulder_correction;
    
    // Convert to servo2 angle: 0° = down, 90° = horizontal, 180° = up
    float servo2_angle = shoulder_angle_rad + M_PI/2;  // Add 90° to make 90° = horizontal
    
    // 5. VALIDATE ALL SERVO LIMITS (0-180°) - TABLE-MOUNTED CONSTRAINTS
    if (servo2_angle < 0.0f || servo2_angle > JOINT_LIMIT_RAD ||
        servo3_angle < 0.0f || servo3_angle > JOINT_LIMIT_RAD) {
        return result;  // Joint limits exceeded
    }
    
    // 6. ADDITIONAL 2D WORKSPACE CONSTRAINTS
    // Ensure servo2 doesn't point below horizontal (can't reach under table)
    if (servo2_angle < M_PI/4) {  // Below 45° is problematic for table-mounted arm
        return result;  // Too close to table surface
    }
    
    // 7. CALCULATE SOLUTION QUALITY METRICS
    float limit_margins[3] = {
        fminf(servo1_angle, JOINT_LIMIT_RAD - servo1_angle),
        fminf(servo2_angle - M_PI/4, JOINT_LIMIT_RAD - servo2_angle),  // Account for table constraint
        fminf(servo3_angle, JOINT_LIMIT_RAD - servo3_angle)
    };
    
    result.min_distance_to_limits = fminf(limit_margins[0], fminf(limit_margins[1], limit_margins[2]));
    result.confidence = result.min_distance_to_limits / (JOINT_LIMIT_RAD / 6.0f);  // More conservative
    result.confidence = fmaxf(0.0f, fminf(1.0f, result.confidence));
    
    // Store solution
    result.joint_angles[0] = servo1_angle;
    result.joint_angles[1] = servo2_angle;
    result.joint_angles[2] = servo3_angle;
    result.is_reachable = true;
    
    return result;
}

// Generate reachable target within servo constraints
void generate_reachable_target(Tendril* env) {
    const int MAX_ATTEMPTS = 50;  // Prevent infinite loops
    int attempts = 0;
    
    while (attempts < MAX_ATTEMPTS) {
        // Generate candidate target within forward hemisphere only
        float candidate_x = randf_env(env, 0.0f, WORKSPACE_SIZE/2);  // Only positive X (forward)
        float candidate_y = randf_env(env, -WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
        float candidate_z = randf_env(env, BASE_DEPTH + 10, BASE_DEPTH + WORKSPACE_SIZE/2);
        
        // Validate reachability
        ReachabilityResult result = validate_target_reachability(candidate_x, candidate_y, candidate_z);
        
        if (result.is_reachable && result.confidence > 0.2f) {  // Require decent confidence
            // Valid target found!
            env->target_pos[0] = candidate_x;
            env->target_pos[1] = candidate_y;
            env->target_pos[2] = candidate_z;
            return;
        }
        
        attempts++;
    }
    
    // Fallback: Generate GUARANTEED reachable target (validated)
    // Use very conservative workspace that we know works with servo limits
    float safe_reach = SEGMENT_LENGTH * 0.6f;  // Only 60% reach to ensure safety
    // CORRECTED: Use servo angles in canonical 0-π range (forward hemisphere)
    float safe_servo_angles[] = {0.0f, M_PI/4, M_PI/2, 3*M_PI/4, M_PI};  // 0-180° servo range
    int angle_index = lcg32_random_r(&env->rng_state) % 5;
    float safe_servo_angle = safe_servo_angles[angle_index];
    
    // Convert servo angle to world angle using CANONICAL MAPPING
    float safe_world_angle = safe_servo_angle - M_PI/2;
    
    // Generate and validate fallback target
    float fallback_x = safe_reach * cosf(safe_world_angle);
    float fallback_y = safe_reach * sinf(safe_world_angle);
    float fallback_z = BASE_DEPTH + SEGMENT_LENGTH * 0.4f;  // Very conservative height
    
    ReachabilityResult fallback_result = validate_target_reachability(fallback_x, fallback_y, fallback_z);
    
    if (fallback_result.is_reachable) {
        env->target_pos[0] = fallback_x;
        env->target_pos[1] = fallback_y;
        env->target_pos[2] = fallback_z;
    } else {
        // Ultimate fallback - center position (always reachable in forward hemisphere)
        env->target_pos[0] = SEGMENT_LENGTH * 0.5f; // Positive X (forward)
        env->target_pos[1] = 0.0f; // Center Y
        env->target_pos[2] = BASE_DEPTH + 20.0f;    // Just above table
    }
}