#ifndef TENDRIL_H_
#define TENDRIL_H_

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

// Macro for UTF-8 string portability (allows ASCII fallback on problematic toolchains)
#ifndef TDRL_TXT
  #define TDRL_TXT(x) x
#endif

// Raylib types: use real ones if raylib is enabled or already included; otherwise, light stubs
#ifdef TENDRIL_WITH_RAYLIB
#include "raylib.h"
#elif defined(RAYLIB_H)  // raylib already included elsewhere
  /* nothing */
#else
typedef struct { float x, y; } Vector2;
typedef struct { float x, y, width, height; } Rectangle;
typedef struct { unsigned char r, g, b, a; } Color;
#endif

// Portable M_PI definition
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// MG996R servo specifications
#define SERVO_SPEED_DEG_SEC 300.0f   // Increased for training (5x faster)
#define SERVO_TORQUE_KG_CM 11.0f     // MG996R: 11 kg⋅cm stall torque

// Achievable servo velocity normalization (not theoretical max)
#define MAX_JOINT_VEL_RAD_S (SERVO_SPEED_DEG_SEC * 0.4f * (M_PI/180.0f))  // ≈2.094 rad/s

// --- Servo/plant constants (3-DOF Tendril) ---
#define CTRL_DT                 (1.0f/100.0f)  // control step = 10 ms
static const float JOINT_VEL_MAX_DEG[3] = {180.f, 160.f, 160.f};   // deg/s (J0 yaw, J1 pitch, J2 pitch)
static const float JOINT_ACC_MAX_DEG[3] = {900.f, 800.f, 800.f};   // deg/s^2
static const float JOINT_MIN_DEG[3]     = {  5.f,  5.f,  10.f};   // servo domain limits [0..180°]
static const float JOINT_MAX_DEG[3]     = {175.f,120.f, 170.f};

#define CMD_LPF_ALPHA           0.35f         // command smoothing (0..1)
#define VEL_DEADBAND_DEG_S      0.20f         // small dead zone to avoid buzz
#define BACKLASH_DEG            0.20f         // gap consumed on direction flips
#define CMD_LATENCY_STEPS       2             // 2 * 10 ms = 20 ms latency
#define DAMPING_PER_SEC         0.12f         // viscous damping (12%/s)

// HARDWARE-ACCURATE STL specifications (measured from actual files)
#define BASE_WIDTH 60.0f      // Base.stl: 60×60×20mm (measured: -30 to +30)
#define BASE_HEIGHT 60.0f
#define BASE_DEPTH 20.0f

#define SEGMENT_WIDTH 50.0f   // Segment.stl: 50×50×20mm (measured: -25 to +25) 
#define SEGMENT_HEIGHT 20.0f  // STL: 20mm height (measured: 0 to 20)
#define SEGMENT_LENGTH 50.0f  // STL: 50mm length (physical hardware)

#define ENDCAP_WIDTH 30.0f    // End_Cap.stl: 30×30×15mm (measured: -15 to +15)
#define ENDCAP_LENGTH 15.0f   // STL: 15mm conical tip (measured: 0 to 15)

#define NUM_JOINTS 3          // 3 MG996R servos: Base(yaw) + Seg1(pitch) + Seg2(pitch)
#define JOINT_LIMIT_DEG 180.0f // MG996R range: 0-180 degrees
#define JOINT_LIMIT_RAD (JOINT_LIMIT_DEG * M_PI / 180.0f)

// Physics parameters
#define TAU 0.02f             // 50Hz timestep (20ms)
#define MAX_STEPS 1000        // Episode length
#define EASY_EPISODES 800     // Number of curriculum episodes with easier success criteria
#define WORKSPACE_SIZE 120.0f // mm workspace (2 * 50mm segments + margin)

// Target pointing parameters (UPDATED for laser pointing)
#define TARGET_ACTIVE 0       // Yellow - tendril aiming toward target
#define TARGET_SUCCESS 1      // Green - pointing accurately and stable
// NO TIMEOUT STATE - Let agent try indefinitely like real hardware
#define ANGULAR_THRESHOLD_RAD (5.0f * M_PI / 180.0f)  // 5 degrees pointing accuracy
#define STABILITY_DURATION 1.0f     // Hold steady for 1 second after accurate pointing (easier curriculum)
#define DISTANCE_THRESHOLD 10.0f    // Legacy distance threshold (10mm)
#define VELOCITY_THRESHOLD 0.1f     // Legacy velocity threshold (0.1 rad/s)

// AUTOMATIC EVALUATION SEQUENCE (Drone-inspired)
#define EVAL_SEQUENCE_LENGTH 10     // Test 10 targets per evaluation
#define EVAL_DIFFICULTY_LEVELS 3    // Easy, Medium, Hard target distances

