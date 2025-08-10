#include "tendril_clean.h"

// HARDWARE-ACCURATE Forward kinematics: Matches physical STL construction
void compute_forward_kinematics(Tendril* env) {
    // Hardware construction chain (user specification):
    // Base.stl → Servo1(yaw) → Segment1.stl → Servo2(pitch) → Segment2.stl → Servo3(pitch) → End_Cap.stl
    
    float servo1_yaw = env->joint_angles[0];      // Base yaw: 0-180° horizontal rotation  
    float servo2_pitch = env->joint_angles[1];    // Shoulder pitch: 0-180° up/down bend
    float servo3_pitch = env->joint_angles[2];    // Elbow pitch: 0-180° up/down bend
    
    // Convert servo angles (0-180°) to proper joint angles (-90° to +90° for pitch)
    float base_yaw = servo1_yaw - M_PI/2;         // Center yaw at 0°
    float shoulder_pitch = servo2_pitch - M_PI/2; // -90° to +90° pitch range
    float elbow_pitch = servo3_pitch - M_PI/2;    // -90° to +90° pitch range
    
    // 1. BASE.STL position (origin platform) 
    float base_x = 0.0f;
    float base_y = 0.0f; 
    float base_z = BASE_DEPTH / 2;  // Half height of base platform
    
    // 2. SERVO1 position (mounted in Base.stl opening)
    float servo1_x = base_x;
    float servo1_y = base_y;
    float servo1_z = base_z + BASE_DEPTH/2;  // Top of base platform
    
    // 3. SEGMENT1.STL position (attached to Servo1 horn, rotated by base_yaw)
    float segment1_end_x = servo1_x + SEGMENT_LENGTH * cosf(base_yaw) * cosf(shoulder_pitch);
    float segment1_end_y = servo1_y + SEGMENT_LENGTH * sinf(base_yaw) * cosf(shoulder_pitch);  
    float segment1_end_z = servo1_z + SEGMENT_LENGTH * sinf(shoulder_pitch);
    
    // 4. SERVO2 position (mounted in Segment1.stl gap)
    float servo2_x = segment1_end_x;
    float servo2_y = segment1_end_y;
    float servo2_z = segment1_end_z;
    
    // 5. SEGMENT2.STL position (attached to Servo2 horn, rotated by shoulder+elbow pitch)
    float total_pitch = shoulder_pitch + elbow_pitch;  // Cumulative bending
    float segment2_end_x = servo2_x + SEGMENT_LENGTH * cosf(base_yaw) * cosf(total_pitch);
    float segment2_end_y = servo2_y + SEGMENT_LENGTH * sinf(base_yaw) * cosf(total_pitch);
    float segment2_end_z = servo2_z + SEGMENT_LENGTH * sinf(total_pitch);
    
    // 6. SERVO3 position (mounted in Segment2.stl gap)  
    float servo3_x = segment2_end_x;
    float servo3_y = segment2_end_y;
    float servo3_z = segment2_end_z;
    
    // 7. END_CAP.STL position (final pointing tip, attached to Servo3 horn)
    float endcap_pitch = total_pitch; // Sum of all joint contributions
    env->end_effector_pos[0] = servo3_x + ENDCAP_LENGTH * cosf(base_yaw) * cosf(endcap_pitch);
    env->end_effector_pos[1] = servo3_y + ENDCAP_LENGTH * sinf(base_yaw) * cosf(endcap_pitch);
    env->end_effector_pos[2] = servo3_z + ENDCAP_LENGTH * sinf(endcap_pitch);
    
    // Calculate pointing direction (End_Cap.stl laser beam direction)
    float final_pitch = total_pitch; // Sum of all preceding joint angles
    
    // Pointing direction from Servo3 position through End_Cap.stl tip
    float pointing_x = cosf(base_yaw) * cosf(final_pitch);
    float pointing_y = sinf(base_yaw) * cosf(final_pitch);
    float pointing_z = sinf(final_pitch);
    
    // Normalize pointing direction vector
    float pointing_mag = sqrtf(pointing_x*pointing_x + pointing_y*pointing_y + pointing_z*pointing_z);
    if (pointing_mag > 0.001f) {
        env->pointing_direction[0] = pointing_x / pointing_mag;
        env->pointing_direction[1] = pointing_y / pointing_mag;
        env->pointing_direction[2] = pointing_z / pointing_mag;
    } else {
        // Fallback pointing direction
        env->pointing_direction[0] = 0.0f;
        env->pointing_direction[1] = 0.0f;
        env->pointing_direction[2] = 1.0f;  // Point up
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
        env->target_direction[0] = 0.0f;
        env->target_direction[1] = 0.0f;
        env->target_direction[2] = 1.0f;
    }
    
    // Calculate angular error using dot product
    float dot_product = env->pointing_direction[0] * env->target_direction[0] + 
                       env->pointing_direction[1] * env->target_direction[1] + 
                       env->pointing_direction[2] * env->target_direction[2];
    
    // Clamp dot product to avoid numerical errors in acos
    dot_product = clampf(dot_product, -1.0f, 1.0f);
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
        env->observations[idx++] = env->joint_velocities[i] / (JOINT_LIMIT_RAD / TAU);
    }
    
    // Pointing direction (already normalized) [3D]
    for (int i = 0; i < 3; i++) {
        env->observations[idx++] = env->pointing_direction[i];
    }
    
    // Angular error (normalized to [0, 1], 0=perfect, 1=opposite) [1D]
    env->observations[idx++] = env->angular_error / M_PI;
    
    // Stability timer (normalized to stability duration) [1D]
    env->observations[idx++] = clampf(env->stability_timer / STABILITY_DURATION, 0.0f, 1.0f);
    
    // Total: 3 + 3 + 3 + 3 + 3 + 1 + 1 = 17D
}

