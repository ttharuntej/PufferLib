#ifndef TENDRIL_H
#define TENDRIL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

// Forward declare raylib types to avoid forcing raylib dependency in core
typedef struct { float x, y; } TendrilVector2;
typedef struct { float x, y, width, height; } TendrilRectangle;
typedef struct { unsigned char r, g, b, a; } TendrilColor;

// HARDWARE-ACCURATE STL specifications (measured from actual files)
#define BASE_WIDTH 60.0f      // Base.stl: 60×60×20mm
#define BASE_DEPTH 20.0f      // Base height/depth (Z dimension)
#define SEGMENT_WIDTH 50.0f   // Segment.stl: 50×50×20mm
#define SEGMENT_HEIGHT 20.0f  // STL: 20mm height
#define SEGMENT_LENGTH 50.0f  // STL: 50mm length (physical hardware)
#define ENDCAP_WIDTH 30.0f    // End_Cap.stl: 30×30×15mm
#define ENDCAP_LENGTH 15.0f   // STL: 15mm conical tip

#define NUM_JOINTS 3          // 3 MG996R servos: Base(yaw) + Seg1(pitch) + Seg2(pitch)
#define JOINT_LIMIT_DEG 180.0f // MG996R range: 0-180 degrees
#define JOINT_LIMIT_RAD (JOINT_LIMIT_DEG * M_PI / 180.0f)

// MG996R servo specifications
#define SERVO_SPEED_DEG_SEC 300.0f   // Increased for training (5x faster)
#define SERVO_TORQUE_KG_CM 11.0f     // MG996R: 11 kg⋅cm stall torque

// Physics parameters
#define TAU 0.02f             // 50Hz timestep (20ms)
#define MAX_STEPS 1000        // Episode length
#define WORKSPACE_SIZE 120.0f // mm workspace (2 * 50mm segments + margin)

// Target pointing parameters (UPDATED for laser pointing)
#define TARGET_ACTIVE 0       // Yellow - tendril aiming toward target
#define TARGET_SUCCESS 1      // Green - pointing accurately and stable
#define ANGULAR_THRESHOLD_RAD (5.0f * M_PI / 180.0f)  // 5 degrees pointing accuracy
#define STABILITY_DURATION 1.0f     // Hold steady for 1 second after accurate pointing (easier curriculum)
#define DISTANCE_THRESHOLD 10.0f    // Legacy distance threshold (10mm)
#define VELOCITY_THRESHOLD 0.1f     // Legacy velocity threshold (0.1 rad/s)

// AUTOMATIC EVALUATION SEQUENCE
#define EVAL_SEQUENCE_LENGTH 10     // Test 10 targets per evaluation
#define EVAL_DIFFICULTY_LEVELS 3    // Easy, Medium, Hard target distances

// Rendering parameters
#define WIDTH 800
#define HEIGHT 600
#define SCALE 2.0f            // Scale factor for visualization

// 2D VIEW CONFIGURATION
#define VIEW_2D_MODE 1        // Enable 2D multi-view rendering
#define VIEW_TOP_X 50         // Top-down view position
#define VIEW_TOP_Y 50
#define VIEW_TOP_SIZE 300     // View dimensions
#define VIEW_SIDE_X 400       // Side view position  
#define VIEW_SIDE_Y 50
#define VIEW_SIDE_SIZE 300
#define VIEW_FRONT_X 50       // Front view position
#define VIEW_FRONT_Y 400
#define VIEW_FRONT_SIZE 300

// PufferLib color scheme (extern declarations - defined in render_2d.c)
extern const TendrilColor PUFF_RED;
extern const TendrilColor PUFF_CYAN;
extern const TendrilColor PUFF_WHITE;
extern const TendrilColor PUFF_BACKGROUND;
extern const TendrilColor PUFF_GREEN;

// PROGRAMMATIC EVALUATION METRICS
typedef struct EvaluationMetrics {
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
} EvaluationMetrics;

// keep the buffer small + cheap
#define METRIC_BUF 512
typedef struct Log {
    float success_rate;       // Target reaching success rate
    float avg_distance;       // Average distance to target
    float movement_efficiency; // Movement smoothness metric
    float episode_length;     // Steps per episode
    float n;                  // Number of episodes
    float score;              // Cumulative score
    EvaluationMetrics eval;   // Programmatic evaluation metrics

    // NEW: rolling telemetry buffers
    float ang_err_hist[METRIC_BUF];  // radians
    float dperp_hist[METRIC_BUF];    // millimeters
    int   hist_idx;
    int   hist_count;

    // NEW: episode accounting
    int episodes;
    int hits;
} Log;

typedef struct Client {
    // Simplified client for 2D rendering
    bool is_dragging;
    TendrilVector2 last_mouse_pos;
} Client;