// Rendering parameters
#define WIDTH 800
#define HEIGHT 600
#define SCALE 2.0f            // Scale factor for visualization

// 2D VIEW CONFIGURATION (for stable evaluation)
#define VIEW_2D_MODE 1        // Enable 2D multi-view rendering (more stable)
#define VIEW_TOP_X 50         // Top-down view position
#define VIEW_TOP_Y 50
#define VIEW_TOP_SIZE 300     // View dimensions

#define VIEW_SIDE_X 400       // Side view position  
#define VIEW_SIDE_Y 50
#define VIEW_SIDE_SIZE 300

#define VIEW_FRONT_X 50       // Front view position
#define VIEW_FRONT_Y 400
#define VIEW_FRONT_SIZE 300

// PufferLib color scheme
static const Color PUFF_RED = (Color){187, 0, 0, 255};
static const Color PUFF_CYAN = (Color){0, 187, 187, 255};
static const Color PUFF_WHITE = (Color){241, 241, 241, 241};
static const Color PUFF_BACKGROUND = (Color){6, 24, 24, 255};
static const Color PUFF_GREEN = (Color){0, 187, 0, 255};

// PROGRAMMATIC EVALUATION METRICS (Drone-inspired)
typedef struct EvaluationMetrics EvaluationMetrics;
struct EvaluationMetrics {
    // TARGET SEQUENCE METRICS
    int current_target_index;     // Current target in sequence (0-9)
    int targets_completed;        // Number of targets successfully hit
    int targets_timed_out;        // Number of targets that timed out
    float total_evaluation_time;  // Total time for full sequence
    
    // MOVEMENT QUALITY METRICS
    float total_path_length;      // Total distance moved by end effector
    float avg_velocity;           // Average movement velocity
    float max_velocity;           // Peak velocity (detect erratic movement)
    float smoothness_score;       // Movement smoothness (0-1, higher = smoother)
    int direction_changes;        // Number of direction reversals (jitter detection)
    
    // POINTING ACCURACY METRICS
    float avg_angular_error;      // Average pointing error across targets
    float best_angular_error;     // Best pointing accuracy achieved
    float worst_angular_error;    // Worst pointing accuracy
    float avg_time_to_target;     // Average time to reach each target
    
    // TRAINING FEEDBACK METRICS
    bool is_wiggly;              // Flag for excessive movement/jitter
    bool is_slow;                // Flag for slow convergence
    bool has_servo_limits_issue; // Flag for hitting servo limits frequently
    float confidence_score;      // Overall performance confidence (0-1)
};

// keep the buffer small + cheap
#define METRIC_BUF 512
typedef struct Log Log;
struct Log {
    float success_rate;       // Target reaching success rate
    float avg_distance;       // Average distance to target
    float movement_efficiency; // Movement smoothness metric
    float episode_length;     // Steps per episode
    float n;                  // Number of episodes
    float score;              // Cumulative score
    
    // NEW aggregates (episode-averaged, then averaged across episodes)
    float mean_angular_error; // radians
    float mean_d_perp;        // mm
    float hit_rate;           // [0,1], fraction of episodes with success
    EvaluationMetrics eval;   // NEW: Programmatic evaluation metrics

    // NEW: rolling telemetry buffers (only used ones)
    float ang_err_hist[METRIC_BUF];  // radians
    float dperp_hist[METRIC_BUF];    // millimeters (ray distance)
    int   hist_idx;
    int   hist_count;

    // NEW: normalization sanity checks
    float pointing_dir_norm;  // Should be ~1.0
    float target_dir_norm;    // Should be ~1.0

    // NEW: episode accounting
    int episodes;
    int hits;
    
};

typedef struct Client Client;  
struct Client {
    // Simplified client for 2D rendering (no complex 3D camera needed)
    bool is_dragging;
    Vector2 last_mouse_pos;
};

// Curriculum constants (shared across observations and rewards)
// Added an easier initial stage with a wide angular gate and short hold
// requirement to smooth early learning.
static const float TENDRIL_THRESHOLD_DEG[6] = {25.0f, 12.0f, 10.0f, 8.0f, 6.0f, 5.0f};
static const float TENDRIL_HOLD_DURATIONS[6] = {0.2f, 0.4f, 0.6f, 0.8f, 1.2f, 2.0f};
#define CURRICULUM_STAGES 6

typedef struct Tendril Tendril;
struct Tendril {
    // Required PufferLib fields
    float* observations;      // [joint_sin_cos(6), end_pos(3), target_pos(3), joint_vels(3), pointing_dir(3), angular_error(1), stability_timer(1)] = 20D
    float* actions;          // [velocity_setpoints_normalized(3)] = 3D (velocity commands in [-1,1])
    float* rewards;          // Reward signal
    bool* terminals;         // Episode termination (bool to match PufferEnv)
    bool* truncations;       // Episode truncation (bool to match PufferEnv)
    Log log;                 // Performance logging
    Client* client;          // Rendering client
    