// EXPERT-OPTIMIZED REWARD FUNCTION
float compute_reward(Tendril* env) {
    float reward = 0.0f;
    
    // 1. DENSE ANGULAR ACCURACY REWARD (Primary - dot product shaping, numerically stable)
    float cosang = env->pointing_direction[0]*env->target_direction[0]
                 + env->pointing_direction[1]*env->target_direction[1]
                 + env->pointing_direction[2]*env->target_direction[2];
    cosang = clampf(cosang, -1.0f, 1.0f);  // Clamp for safety
    
    // Strictly positive shaping emphasizing precision (0 to +8)
    float ang_reward = 8.0f * powf(fmaxf(0.0f, cosang), 3.0f);  // Cubic precision focus
    reward += ang_reward;
    
    // 2. LASER BEAM PERPENDICULAR DISTANCE REWARD (Expert insight!)
    // For laser pointing, miss distance from beam matters more than tip-to-target distance
    float rx = env->target_pos[0] - env->end_effector_pos[0];
    float ry = env->target_pos[1] - env->end_effector_pos[1];
    float rz = env->target_pos[2] - env->end_effector_pos[2];
    
    // d_perp = || r × pointing_dir || (perpendicular distance from laser beam to target)
    float cx = ry*env->pointing_direction[2] - rz*env->pointing_direction[1];
    float cy = rz*env->pointing_direction[0] - rx*env->pointing_direction[2]; 
    float cz = rx*env->pointing_direction[1] - ry*env->pointing_direction[0];
    float d_perp = sqrtf(cx*cx + cy*cy + cz*cz);
    
    // Compact shaping bonus for laser accuracy (0 to +2)
    reward += 2.0f * expf(-d_perp / 25.0f);
    
    // 3. SPEED BONUS (Fast angular improvement)
    float angular_improvement = env->last_angular_error - env->angular_error;
    if (angular_improvement > 0.001f) {
        reward += angular_improvement * 10.0f;  // Reward fast convergence
    }
    
    // 4. GRADUATED PRECISION BONUSES (Clear milestones)
    float error_degrees = env->angular_error * 180.0f / M_PI;
    if (error_degrees < 20.0f) reward += 3.0f;   // Getting close
    if (error_degrees < 10.0f) reward += 5.0f;   // Very good  
    if (error_degrees < 5.0f)  reward += 10.0f;  // Success threshold
    if (error_degrees < 2.0f)  reward += 15.0f;  // Excellent precision
    
    // 5. STABILITY BONUS (Hold position when accurate)
    if (env->angular_error < ANGULAR_THRESHOLD_RAD) {
        reward += env->stability_timer * 15.0f;  // Reward steady pointing
        if (env->stability_timer >= STABILITY_DURATION) {
            reward += 25.0f;  // Mission complete bonus!
        }
    }
    
    // 6. SMOOTH MOTION PENALTIES (FIXED: Per-joint acceleration tracking)
    float total_velocity = 0.0f, total_accel = 0.0f;
    for (int i = 0; i < NUM_JOINTS; i++) {
        float v = env->joint_velocities[i];
        total_velocity += fabsf(v);
        total_accel += fabsf(v - env->prev_joint_vel[i]);  // FIXED: Per-joint comparison
        env->prev_joint_vel[i] = v;  // Update for next step
    }
    reward -= total_velocity * 0.03f;  // Modest velocity penalty
    reward -= total_accel * 0.05f;     // Acceleration penalty for smoothness
    
    // 7. ACTION-BASED SMOOTHNESS (Expert recommendation - control costs)
    float u_cost = 0.0f, du_cost = 0.0f;
    for (int i = 0; i < NUM_JOINTS; i++) {
        u_cost += fabsf(env->actions[i]);
        du_cost += fabsf(env->actions[i] - env->last_actions[i]);  // Action smoothness
        env->last_actions[i] = env->actions[i];  // Update for next step
    }
    reward -= 0.01f * u_cost;   // Small control effort penalty
    reward -= 0.02f * du_cost;  // Action smoothness penalty
    
    // 8. TIME EFFICIENCY PENALTY
    reward -= 0.01f;  // Small time penalty per step
    
    // 9. JOINT LIMIT PENALTIES (Safety - consistent 5-175° range)
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_deg = env->joint_angles[i] * 180.0f / M_PI;
        if (angle_deg < 10.0f || angle_deg > 170.0f) {  // Approaching limits
            reward -= 2.0f;  // Warning penalty
        }
        if (angle_deg < 5.0f || angle_deg > 175.0f) {   // At limits  
            reward -= 10.0f;  // Strong penalty
        }
    }
    
    return reward;
}

