#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

// Conditional raylib inclusion - only needed for rendering
#ifndef TENDRIL_MATH_ONLY
#include "raylib.h"
#endif

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
#define DT 0.02f              // 50Hz timestep (20ms)

// MG996R servo specifications  
#define MAX_SERVO_DELTA_DEG (300.0f * DT)  // Max angle change per timestep: 6°/step @ 50Hz
                                            // (For future servo rate limiting in step_env)
#define SERVO_TORQUE_KG_CM 11.0f     // MG996R: 11 kg⋅cm stall torque
#define MAX_STEPS 1000        // Episode length
#define WORKSPACE_SIZE 120.0f // mm workspace (2 * 50mm segments + margin)

// Target pointing parameters (UPDATED for laser pointing)
#define TARGET_ACTIVE 0       // Yellow - tendril aiming toward target
#define TARGET_SUCCESS 1      // Green - pointing accurately and stable
// NO TIMEOUT STATE - Let agent try indefinitely like real hardware
#define ANGULAR_THRESHOLD_RAD (5.0f * M_PI / 180.0f)  // 5 degrees pointing accuracy
#define STABILITY_DURATION 2.0f     // Hold steady for 2 seconds after accurate pointing
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

#ifndef TENDRIL_MATH_ONLY
// PufferLib color scheme
const Color PUFF_RED = (Color){187, 0, 0, 255};
const Color PUFF_CYAN = (Color){0, 187, 187, 255};
const Color PUFF_WHITE = (Color){241, 241, 241, 241};
const Color PUFF_BACKGROUND = (Color){6, 24, 24, 255};
const Color PUFF_GREEN = (Color){0, 187, 0, 255};
#endif

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

typedef struct Log Log;
struct Log {
    float success_rate;       // Target reaching success rate
    float avg_distance;       // Average distance to target
    float movement_efficiency; // Movement smoothness metric
    float episode_length;     // Steps per episode
    float n;                  // Number of episodes
    float score;              // Cumulative score
    EvaluationMetrics eval;   // NEW: Programmatic evaluation metrics
};

#ifndef TENDRIL_MATH_ONLY
typedef struct Client Client;  
struct Client {
    // Simplified client for 2D rendering (no complex 3D camera needed)
    bool is_dragging;
    Vector2 last_mouse_pos;
};
#else
// Math-only client stub
typedef struct Client { int dummy; } Client;
#endif

typedef struct Tendril Tendril;
struct Tendril {
    // Required PufferLib fields
    float* observations;      // [joint_angles(3), end_pos(3), target_pos(3), joint_vels(3), pointing_dir(3), angular_error(1), stability_timer(1)] = 17D
    float* actions;          // [joint_angle_deltas(3)] = 3D
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
    float stability_timer;              // Time spent pointing accurately (seconds)
    float target_direction[3];          // Normalized direction from tip to target
    
    // Episode management
    int tick;                    // Current timestep
    float episode_return;        // Cumulative reward
    float last_angular_error;    // Previous angular error for progress tracking
    
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
    
    // Action consistency tracking (for smoothness penalty)
    float last_actions[NUM_JOINTS];          // Previous step's actions
    float smoothed_velocities[NUM_JOINTS];   // Exponentially smoothed velocities
    
    // Hardware simulation parameters (from real-to-sim calibration)
    float servo_backlash[NUM_JOINTS];   // Servo deadband (radians)
    float friction_coeffs[NUM_JOINTS];  // Joint friction
    float joint_limits[NUM_JOINTS][2];  // Min/max joint angles
    
    // Per-environment RNG state (avoids global rand() issues in vectorized mode)
    uint32_t rng_state;                // Per-environment LCG+XorShift RNG state
};

// SERVO REACHABILITY VALIDATION - First Principles Implementation
typedef struct {
    bool is_reachable;           // Can servos reach this target?
    float joint_angles[3];       // Required servo angles (0-180°)
    float confidence;            // Solution confidence (0-1)
    float min_distance_to_limits; // Minimum distance to any servo limit
} ReachabilityResult;

// ============================================================================
// FUNCTION PROTOTYPES - implementations in tendril_math.c
// ============================================================================

// Core mathematical functions
void compute_forward_kinematics(Tendril* env);
void compute_observations(Tendril* env);
float compute_reward(Tendril* env);

// Environment management
void init(Tendril* env);
int allocate(Tendril* env);  // Returns 0 on success, -1 on failure
void free_allocated(Tendril* env);

// Inverse kinematics and targeting
ReachabilityResult validate_target_reachability(float target_x, float target_y, float target_z);
void generate_reachable_target(Tendril* env);

// Utility functions - shared inline helpers

// Per-environment LCG+XorShift RNG (thread-safe, deterministic)
// Uses Linear Congruential Generator with XorShift output transformation
static inline uint32_t lcg32_random_r(uint32_t* state) {
    uint32_t oldstate = *state;
    *state = oldstate * 1664525u + 1013904223u;  // LCG constants
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 27u;  // Fixed: use 27 bits for 32-bit state
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static inline float randf_env(Tendril* env, float min, float max) {
    uint32_t r = lcg32_random_r(&env->rng_state);
    float f = (float)r / (float)UINT32_MAX;
    return min + f * (max - min);
}

static inline void seed_env_rng(Tendril* env, uint32_t seed) {
    env->rng_state = seed;
    // Warm up the generator
    for (int i = 0; i < 10; i++) {
        lcg32_random_r(&env->rng_state);
    }
}

// Legacy randf - use randf_env instead for vectorized environments
static inline float randf(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

static inline float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

#ifndef TENDRIL_MATH_ONLY
// ============================================================================
// RENDERING AND ENVIRONMENT FUNCTIONS - implementations in tendril.c
// ============================================================================

// Function declarations (implementations in tendril.c)
// New API (preferred names)
void reset_env(Tendril* env, uint32_t seed);
void step_env(Tendril* env);
void render_env(Tendril* env);
void close_env(Tendril* env);

// Legacy API (for backward compatibility)
void c_reset(Tendril* env, uint32_t seed);
void c_step(Tendril* env);
void c_render(Tendril* env);
void c_close(Tendril* env);
void add_log(Tendril* env);
Client* make_client(Tendril* env);
void close_client(Client* client);

// 2D Rendering function declarations (stable visualization)
void draw_2d_views(Tendril* env);
void draw_top_view(Tendril* env, Rectangle view, float scale);
void draw_side_view(Tendril* env, Rectangle view, float scale);
void draw_front_view(Tendril* env, Rectangle view, float scale);
void draw_status_overlay(Tendril* env);

// Workspace visualization functions
void draw_reachable_workspace_top(Tendril* env, Vector2 center, float scale);
void draw_reachable_workspace_side(Tendril* env, Vector2 base_pos, float scale);

// AUTOMATIC EVALUATION SYSTEM (Drone-inspired)
void init_evaluation_sequence(Tendril* env);
void generate_target_sequence(Tendril* env);
void update_evaluation_metrics(Tendril* env);
void advance_to_next_target(Tendril* env);
void calculate_movement_quality(Tendril* env);
void detect_training_issues(Tendril* env);
void print_evaluation_report(Tendril* env);
#endif // TENDRIL_MATH_ONLY