    // Tendril state
    float joint_angles[NUM_JOINTS];     // Current joint positions (radians)
    float joint_velocities[NUM_JOINTS]; // Joint angular velocities
    float target_pos[3];                // Target XYZ position (mm)
    float end_effector_pos[3];          // Current end effector position
    
    // Laser pointing state (NEW)
    float pointing_direction[3];        // Direction vector the laser points (normalized)
    float angular_error;                // Current angular error to target (radians)
    float angular_error_ema;            // Smoothed angular error for stable gate (radians)
    float stability_timer;              // Time spent pointing accurately (seconds)
    float target_direction[3];          // Normalized direction from tip to target
    
    // Episode management
    int tick;                    // Current timestep
    float episode_return;        // Cumulative reward
    float last_angular_error;    // Previous angular error for progress tracking
    
    // Episode-level telemetry (for success pop reward fix)
    float ep_ang_sum;            // sum of angular_error over this episode
    float ep_dperp_sum;          // sum of d_perp over this episode
    int ep_steps;                // step count this episode
    int last_episode_steps;      // steps in the last completed episode (for logging)
    int ep_hit;                  // 1 if TARGET_SUCCESS happened, else 0
    float last_d_perp;           // instantaneous for logging
    bool success_bonus_given;    // prevent double-adding the success bonus
    
    // Target state management for visualization
    int target_state;            // 0=ACTIVE, 1=SUCCESS, 2=TIMEOUT, 3=TRANSITION
    float target_start_time;     // When current target was placed
    float previous_velocity;     // For velocity-based stopping
    
    // AUTOMATIC EVALUATION SEQUENCE (Drone-inspired)
    bool auto_eval_mode;         // Enable automatic target progression
    float transition_start_time; // When success pause started
    float sequence_start_time;   // When evaluation sequence began
    float target_sequence[EVAL_SEQUENCE_LENGTH][3]; // Pre-generated reachable targets [x,y,z]
    float last_end_effector_pos[3]; // For movement tracking [x,y,z]
    float movement_sample_time;    // For velocity sampling
    
    // Hardware simulation parameters (from real-to-sim calibration)
    float servo_backlash[NUM_JOINTS];   // Servo deadband (radians)
    float friction_coeffs[NUM_JOINTS];  // Joint friction
    float joint_limits[NUM_JOINTS][2];  // Min/max joint angles
    
    // FIXED: Per-joint smoothness tracking (from expert feedback)
    float prev_joint_vel[NUM_JOINTS];   // Previous joint velocities for acceleration calculation

    // NEW: to count a success once per episode
    bool episode_success_recorded;
    
    // Deferred reset flag (prevents obs/flags desync)
    bool pending_reset;
    
    // Distance-delta shaping
    float prev_dist_mm;      // previous distance to target for progress reward
    
    // Per-environment curriculum (reviewer's fix)
    uint64_t steps;          // per-env step counter
    double sched_total_steps; // steps per this env to reach full difficulty

    // NEW: Angular-error based curriculum
    int curriculum_stage;    // 0..CURRICULUM_STAGES-1 progression
    float ang_err_ema;       // EMA of per-episode angular error (deg)
    int episode_count;       // total episodes for this environment
    
    // NEW: Action smoothing and tracking
    float last_actions[NUM_JOINTS];  // previous raw actions for analysis
    float command_angles[NUM_JOINTS]; // last commanded angles for slew-rate limiting
    int limit_streak[NUM_JOINTS];    // consecutive steps near hard limit (for stall detection)
    
    // Per-environment telemetry counters (reviewer's fix)
    uint64_t limit_hits;     // servo limit violations this env
    double jitter_sum_rad_per_s; // cumulative jitter this env
    uint32_t log_steps;      // steps for telemetry normalization
    
    // NEW: Servo plant state (3-DOF) - Degrees-state for realistic servo dynamics
    float joint_deg[3];           // current joint angles [deg]
    float joint_vel_deg_s[3];     // current joint velocities [deg/s]
    float cmd_filt_deg_s[3];      // filtered command [deg/s]
    float last_cmd_norm[3];       // last raw action [-1,1], for obs/debug
    int8_t sign_prev[3];          // previous command sign (-1,0,+1)
    float backlash_remain_deg[3]; // remaining backlash to consume [deg]

    // Simple fixed-latency queue per joint (command delay)
    float cmd_queue_deg_s[3][CMD_LATENCY_STEPS];
    int   cmd_queue_idx;

    // Servo saturation accounting (per episode)
    int   servo_saturated_step;   // 0/1 last step
    int   servo_steps;            // steps in episode (denominator for limit_hits)
    