// Initialize tendril environment
void init(Tendril* env) {
    env->tick = 0;
    memset(&env->log, 0, sizeof(Log));
    
    // Initialize joint limits and hardware parameters
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_limits[i][0] = 0.0f;
        env->joint_limits[i][1] = JOINT_LIMIT_RAD;
        env->servo_backlash[i] = 0.02f; // ~1 degree backlash
        env->friction_coeffs[i] = 0.1f;
        
        // FIXED: Initialize smoothness tracking (expert feedback)
        env->prev_joint_vel[i] = 0.0f;  // Previous velocities start at zero
        env->last_actions[i] = 0.0f;    // Previous actions start at zero
    }
    
    // FIXED: Initialize per-env movement direction tracking (no more global state!)
    env->last_move_dir[0] = 0.0f;
    env->last_move_dir[1] = 0.0f;
    env->last_move_dir[2] = 0.0f;
}

// Allocate memory for PufferLib interface
void allocate(Tendril* env) {
    init(env);
    env->observations = (float*)calloc(17, sizeof(float));  // 17D observation
    env->actions = (float*)calloc(3, sizeof(float));        // 3D action
    env->rewards = (float*)calloc(1, sizeof(float));
    env->terminals = (bool*)calloc(1, sizeof(bool));
    env->truncations = (bool*)calloc(1, sizeof(bool));
}

// Free allocated memory
void free_allocated(Tendril* env) {
    if (env->observations) free(env->observations);
    if (env->actions) free(env->actions);
    if (env->rewards) free(env->rewards);
    if (env->terminals) free(env->terminals);
    if (env->truncations) free(env->truncations);
    
    // Null pointers for safety
    env->observations = NULL;
    env->actions = NULL;
    env->rewards = NULL;
    env->terminals = NULL;
    env->truncations = NULL;
}