// SERVO REACHABILITY VALIDATION
typedef struct {
    bool is_reachable;           // Can servos reach this target?
    float joint_angles[3];       // Required servo angles (0-180°)
    float confidence;            // Solution confidence (0-1)
    float min_distance_to_limits; // Minimum distance to any servo limit
} ReachabilityResult;

typedef struct Tendril {
    // Required PufferLib fields
    float* observations;      // [17D] joint_angles, end_pos, target_pos, joint_vels, pointing_dir, angular_error, stability_timer
    float* actions;          // [3D] joint_angle_deltas
    float* rewards;          // Reward signal
    bool* terminals;         // Episode termination
    bool* truncations;       // Episode truncation
    Log log;                 // Performance logging
    Client* client;          // Rendering client
    
    // Tendril state
    float joint_angles[NUM_JOINTS];     // Current joint positions (radians)
    float joint_velocities[NUM_JOINTS]; // Joint angular velocities
    float target_pos[3];                // Target XYZ position (mm)
    float end_effector_pos[3];          // Current end effector position
    
    // Laser pointing state
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
    float previous_velocity;     // For velocity-based stopping (legacy)
    
    // AUTOMATIC EVALUATION SEQUENCE
    bool auto_eval_mode;         // Enable automatic target progression
    float transition_start_time; // When success pause started
    float sequence_start_time;   // When evaluation sequence began
    float target_sequence[EVAL_SEQUENCE_LENGTH][3]; // Pre-generated reachable targets [x,y,z]
    float last_end_effector_pos[3]; // For movement tracking [x,y,z]
    float movement_sample_time;    // For velocity sampling
    
    // Hardware simulation parameters
    float servo_backlash[NUM_JOINTS];   // Servo deadband (radians)
    float friction_coeffs[NUM_JOINTS];  // Joint friction
    float joint_limits[NUM_JOINTS][2];  // Min/max joint angles
    
    // FIXED: Per-env smoothness tracking (no more global state!)
    float prev_joint_vel[NUM_JOINTS];   // Previous joint velocities for acceleration calculation
    float last_actions[NUM_JOINTS];     // Previous actions for control smoothness
    float last_move_dir[3];             // FIXED: Per-env movement direction tracking

    // NEW: to count a success once per episode
    bool episode_success_recorded;
} Tendril;

// Utility functions (static inline for header-only)
static inline float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static inline float randf(float min, float max) {
    return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

// NEW: perpendicular miss distance from beam line to target
static inline float laser_miss_distance(const Tendril* env) {
    float rx = env->target_pos[0] - env->end_effector_pos[0];
    float ry = env->target_pos[1] - env->end_effector_pos[1];
    float rz = env->target_pos[2] - env->end_effector_pos[2];
    float px = env->pointing_direction[0];
    float py = env->pointing_direction[1];
    float pz = env->pointing_direction[2];
    // |r × p|
    float cx = ry*pz - rz*py;
    float cy = rz*px - rx*pz;
    float cz = rx*py - ry*px;
    return sqrtf(cx*cx + cy*cy + cz*cz);
}

// CORE PHYSICS FUNCTIONS (implemented in tendril_core.c)
void compute_forward_kinematics(Tendril* env);
void compute_observations(Tendril* env);
float compute_reward(Tendril* env);
void init(Tendril* env);
void allocate(Tendril* env);
void free_allocated(Tendril* env);

// Core simulation functions
void c_reset(Tendril* env);
void c_step(Tendril* env);
void add_log(Tendril* env);

// Target generation and validation
ReachabilityResult validate_target_reachability(float target_x, float target_y, float target_z);
void generate_reachable_target(Tendril* env);

// Evaluation functions
void init_evaluation_sequence(Tendril* env);
void generate_target_sequence(Tendril* env);
void update_evaluation_metrics(Tendril* env);
void advance_to_next_target(Tendril* env);
void detect_training_issues(Tendril* env);
void print_evaluation_report(Tendril* env);

// RENDERING FUNCTIONS (implemented in render_2d.c)
void c_render(Tendril* env);
void c_close(Tendril* env);
Client* make_client(Tendril* env);
void close_client(Client* client);

// 2D Rendering functions
void draw_2d_views(Tendril* env);
void draw_top_view(Tendril* env, TendrilRectangle view, float scale);
void draw_side_view(Tendril* env, TendrilRectangle view, float scale);
void draw_front_view(Tendril* env, TendrilRectangle view, float scale);
void draw_status_overlay(Tendril* env);

// Workspace visualization functions
void draw_reachable_workspace_top(Tendril* env, TendrilVector2 center, float scale);
void draw_reachable_workspace_side(Tendril* env, TendrilVector2 base_pos, float scale);

#endif // TENDRIL_H