    // Per-environment thread-safe RNG state (xorshift64)
    uint64_t rng_state;      // Thread-safe per-env random number generator
};

// Thread-safe per-environment RNG functions (xorshift64)
static inline uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static inline void seed_rng(Tendril* env, uint64_t seed) {
    env->rng_state = seed ? seed : 1; // xorshift64 can't have 0 state
}

static inline float randf_env(Tendril* env, float min, float max) {
    uint64_t raw = xorshift64(&env->rng_state);
    return min + ((float)(raw >> 32) / (float)UINT32_MAX) * (max - min);
}

static inline int rand_env(Tendril* env) {
    return (int)(xorshift64(&env->rng_state) >> 32);
}

// Function declarations for PufferLib binding
#if !defined(TENDRIL_STANDALONE)
// Only exported in the binding/library build
void c_reset(Tendril* env);
void c_step(Tendril* env);
void c_render(Tendril* env);
void c_close(Tendril* env);
#endif

// Forward declaration for telemetry function
static inline void update_telemetry_sanity(Tendril* env);

// HARDWARE-ACCURATE Forward kinematics: Matches physical STL construction
static inline void compute_forward_kinematics(Tendril* env) {
    // Hardware construction chain (user specification):
    // Base.stl → Servo1(yaw) → Segment1.stl → Servo2(pitch) → Segment2.stl → Servo3(pitch) → End_Cap.stl
    
    float servo1_yaw = env->joint_angles[0];      // Base yaw: 0-180° horizontal rotation  
    float servo2_pitch = env->joint_angles[1];    // Shoulder pitch: 0-180° up/down bend
    float servo3_pitch = env->joint_angles[2];    // Elbow pitch: 0-180° up/down bend
    
    // Convert servo angles (0-180°) to proper joint angles (-90° to +90° for pitch)
    float base_yaw = servo1_yaw - M_PI/2;         // Center yaw at 0°
    float shoulder_pitch = servo2_pitch - M_PI/2; // -90° to +90° pitch range
    float elbow_pitch = servo3_pitch - M_PI/2;    // -90° to +90° pitch range
    
    // Cache trigonometry (computed multiple times in FK)
    const float cYaw = cosf(base_yaw), sYaw = sinf(base_yaw);
    const float cSh  = cosf(shoulder_pitch), sSh  = sinf(shoulder_pitch);
    const float cTot = cosf(shoulder_pitch + elbow_pitch), sTot = sinf(shoulder_pitch + elbow_pitch);
    
    // 1. BASE.STL position (origin platform) 
    float base_x = 0.0f;
    float base_y = 0.0f; 
    float base_z = BASE_DEPTH / 2;  // Half height of base platform
    
    // 2. SERVO1 position (mounted in Base.stl opening)
    float servo1_x = base_x;
    float servo1_y = base_y;
    float servo1_z = base_z + BASE_DEPTH/2;  // Top of base platform
    
    // 3. SEGMENT1.STL position (attached to Servo1 horn, rotated by base_yaw)
    // Segment extends horizontally from servo1, then rotated by yaw
    float segment1_end_x = servo1_x + SEGMENT_LENGTH * cYaw * cSh;
    float segment1_end_y = servo1_y + SEGMENT_LENGTH * sYaw * cSh;  
    float segment1_end_z = servo1_z + SEGMENT_LENGTH * sSh;
    
    // 4. SERVO2 position (mounted in Segment1.stl gap)
    float servo2_x = segment1_end_x;
    float servo2_y = segment1_end_y;
    float servo2_z = segment1_end_z;
    
    // 5. SEGMENT2.STL position (attached to Servo2 horn, rotated by shoulder+elbow pitch)
    // Second segment bends relative to first segment orientation (total_pitch = cTot/sTot)
    float segment2_end_x = servo2_x + SEGMENT_LENGTH * cYaw * cTot;
    float segment2_end_y = servo2_y + SEGMENT_LENGTH * sYaw * cTot;
    float segment2_end_z = servo2_z + SEGMENT_LENGTH * sTot;
    
    // 6. SERVO3 position (mounted in Segment2.stl gap)  
    float servo3_x = segment2_end_x;
    float servo3_y = segment2_end_y;
    float servo3_z = segment2_end_z;
    
    // 7. END_CAP.STL position (final pointing tip, attached to Servo3 horn)
    // End cap extends from servo3 position, using actual STL length (15mm)
    // End cap follows the total pitch direction (sum of all joint contributions)
    env->end_effector_pos[0] = servo3_x + ENDCAP_LENGTH * cYaw * cTot;
    env->end_effector_pos[1] = servo3_y + ENDCAP_LENGTH * sYaw * cTot;
    env->end_effector_pos[2] = servo3_z + ENDCAP_LENGTH * sTot;
    
    // NEW: Calculate pointing direction (End_Cap.stl laser beam direction)
    // Hardware chain: Servo3 rotates End_Cap.stl which determines final pointing direction
    // The end cap points in the direction of the final segment after all rotations
    // Pointing direction follows total pitch (sum of all joint contributions)
    
    // Pointing direction from Servo3 position through End_Cap.stl tip (use cached trigs)
    float pointing_x = cYaw * cTot;
    float pointing_y = sYaw * cTot;
    float pointing_z = sTot;
    
    // Normalize pointing direction vector (should already be normalized, but safety check)
    float pointing_mag = sqrtf(pointing_x*pointing_x + pointing_y*pointing_y + pointing_z*pointing_z);
    if (pointing_mag > 0.001f) {
        env->pointing_direction[0] = pointing_x / pointing_mag;
        env->pointing_direction[1] = pointing_y / pointing_mag;
        env->pointing_direction[2] = pointing_z / pointing_mag;
    } else {
        // Fallback pointing direction (shouldn't happen with proper trigonometry)
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
    
    // Keep end effector above ground (only Z constraint, no XY rescaling)
    if (env->end_effector_pos[2] < BASE_DEPTH) {
        env->end_effector_pos[2] = BASE_DEPTH;
    }
    
    // Update telemetry for sanity checks
    update_telemetry_sanity(env);
}

// Compute observation vector (20D for laser pointing)
static inline void compute_observations(Tendril* env) {
    int idx = 0;
    
    // Joint angles (sin/cos encoding to avoid wrap discontinuities) [6D]
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->observations[idx++] = sinf(env->joint_angles[i]);
        env->observations[idx++] = cosf(env->joint_angles[i]);
    }
    
    // End effector position (centered and scaled for PPO) [3D]
    const float XY_HALF = WORKSPACE_SIZE * 0.5f;
    const float Z_REF   = BASE_DEPTH + WORKSPACE_SIZE * 0.25f;  // mid working height
    env->observations[idx++] = env->end_effector_pos[0] / XY_HALF;      // ≈ [-1,1]
    env->observations[idx++] = env->end_effector_pos[1] / XY_HALF;
    env->observations[idx++] = (env->end_effector_pos[2] - Z_REF) / XY_HALF;  // centered Z
    
    // Target position (centered and scaled for PPO) [3D]
    env->observations[idx++] = env->target_pos[0] / XY_HALF;      // ≈ [-1,1]
    env->observations[idx++] = env->target_pos[1] / XY_HALF;
    env->observations[idx++] = (env->target_pos[2] - Z_REF) / XY_HALF;  // centered Z
    
    // Joint velocities (normalized by achievable servo speeds) [3D]
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->observations[idx++] = env->joint_velocities[i] / MAX_JOINT_VEL_RAD_S;
    }
    
    // NEW: Pointing direction (already normalized) [3D]
    for (int i = 0; i < 3; i++) {
        env->observations[idx++] = env->pointing_direction[i];
    }
    
    // NEW: Angular error (normalized to [0, 1], 0=perfect, 1=opposite) [1D]
    env->observations[idx++] = env->angular_error / M_PI;
    
    // NEW: Stability timer (normalized to current stage requirement) [1D]
    // Use stage-based hold duration for proper normalization
    float current_hold_req = TENDRIL_HOLD_DURATIONS[env->curriculum_stage];
    env->observations[idx++] = fminf(env->stability_timer / current_hold_req, 1.0f);
    
    // Total: 6 + 3 + 3 + 3 + 3 + 1 + 1 = 20D
}