// SERVO REACHABILITY VALIDATION - First Principles Implementation
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
    cos_elbow = clampf(cos_elbow, -1.0f, 1.0f);  // Clamp for numerical stability
    
    float elbow_internal_angle = acosf(cos_elbow);
    
    // Convert to servo3 angle: 90° = straight out, 0° = bent down, 180° = bent up
    float servo3_angle = M_PI/2 + (M_PI - elbow_internal_angle)/2;  // Center around 90°
    
    // 4. SOLVE SHOULDER ANGLE (servo2)
    float target_angle_from_horizontal = atan2f(vertical_reach, planar_reach);
    
    // More stable calculation using atan2 instead of asin
    float shoulder_correction_sin = (L2 * sinf(elbow_internal_angle)) / target_distance;
    float shoulder_correction_cos = (L1 + L2 * cosf(elbow_internal_angle)) / target_distance;
    float shoulder_correction = atan2f(shoulder_correction_sin, shoulder_correction_cos);
    
    float shoulder_angle_rad = target_angle_from_horizontal - shoulder_correction;
    
    // Convert to servo2 angle: 0° = down, 90° = horizontal, 180° = up
    float servo2_angle = shoulder_angle_rad + M_PI/2;  // Add 90° to make 90° = horizontal
    
    // 5. VALIDATE ALL SERVO LIMITS (5-175° safety margins) - FIXED: Match runtime constraints
    float min_safe = 5.0f * M_PI / 180.0f;    // 5° minimum safety margin
    float max_safe = 175.0f * M_PI / 180.0f;  // 175° maximum safety margin
    
    if (servo1_angle < min_safe || servo1_angle > max_safe ||
        servo2_angle < min_safe || servo2_angle > max_safe ||
        servo3_angle < min_safe || servo3_angle > max_safe) {
        return result;  // Joint limits exceeded (with safety margins)
    }
    
    // 6. ADDITIONAL 2D WORKSPACE CONSTRAINTS
    // RELAXED: From 45° to 15° based on expert feedback (geometry-based would be better)
    if (servo2_angle < 15.0f * M_PI / 180.0f) {  // Below 15° is problematic for table-mounted arm
        return result;  // Too close to table surface
    }
    
    // 7. CALCULATE SOLUTION QUALITY METRICS (FIXED: Use consistent safety margins)
    float limit_margins[3] = {
        fminf(servo1_angle - min_safe, max_safe - servo1_angle),
        fminf(fmaxf(servo2_angle - 15.0f * M_PI / 180.0f, servo2_angle - min_safe), max_safe - servo2_angle),  // Table + safety constraints
        fminf(servo3_angle - min_safe, max_safe - servo3_angle)
    };
    
    result.min_distance_to_limits = fminf(limit_margins[0], fminf(limit_margins[1], limit_margins[2]));
    result.confidence = result.min_distance_to_limits / (JOINT_LIMIT_RAD / 6.0f);  // More conservative
    result.confidence = clampf(result.confidence, 0.0f, 1.0f);
    
    // Store solution
    result.joint_angles[0] = servo1_angle;
    result.joint_angles[1] = servo2_angle;
    result.joint_angles[2] = servo3_angle;
    result.is_reachable = true;
    
    return result;
}