// NEW Reward function: encourage accurate laser pointing with stability
// float compute_reward(Tendril* env) {
//     // 1. ANGULAR ACCURACY REWARD (primary objective)
//     // Perfect pointing (0°) = 1.0, opposite pointing (180°) = 0.0
//     float angular_accuracy = 1.0f - (env->angular_error / M_PI);
//     float accuracy_reward = angular_accuracy * angular_accuracy; // Quadratic for precision emphasis
    
//     // 2. PRECISION BONUSES (exponential rewards for accurate pointing)
//     float precision_bonus = 0.0f;
//     float error_degrees = env->angular_error * 180.0f / M_PI;
//     if (error_degrees < 10.0f) precision_bonus += 2.0f;  // Getting close
//     if (error_degrees < 5.0f)  precision_bonus += 5.0f;  // Very accurate (success threshold)
//     if (error_degrees < 2.0f)  precision_bonus += 10.0f; // Excellent precision
//     if (error_degrees < 1.0f)  precision_bonus += 20.0f; // Perfect laser pointing
    
//     // 3. STABILITY REWARD (encourage holding steady after accurate pointing)
//     float stability_reward = 0.0f;
//     if (env->angular_error < ANGULAR_THRESHOLD_RAD) {
//         // When pointing accurately, reward stability time
//         float stability_progress = fminf(env->stability_timer / STABILITY_DURATION, 1.0f);
//         stability_reward = stability_progress * 15.0f; // Big reward for holding steady
        
//         // MASSIVE bonus for completing full stability duration
//         if (env->stability_timer >= STABILITY_DURATION) {
//             stability_reward += 50.0f; // Mission accomplished!
//         }
//     }
    
//     // 4. PROGRESS REWARD (encourage improving angular accuracy)
//     float progress_reward = 0.0f;
//     if (env->last_angular_error > 0) {
//         float angular_improvement = env->last_angular_error - env->angular_error;
//         progress_reward = angular_improvement * 10.0f; // Reward for getting more accurate
//     }
//     env->last_angular_error = env->angular_error;
    
//     // 5. SMOOTHNESS REWARD (encourage coordinated 3-joint movements)
//     float smoothness_reward = 0.0f;
//     float total_velocity = 0.0f;
//     for (int i = 0; i < NUM_JOINTS; i++) {
//         total_velocity += fabsf(env->joint_velocities[i]);
//     }
//     // Reward smooth, controlled movements (not too fast, not too slow)
//     float ideal_velocity = 0.5f; // rad/s
//     float velocity_error = fabsf(total_velocity - ideal_velocity);
//     smoothness_reward = fmaxf(0.0f, 1.0f - velocity_error) * 2.0f;
    
//     // 6. JOINT LIMIT PENALTIES (avoid servo damage)
//     float limit_penalty = 0.0f;
//     for (int i = 0; i < NUM_JOINTS; i++) {
//         if (env->joint_angles[i] < -0.1f || env->joint_angles[i] > JOINT_LIMIT_RAD + 0.1f) {
//             limit_penalty += 5.0f; // Strong penalty for exceeding servo limits
//         }
//     }
    
//     // TOTAL REWARD COMBINATION
//     float total_reward = accuracy_reward * 3.0f +     // Primary: angular accuracy
//                         precision_bonus +              // Bonus: precision thresholds  
//                         stability_reward +             // Bonus: holding steady
//                         progress_reward +              // Bonus: improvement
//                         smoothness_reward -            // Bonus: smooth movements
//                         limit_penalty;                 // Penalty: limit violations
    
//     return total_reward;
// }
static inline float compute_reward(Tendril* env) {
    
    // SIMPLIFIED REWARD: Make gradients show up immediately
    float cosang = env->pointing_direction[0]*env->target_direction[0]
                 + env->pointing_direction[1]*env->target_direction[1]
                 + env->pointing_direction[2]*env->target_direction[2];
    cosang = fmaxf(-1.0f, fminf(1.0f, cosang));  // Clamp for safety
    
    // 2. LASER BEAM PERPENDICULAR DISTANCE 
    float rx = env->target_pos[0] - env->end_effector_pos[0];
    float ry = env->target_pos[1] - env->end_effector_pos[1];
    float rz = env->target_pos[2] - env->end_effector_pos[2];
    
    // d_perp = || r × pointing_dir || (perpendicular distance from laser beam to target)
    float cx = ry*env->pointing_direction[2] - rz*env->pointing_direction[1];
    float cy = rz*env->pointing_direction[0] - rx*env->pointing_direction[2]; 
    float cz = rx*env->pointing_direction[1] - ry*env->pointing_direction[0];
    float d_perp = sqrtf(cx*cx + cy*cy + cz*cz);
    env->last_d_perp = d_perp;  // Store for telemetry
    
    // Angular error for improvement tracking
    float d_err = env->last_angular_error - env->angular_error;
    
    // RAY-GATED MISS DISTANCE: Discourage backward pointing
    float r_mag = sqrtf(rx*rx + ry*ry + rz*rz);
    float d_perp_line = d_perp;                       // what we already computed
    float d_perp_ray  = (cosang >= 0.f) ? d_perp_line : r_mag;
    
    // ENHANCED COSINE REWARD: Make orientation matter from the start
    float cos_term = 1.0f * cosang;                   // ↑ weight (was 2.0f forward bonus)
    float perp_term = -0.005f * d_perp_ray;           // gentler (was -0.010f)
    float progress_term = 0.20f * fmaxf(0.f, d_err);  // keep same
    float back_pen = 0.05f * fmaxf(0.f, -cosang);     // lighter back penalty (was 0.2f)
    
    // Optional: proximity penalty to push away from servo limits (unified per-joint limits)
    float proximity_penalty = 0.0f;
    for (int i = 0; i < NUM_JOINTS; i++) {
        float a = env->joint_angles[i];
        float min_r = JOINT_MIN_DEG[i]*M_PI/180.f, max_r = JOINT_MAX_DEG[i]*M_PI/180.f;
        float prox = fmaxf(0.f, (min_r + 5*M_PI/180.f) - a) + 
                     fmaxf(0.f, a - (max_r - 5*M_PI/180.f));
        proximity_penalty += 0.02f * prox;
    }
    
    float shaped = cos_term + perp_term + progress_term - back_pen - proximity_penalty;
    
    // Add small baseline for early episodes to prevent deep negative returns
    if (env->log.episodes < EASY_EPISODES) {
        shaped += 0.05f;  // Small positive baseline
    }
    
    env->rewards[0] = shaped;
    return env->rewards[0];
}