// Generate reachable target within servo constraints - FIXED: Use consistent constraints
void generate_reachable_target(Tendril* env) {
    const int MAX_ATTEMPTS = 50;  // Prevent infinite loops
    int attempts = 0;
    
    while (attempts < MAX_ATTEMPTS) {
        // Generate candidate target within forward hemisphere only (consistent with validation)
        float candidate_x = randf(0.0f, WORKSPACE_SIZE/2);  // Only positive X (forward)
        float candidate_y = randf(-WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
        float candidate_z = randf(BASE_DEPTH + 10, BASE_DEPTH + WORKSPACE_SIZE/2);
        
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
    
    // Fallback: Generate GUARANTEED reachable target
    float safe_reach = SEGMENT_LENGTH * 0.6f;  // Conservative reach
    float safe_angles[] = {-M_PI/2, -M_PI/4, 0.0f, M_PI/4, M_PI/2};  // Forward hemisphere only
    int angle_index = rand() % 5;
    float safe_angle = safe_angles[angle_index];
    
    // Generate and validate fallback target
    float fallback_x = safe_reach * cosf(safe_angle);
    float fallback_y = safe_reach * sinf(safe_angle);
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

// Core simulation step - CLEANED UP
void c_step(Tendril* env) {
    env->tick++;
    
    // PROCESS ACTIONS - Apply joint angle changes with STRICT SERVO LIMITS
    for (int i = 0; i < NUM_JOINTS; i++) {
        // Actions are in range [-1, 1], convert to angle deltas
        // OPTIMIZED: Reduced for smoother, more precise control
        float max_delta_per_step = (SERVO_SPEED_DEG_SEC * TAU * 0.4f) * (M_PI / 180.0f); // ~2.4°/step for precision
        float requested_delta = env->actions[i] * max_delta_per_step; // FIXED: No sign flip
        
        // Apply servo speed limit
        float delta = clampf(requested_delta, -max_delta_per_step, max_delta_per_step);
        float old_angle = env->joint_angles[i];
        env->joint_angles[i] += delta;
        
        // FIXED: Clamp to safe joint limits (5-175° safety margin) - consistent with validation
        float min_safe = 5.0f * M_PI / 180.0f;    // 5° minimum
        float max_safe = 175.0f * M_PI / 180.0f;  // 175° maximum
        env->joint_angles[i] = clampf(env->joint_angles[i], min_safe, max_safe);
        
        // Update velocity based on ACTUAL change after clamping
        env->joint_velocities[i] = (env->joint_angles[i] - old_angle) / TAU;
    }
    
    // Update forward kinematics
    compute_forward_kinematics(env);
    
    // TARGET STATE LOGIC: Update stability timer and success state
    if (env->angular_error < ANGULAR_THRESHOLD_RAD) {
        env->stability_timer += TAU;
        
        // SUCCESS: Accurate pointing for required duration
        if (env->stability_timer >= STABILITY_DURATION) {
            env->target_state = TARGET_SUCCESS;
        }
    } else {
        env->stability_timer = 0.0f;  // Reset if not accurate
    }

    // REWARD CALCULATION: Use expert-optimized reward function
    env->rewards[0] = compute_reward(env);
    
    // FIXED: Update angular error tracking exactly once per step (expert feedback)
    env->last_angular_error = env->angular_error;
    
    // TRAINING EPISODE TERMINATION: Simple success-based ending
    env->terminals[0] = (env->target_state == TARGET_SUCCESS);  // Episode ends when target reached
    env->truncations[0] = (env->tick >= MAX_STEPS);             // Truncate at max steps
    
    // Update observations
    compute_observations(env);
    
    // NEW: record per-step telemetry + count hits once
    {
        float d_perp = laser_miss_distance(env);
        int i = env->log.hist_idx % METRIC_BUF;
        env->log.ang_err_hist[i] = env->angular_error;  // radians
        env->log.dperp_hist[i]   = d_perp;              // mm
        env->log.hist_idx++;
        if (env->log.hist_count < METRIC_BUF) env->log.hist_count++;
    }
    if (env->target_state == TARGET_SUCCESS && !env->episode_success_recorded) {
        env->log.hits += 1;
        env->episode_success_recorded = true;
    }
}

// Reset environment for new episode
void c_reset(Tendril* env) {
    env->episode_return = 0.0f;
    env->tick = 0;
    
    // Reset joint angles to center positions
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_angles[i] = JOINT_LIMIT_RAD / 2.0f; // 90 degrees
        env->joint_velocities[i] = 0.0f;
        
        // FIXED: Reset smoothness tracking
        env->prev_joint_vel[i] = 0.0f;
        env->last_actions[i] = 0.0f;
    }
    
    // FIXED: Reset per-env movement direction tracking
    env->last_move_dir[0] = 0.0f;
    env->last_move_dir[1] = 0.0f;
    env->last_move_dir[2] = 0.0f;
    
    // Generate servo-reachable target
    generate_reachable_target(env);
    
    // Initialize target state
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = 0.0f; // Will be set by render system if needed
    env->stability_timer = 0.0f;
    env->previous_velocity = 0.0f;
    
    // Compute initial state
    compute_forward_kinematics(env);
    compute_observations(env);
    
    // Initialize angular error for reward shaping
    env->last_angular_error = env->angular_error;
    
    // NEW: episode accounting
    env->log.episodes += 1;
    env->episode_success_recorded = false;
}

// Add performance logging
void add_log(Tendril* env) {
    // Calculate distance to target
    float dx = env->end_effector_pos[0] - env->target_pos[0];
    float dy = env->end_effector_pos[1] - env->target_pos[1]; 
    float dz = env->end_effector_pos[2] - env->target_pos[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);
    
    // Success if within 10mm of target OR angular error < 5°
    bool distance_success = distance < 10.0f;
    bool angular_success = env->angular_error < ANGULAR_THRESHOLD_RAD;
    bool success = distance_success || angular_success;
    
    env->log.success_rate += success ? 1.0f : 0.0f;
    env->log.avg_distance += distance;
    env->log.episode_length += env->tick;
    env->log.score += env->episode_return;
    env->log.n += 1.0f;
}

// FIXED: Update evaluation metrics with per-env state (no more global state!)
void update_evaluation_metrics(Tendril* env) {
    EvaluationMetrics* eval = &env->log.eval;
    
    // Calculate current movement direction
    float current_pos[3] = {
        env->end_effector_pos[0],
        env->end_effector_pos[1], 
        env->end_effector_pos[2]
    };
    
    float movement[3] = {
        current_pos[0] - env->last_end_effector_pos[0],
        current_pos[1] - env->last_end_effector_pos[1],
        current_pos[2] - env->last_end_effector_pos[2]
    };
    
    float magnitude = sqrtf(movement[0]*movement[0] + movement[1]*movement[1] + movement[2]*movement[2]);
    
    // Update movement metrics
    eval->total_path_length += magnitude;
    
    if (magnitude > 0.1f) {
        // Normalize current direction
        float current_direction[3] = {
            movement[0] / magnitude,
            movement[1] / magnitude,
            movement[2] / magnitude
        };
        
        // FIXED: Use per-env last_move_dir instead of global static
        float* last_direction = env->last_move_dir;
        
        // Check for direction changes (jitter detection)
        if (magnitude > 1.0f) {
            float dot = current_direction[0]*last_direction[0] +
                       current_direction[1]*last_direction[1] +
                       current_direction[2]*last_direction[2];
            
            if (dot < 0.5f && env->tick > 100) {
                eval->direction_changes++;
            }
            
            // Update per-env direction tracking
            last_direction[0] = current_direction[0];
            last_direction[1] = current_direction[1]; 
            last_direction[2] = current_direction[2];
        }
        
        // Update velocity tracking
        float current_velocity = magnitude / TAU;
        eval->avg_velocity = (eval->avg_velocity * env->tick + current_velocity) / (env->tick + 1);
        if (current_velocity > eval->max_velocity) {
            eval->max_velocity = current_velocity;
        }
    }
    
    // Update position tracking for next step
    env->last_end_effector_pos[0] = current_pos[0];
    env->last_end_effector_pos[1] = current_pos[1];
    env->last_end_effector_pos[2] = current_pos[2];
    
    // Update angular error tracking
    eval->avg_angular_error = (eval->avg_angular_error * env->tick + env->angular_error) / (env->tick + 1);
    if (env->angular_error < eval->best_angular_error || eval->best_angular_error == 0.0f) {
        eval->best_angular_error = env->angular_error;
    }
    if (env->angular_error > eval->worst_angular_error) {
        eval->worst_angular_error = env->angular_error;
    }
}

// Stub implementations for declared but unused functions
void init_evaluation_sequence(Tendril* env) {
    // Placeholder - implement if needed for auto-evaluation
    (void)env; // Suppress unused parameter warning
}

void generate_target_sequence(Tendril* env) {
    // Placeholder - implement if needed for auto-evaluation
    (void)env;
}

void advance_to_next_target(Tendril* env) {
    // Placeholder - implement if needed for auto-evaluation
    (void)env;
}

void detect_training_issues(Tendril* env) {
    // Placeholder - implement if needed for training diagnostics
    (void)env;
}

void print_evaluation_report(Tendril* env) {
    // Placeholder - implement if needed for evaluation reporting
    (void)env;
}