// Initialize tendril environment
static inline void init(Tendril* env) {
    env->tick = 0;
    env->pending_reset = false;  // Initialize deferred reset flag
    memset(&env->log, 0, sizeof(Log));

    // NEW: Initialize curriculum/stats defaults for all builds (expert fix)
    env->curriculum_stage = 0;
    env->ang_err_ema = 0.0f;
    env->episode_count = 0;
    env->steps = 0;
    env->sched_total_steps = 0.0;  // Legacy telemetry compatibility
    
    // Initialize joint limits (0 to 180 degrees for servos)
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_limits[i][0] = 0.0f;
        env->joint_limits[i][1] = JOINT_LIMIT_RAD;
        env->servo_backlash[i] = 0.02f; // ~1 degree backlash
        env->friction_coeffs[i] = 0.1f;
        
        // FIXED: Initialize smoothness tracking (expert feedback)
        env->prev_joint_vel[i] = 0.0f;  // Previous velocities start at zero
        env->last_actions[i] = 0.0f;    // Previous actions start at zero
    }
}

// Allocate memory for PufferLib interface
static inline void allocate(Tendril* env) {
    init(env);
    env->observations = (float*)calloc(20, sizeof(float));  // 20D observation (sin/cos angles + pointing + stability)
    env->actions = (float*)calloc(3, sizeof(float));        // 3D action
    env->rewards = (float*)calloc(1, sizeof(float));
    env->terminals = (bool*)calloc(1, sizeof(bool));
    env->truncations = (bool*)calloc(1, sizeof(bool));
}

// Free allocated memory
static inline void free_allocated(Tendril* env) {
    free(env->observations); env->observations = NULL;
    free(env->actions);      env->actions = NULL;
    free(env->rewards);      env->rewards = NULL;
    free(env->terminals);    env->terminals = NULL;
    free(env->truncations);  env->truncations = NULL;
}

// Utility functions
static inline float randf(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

// Utility function: laser miss distance calculation
static inline float laser_miss_distance(Tendril* env) {
    float rx = env->target_pos[0] - env->end_effector_pos[0];
    float ry = env->target_pos[1] - env->end_effector_pos[1];
    float rz = env->target_pos[2] - env->end_effector_pos[2];
    
    // d_perp = || r × pointing_dir || (perpendicular distance from laser beam to target)
    float cx = ry*env->pointing_direction[2] - rz*env->pointing_direction[1];
    float cy = rz*env->pointing_direction[0] - rx*env->pointing_direction[2]; 
    float cz = rx*env->pointing_direction[1] - ry*env->pointing_direction[0];
    return sqrtf(cx*cx + cy*cy + cz*cz);
}

// Add telemetry for sanity checks
static inline void update_telemetry_sanity(Tendril* env) {
    // Track pointing direction norm (should be ~1.0)
    float pointing_mag = sqrtf(env->pointing_direction[0]*env->pointing_direction[0] +
                              env->pointing_direction[1]*env->pointing_direction[1] +
                              env->pointing_direction[2]*env->pointing_direction[2]);
    env->log.pointing_dir_norm = pointing_mag;
    
    // Track target direction norm (should be ~1.0) 
    float target_mag = sqrtf(env->target_direction[0]*env->target_direction[0] +
                            env->target_direction[1]*env->target_direction[1] +
                            env->target_direction[2]*env->target_direction[2]);
    env->log.target_dir_norm = target_mag;
}

// SERVO REACHABILITY VALIDATION - First Principles Implementation
typedef struct {
    bool is_reachable;           // Can servos reach this target?
    float joint_angles[3];       // Required servo angles (0-180°)
    float confidence;            // Solution confidence (0-1)
    float min_distance_to_limits; // Minimum distance to any servo limit
} ReachabilityResult;

// CORRECTED: 2D Table-Mounted Servo Inverse Kinematics (0-180° servos)
static inline ReachabilityResult validate_target_reachability(float target_x, float target_y, float target_z) {
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
    
    // Convert to servo3 angle: 90° = straight out, 0° = bent down, 180° = bent up
    float servo3_angle = M_PI/2 + (M_PI - elbow_internal_angle)/2;  // Center around 90°
    
    // 4. SOLVE SHOULDER ANGLE (servo2) - Using numerically stable atan2 approach
    float target_angle_from_horizontal = atan2f(vertical_reach, planar_reach);
    
    // More stable calculation using atan2 instead of asin
    float shoulder_correction_sin = (L2 * sinf(elbow_internal_angle)) / target_distance;
    float shoulder_correction_cos = (L1 + L2 * cosf(elbow_internal_angle)) / target_distance;
    float shoulder_correction = atan2f(shoulder_correction_sin, shoulder_correction_cos);
    
    float shoulder_angle_rad = target_angle_from_horizontal - shoulder_correction;
    
    // Convert to servo2 angle: 0° = down, 90° = horizontal, 180° = up
    float servo2_angle = shoulder_angle_rad + M_PI/2;  // Add 90° to make 90° = horizontal
    
    // 5. VALIDATE ALL SERVO LIMITS (unified JOINT_MIN/MAX_DEG) - FIXED: Match runtime constraints
    const float min_safe[3] = {
        JOINT_MIN_DEG[0]*M_PI/180.f, JOINT_MIN_DEG[1]*M_PI/180.f, JOINT_MIN_DEG[2]*M_PI/180.f };
    const float max_safe[3] = {
        JOINT_MAX_DEG[0]*M_PI/180.f, JOINT_MAX_DEG[1]*M_PI/180.f, JOINT_MAX_DEG[2]*M_PI/180.f };
    
    if (servo1_angle < min_safe[0] || servo1_angle > max_safe[0] ||
        servo2_angle < min_safe[1] || servo2_angle > max_safe[1] ||
        servo3_angle < min_safe[2] || servo3_angle > max_safe[2]) {
        return result;  // Joint limits exceeded (with safety margins)
    }
    
    // 6. ADDITIONAL 2D WORKSPACE CONSTRAINTS
    // Use unified servo limits instead of hardcoded values
    if (servo2_angle < min_safe[1]) {
        return result;  // Too close to table surface (handled by unified limits)
    }
    
    // 7. CALCULATE SOLUTION QUALITY METRICS (FIXED: Use unified safety margins)
    float limit_margins[3] = {
        fminf(servo1_angle - min_safe[0], max_safe[0] - servo1_angle),
        fminf(servo2_angle - min_safe[1], max_safe[1] - servo2_angle),
        fminf(servo3_angle - min_safe[2], max_safe[2] - servo3_angle)
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
static inline void generate_reachable_target(Tendril* env) {
    const int MAX_ATTEMPTS = 50;  // Prevent infinite loops
    int attempts = 0;
    
    // CURRICULUM: Make early targets easy to find forward signal
    int easy = env->log.episodes < 800; // first ~800 episodes across vec; tune if needed
    
    while (attempts < MAX_ATTEMPTS) {
        float candidate_x, candidate_y, candidate_z;
        if (easy) {
            // tighter cone + mid height for better gradients
            candidate_x = randf_env(env, 40.0f, 90.0f);
            candidate_y = randf_env(env, -5.0f, 5.0f);
            candidate_z = randf_env(env, BASE_DEPTH + 40.0f, BASE_DEPTH + 60.0f);
        } else {
            candidate_x = randf_env(env, 0.0f, WORKSPACE_SIZE/2);
            candidate_y = randf_env(env, -WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
            candidate_z = randf_env(env, BASE_DEPTH + 10, BASE_DEPTH + WORKSPACE_SIZE/2);
        }
        
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
    // FIXED: Only use forward-hemisphere angles (x >= 0)
    float safe_angles[] = {-M_PI/2, -M_PI/4, 0.0f, M_PI/4, M_PI/2};  // -90° to +90°
    int angle_index = rand_env(env) % 5;
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

// Additional function declarations (implementations in binding.c)
#if !defined(TENDRIL_STANDALONE)
void add_log(Tendril* env);
Client* make_client(Tendril* env);
void close_client(Client* client);
#endif

// 2D Rendering function declarations (stable visualization)
#if !defined(TENDRIL_STANDALONE)
void draw_2d_views(Tendril* env);
void draw_top_view(Tendril* env, Rectangle view, float scale);
void draw_side_view(Tendril* env, Rectangle view, float scale);
void draw_front_view(Tendril* env, Rectangle view, float scale);
void draw_status_overlay(Tendril* env);

// Workspace visualization functions
void draw_reachable_workspace_top(Tendril* env, Vector2 center, float scale);
void draw_reachable_workspace_side(Tendril* env, Vector2 base_pos, float scale);
#endif

// AUTOMATIC EVALUATION SYSTEM (Drone-inspired)
void init_evaluation_sequence(Tendril* env);
void generate_target_sequence(Tendril* env);
void update_evaluation_metrics(Tendril* env);
void advance_to_next_target(Tendril* env);
void detect_training_issues(Tendril* env);
void print_evaluation_report(Tendril* env);

#endif // TENDRIL_H_