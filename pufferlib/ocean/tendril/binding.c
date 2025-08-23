// Python-C binding for Tendril environment
// Auto-generated from PufferLib ocean pattern

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>

#ifndef TENDRIL_DEFER_RESET
#define TENDRIL_DEFER_RESET 1
#endif

// Raylib enabled via build flag -DTENDRIL_WITH_RAYLIB
#ifdef TENDRIL_WITH_RAYLIB
#include "raylib.h"
#endif
#include "tendril.h"
#include <assert.h>

// Portable time function for headless compatibility
#ifdef TENDRIL_WITH_RAYLIB
static inline float now_s(void) { return (float)GetTime(); }
#else
  #if defined(_WIN32)
    #include <windows.h>
    static inline float now_s(void) {
        LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c);
        return (float)((double)c.QuadPart / (double)f.QuadPart);
    }
  #else
    #include <time.h>
    static inline float now_s(void) {
        struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
        return (float)ts.tv_sec + ts.tv_nsec * 1e-9f;
    }
  #endif
#endif

// NumPy validation helpers - fail fast instead of copying
static int expect_carray(PyObject *o, int typenum, const char *name) {
    if (!PyArray_Check(o)) { 
        PyErr_Format(PyExc_TypeError, "%s must be a numpy array", name); 
        return 0; 
    }
    PyArrayObject *a = (PyArrayObject*)o;
    if (PyArray_TYPE(a) != typenum || !PyArray_ISCARRAY(a)) {
        const char *expected_dtype = (typenum == NPY_FLOAT32) ? "float32" :
                                   (typenum == NPY_BOOL) ? "bool" : "unknown";
        PyErr_Format(PyExc_TypeError, "%s must be C-contiguous and of dtype=%s", name, expected_dtype);
        return 0;
    }
    return 1;
}

// Utility functions
static float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int cmp_float(const void* a, const void* b) {
    float x = *(const float*)a, y = *(const float*)b;
    return (x > y) - (x < y);
}

// randf function now defined in tendril.h

// Helper for sign function
static inline int sgnf(float x) { return (x > 0.f) - (x < 0.f); }

// Servo plant function - implements realistic servo dynamics (3-DOF)
static void _apply_servo_plant(struct Tendril* env, const float action01[3], float dt)
{
    int saturated_any = 0;
    float nonlin = env->servo_nonlinearity;

    // Push commands into latency queues and pull delayed cmds
    float delayed_cmd_deg_s[3];
    for (int j = 0; j < 3; ++j) {
        float a = clampf(action01[j], -1.f, 1.f);
        env->last_cmd_norm[j] = a;

        float v_cmd = a * JOINT_VEL_MAX_DEG[j]; // desired velocity setpoint [deg/s]
        env->cmd_queue_deg_s[j][env->cmd_queue_idx] = v_cmd;
        int read_idx = (env->cmd_queue_idx + 1) % CMD_LATENCY_STEPS;
        delayed_cmd_deg_s[j] = env->cmd_queue_deg_s[j][read_idx];
    }
    env->cmd_queue_idx = (env->cmd_queue_idx + 1) % CMD_LATENCY_STEPS;

    for (int j = 0; j < 3; ++j) {
        // 1) Low-pass filter (command smoothing)
        float v_ref = (1.f - CMD_LPF_ALPHA) * env->cmd_filt_deg_s[j] + CMD_LPF_ALPHA * delayed_cmd_deg_s[j];
        env->cmd_filt_deg_s[j] = v_ref;

        if (nonlin > 0.f) {
            // 2) Deadband
            if (fabsf(v_ref) < VEL_DEADBAND_DEG_S * nonlin) v_ref = 0.f;

            // 3) Backlash: on direction flip, consume BACKLASH_DEG of "gap"
            int sign_now = sgnf(v_ref);
            if (sign_now != 0 && sign_now != env->sign_prev[j]) {
                env->backlash_remain_deg[j] = BACKLASH_DEG * nonlin;
                env->sign_prev[j] = sign_now;
            }
            if (env->backlash_remain_deg[j] > 0.f && v_ref != 0.f) {
                float consume = fminf(fabsf(v_ref) * dt, env->backlash_remain_deg[j]);
                env->backlash_remain_deg[j] -= consume;
                // while backlash remains, do not move the output shaft
                v_ref = 0.f;
            }

            // 4) Acceleration limiting
            float dv = v_ref - env->joint_vel_deg_s[j];
            float dv_max = JOINT_ACC_MAX_DEG[j] * dt;
            dv_max = dv_max * nonlin + 1e6f * (1.f - nonlin);
            if (dv > dv_max) { dv = dv_max; saturated_any = 1; }
            if (dv < -dv_max){ dv = -dv_max; saturated_any = 1; }
            env->joint_vel_deg_s[j] += dv;

            // 5) Damping (viscous)
            env->joint_vel_deg_s[j] *= (1.f - DAMPING_PER_SEC * dt * nonlin);

            // 6) Velocity saturation
            float vlim = JOINT_VEL_MAX_DEG[j] * nonlin + 1e6f * (1.f - nonlin);
            if (env->joint_vel_deg_s[j] > vlim) { env->joint_vel_deg_s[j] = vlim; saturated_any = 1; }
            if (env->joint_vel_deg_s[j] < -vlim){ env->joint_vel_deg_s[j] = -vlim; saturated_any = 1; }
        } else {
            // Linear warmup mode: directly integrate filtered command
            env->joint_vel_deg_s[j] = v_ref;
        }

        // 7) Integrate angle and clamp to joint limits when fully nonlinear
        env->joint_deg[j] += env->joint_vel_deg_s[j] * dt;
        if (nonlin >= 1.f) {
            if (env->joint_deg[j] < JOINT_MIN_DEG[j]) { env->joint_deg[j] = JOINT_MIN_DEG[j]; saturated_any = 1; env->joint_vel_deg_s[j] = 0.f; }
            if (env->joint_deg[j] > JOINT_MAX_DEG[j]) { env->joint_deg[j] = JOINT_MAX_DEG[j]; saturated_any = 1; env->joint_vel_deg_s[j] = 0.f; }
        }
    }

    env->servo_saturated_step = (nonlin >= 1.f) ? saturated_any : 0;
    env->servo_steps += 1;
    if (nonlin >= 1.f && saturated_any) env->limit_hits += 1;

    // Export back to the rest of the sim (FK expects radians)
    for (int j = 0; j < 3; ++j) {
        env->joint_angles[j] = env->joint_deg[j] * (float)M_PI / 180.f;          // radians
        env->joint_velocities[j] = env->joint_vel_deg_s[j] * (float)M_PI / 180.f; // rad/s (if you track it)
    }
}

// Auto-generate new target after success/timeout
static void generate_new_target(Tendril* env) {
    env->target_pos[0] = randf_env(env, -WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
    env->target_pos[1] = randf_env(env, -WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
    env->target_pos[2] = randf_env(env, BASE_DEPTH, BASE_DEPTH + WORKSPACE_SIZE/2);
    
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = now_s();
    env->stability_timer = 0.0f;
    env->tick = 0; // Reset step counter for new target
}

// Removed unused utility functions that conflicted with raylib names

// C function implementations
void c_reset(Tendril* env) {
    env->episode_return = 0.0f;
    env->tick = 0;
    
    // NEW: episode accounting
    env->log.episodes += 1;
    env->episode_success_recorded = false;
    env->episode_count++;
    
    // Initialize curriculum system on first episode
    if (env->episode_count == 1) {
        env->curriculum_stage = 0;
        env->ang_err_ema = 0.0f;
    }

#if TENDRIL_SERVO_WARMUP
    if (env->servo_nonlinearity < 1.0f && env->hit_rate_ema >= SERVO_NONLINEAR_TARGET_HIT_RATE) {
        env->servo_nonlinearity += SERVO_NONLINEAR_STEP;
        if (env->servo_nonlinearity > 1.0f) env->servo_nonlinearity = 1.0f;
    }
#endif
    
    // Reset control state every episode (not just episode 1)
    memset(env->last_actions, 0, sizeof(env->last_actions));
    memset(env->limit_streak, 0, sizeof(env->limit_streak));
    // Initialize command angles to current joint positions
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->command_angles[i] = env->joint_angles[i];
    }
    
    
    // Legacy curriculum schedule - kept for telemetry compatibility only
    if (env->sched_total_steps <= 0) {
        env->sched_total_steps = 750000.0;  // Not used in new hit-rate curriculum
        env->steps = 0;
    }
    
    // Initialize per-env telemetry counters (reviewer's fix)
    env->limit_hits = 0;
    env->jitter_sum_rad_per_s = 0.0;
    env->log_steps = 0;
    
    // Reset episode telemetry (success pop reward fix)
    env->ep_ang_sum = 0.0f;
    env->ep_dperp_sum = 0.0f;
    env->ep_steps = 0;
    // Note: last_episode_steps is set during termination, not reset
    env->ep_hit = 0;
    env->success_bonus_given = false;
    
    // Reset joint angles to center positions
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_angles[i] = JOINT_LIMIT_RAD / 2.0f; // 90 degrees
        env->joint_velocities[i] = 0.0f;
    }
    
    // Generate servo-reachable target (same as training)
    generate_reachable_target(env);
    
    // IK warm-start for early curriculum
    if (env->log.episodes < EASY_EPISODES) {
        float min_limit = 5.0f * M_PI/180.0f, max_limit = 175.0f * M_PI/180.0f;
        ReachabilityResult sol = validate_target_reachability(
            env->target_pos[0], env->target_pos[1], env->target_pos[2]);

        if (sol.is_reachable) {
            float n1 = (randf_env(env, -6.0f, 6.0f)) * M_PI/180.0f;
            float n2 = (randf_env(env, -6.0f, 6.0f)) * M_PI/180.0f;
            float n3 = (randf_env(env, -6.0f, 6.0f)) * M_PI/180.0f;
            env->joint_angles[0] = clampf(sol.joint_angles[0] + n1, min_limit, max_limit);
            env->joint_angles[1] = clampf(sol.joint_angles[1] + n2, min_limit, max_limit);
            env->joint_angles[2] = clampf(sol.joint_angles[2] + n3, min_limit, max_limit);
        } else {
            float yaw = atan2f(env->target_pos[1], env->target_pos[0]);
            env->joint_angles[0] = clampf(yaw + M_PI/2, min_limit, max_limit);
            env->joint_angles[1] = clampf(120.0f * M_PI/180.0f, min_limit, max_limit);
            env->joint_angles[2] = clampf( 90.0f * M_PI/180.0f, min_limit, max_limit);
        }
    }
    
    // NEW: Initialize servo plant state from current servo angles (radians) w/o second-guessing IK
    for (int j = 0; j < 3; ++j) {
        env->joint_deg[j]       = env->joint_angles[j] * 180.f / (float)M_PI; // 0..180°
        // Clamp *in the same servo domain* once:
        if (env->joint_deg[j] < JOINT_MIN_DEG[j]) env->joint_deg[j] = JOINT_MIN_DEG[j];
        if (env->joint_deg[j] > JOINT_MAX_DEG[j]) env->joint_deg[j] = JOINT_MAX_DEG[j];
        env->joint_vel_deg_s[j] = 0.f;
        env->cmd_filt_deg_s[j]  = 0.f;
        env->last_cmd_norm[j]   = 0.f;
        env->sign_prev[j]       = 0;
        env->backlash_remain_deg[j] = 0.f;
        for (int k = 0; k < CMD_LATENCY_STEPS; ++k) env->cmd_queue_deg_s[j][k] = 0.f;
    }
    env->cmd_queue_idx = 0;

    // Export back in *servo domain* radians
    for (int j = 0; j < 3; ++j) {
        env->joint_angles[j] = env->joint_deg[j] * (float)M_PI / 180.f;
    }

    // Servo saturation counters (per-episode)
    env->servo_saturated_step = 0;
    env->servo_steps          = 0;
    // Note: limit_hits is already reset above in telemetry counters section
    
    // Initialize target state
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = now_s();
    env->previous_velocity = 0.0f;
    
    // NEW: Initialize pointing state
    env->angular_error = 0.0f;
    env->stability_timer = 0.0f;
    env->last_angular_error = 0.0f;
    
    // Compute initial state
    compute_forward_kinematics(env);
    compute_observations(env);
    
    // Initialize EMA on reset to prevent cold-start bias
    env->angular_error_ema = env->angular_error;
    
    // Initialize angular error for reward shaping
    env->last_angular_error = env->angular_error;
    
    // Initialize distance for distance-delta shaping
    float dx = env->target_pos[0] - env->end_effector_pos[0];
    float dy = env->target_pos[1] - env->end_effector_pos[1]; 
    float dz = env->target_pos[2] - env->end_effector_pos[2];
    env->prev_dist_mm = sqrtf(dx*dx + dy*dy + dz*dz);
}

// Simple c_step wrapper that calls the actual implementation from tendril.h
void c_step(Tendril* env) {
    // Handle deferred reset from previous step (prevents obs/flags desync)
#if TENDRIL_DEFER_RESET
    if (env->pending_reset) {
        env->pending_reset = false;
        c_reset(env);
        return; // Skip processing, just do the reset
    }
#endif
    
    env->tick++;
    
    // NEW: Servo plant model - realistic servo dynamics (replaces old action processing)
    // Actions are now velocity setpoints in [-1,1] → per-joint [deg/s]
    
    // How many control substeps per environment step?
    // Environment runs at 50Hz (TAU=0.02s), servo plant at 100Hz (CTRL_DT=0.01s)
    float ratio = TAU / CTRL_DT;
    const int ctrl_substeps = (int)(ratio + 0.5f);  // Should be 2 substeps
    
    // Ensure integer ratio to prevent time accumulation errors
    assert(fabsf(ratio - ctrl_substeps) < 1e-4f);
    
    for (int u = 0; u < ctrl_substeps; ++u) {
        _apply_servo_plant(env, env->actions /* length=3 */, CTRL_DT);
    }
    
    // NEW: Update jitter metric (sum of absolute accelerations)
    double jitter_step = 0.0;
    for (int j = 0; j < 3; ++j) {
        float dv = env->joint_velocities[j] - env->prev_joint_vel[j];
        jitter_step += fabsf(dv) / TAU;              // rad/s^2 → "jerkiness"
        env->prev_joint_vel[j] = env->joint_velocities[j];
    }
    env->jitter_sum_rad_per_s += jitter_step;
    
    // Update kinematics and compute base reward
    compute_forward_kinematics(env);
    float step_reward = compute_reward(env);
    
    // NEW: Add soft-limit penalty and stall detection (uses hardware-accurate limits)
    // (1) Soft-limit penalty - discourage living near servo limits
    float limit_pen = 0.0f;
    bool stall = false;
    for (int j = 0; j < NUM_JOINTS; j++) {
        const float min_deg = JOINT_MIN_DEG[j], max_deg = JOINT_MAX_DEG[j], band = 5.0f;
        float q_deg = env->joint_angles[j] * 180.0f / M_PI;
        
        // Soft penalty in last 5° band
        float edge = fminf(q_deg - min_deg, max_deg - q_deg);
        float x = fmaxf(0.0f, band - edge) / band;  // 0..1 in last 5°
        limit_pen += x * x;  // Stronger near edge
        
        // Stall detection - consecutive steps near hard limit
        if (q_deg < min_deg + 1.0f || q_deg > max_deg - 1.0f) {
            env->limit_streak[j]++;
        } else {
            env->limit_streak[j] = 0;
        }
        if (env->limit_streak[j] >= 40) {  // Exactly 0.8s at 50Hz (40 steps)
            stall = true;
        }
    }
    step_reward -= 0.01f * (limit_pen / NUM_JOINTS);  // Small penalty
    
    // (2) Terminal penalty for prolonged limit stalls (termination handled later)
    if (stall) {
        step_reward -= 0.05f;  // Tiny terminal penalty
    }
    
    // Increment per-env telemetry step counter (reviewer's fix)
    env->log_steps++;
    
    // ---- NEW: CURRICULUM SUCCESS GATE: Hit-rate based progression ----
    env->steps++;  // Keep for telemetry compatibility
    
    // Stage-based curriculum thresholds (reviewer's suggestion)
    int stage = env->curriculum_stage;
    float thr_deg = TENDRIL_THRESHOLD_DEG[stage];
    float hold_s = TENDRIL_HOLD_DURATIONS[stage];
    
    float thr = thr_deg * M_PI / 180.0f;
    float hold = hold_s;
    
    // EMA smoothing to reduce gate chatter (α=0.1 for stability)
    env->angular_error_ema = 0.9f * env->angular_error_ema + 0.1f * env->angular_error;

    // NEW: Leaky hold timer (eligibility trace for dense learning signal)
    if (env->angular_error_ema < thr) {
        env->stability_timer += TAU;
        if (env->stability_timer >= hold) {
            if (!env->success_bonus_given) {
                step_reward += 50.0f;            // add success bonus once
                env->success_bonus_given = true;
            }
            env->target_state = TARGET_SUCCESS;

            // Count a hit once per episode (unlocks curriculum progression)
            if (!env->episode_success_recorded) {
                env->log.hits += 1;
                env->episode_success_recorded = true;
                env->ep_hit = 1;
            }
        }
    } else {
        // Leaky decay instead of hard reset (eligibility trace style)
        // Decay ~0.5s per 1.0s outside threshold for dense learning signal
        env->stability_timer = fmaxf(0.0f, env->stability_timer - 0.5f * TAU);
    }

    // Publish final reward and accumulate once
    env->rewards[0] = step_reward;
    env->episode_return += step_reward;
    
    // Episode termination - ensure mutual exclusivity for RL trainers
    int max_steps = (env->log.episodes < 800) ? 400 : 800; // Dynamic episode length
    bool success_terminal = (env->target_state == TARGET_SUCCESS);
    bool stall_terminal = stall;  // Early termination for limit stalls
    bool timeout_truncation = (env->tick >= max_steps);
    
    env->terminals[0] = (unsigned char)((success_terminal || stall_terminal) ? 1 : 0);
    env->truncations[0] = (unsigned char)((!env->terminals[0] && timeout_truncation) ? 1 : 0);
    
    // Update observations
    compute_observations(env);
    
    // Telemetry with proper angle wrapping
    // Use absolute value of wrapped angular error for per-step accumulation
    float wrapped_ang_error = fabs(env->angular_error);
    if (wrapped_ang_error > M_PI) {
        wrapped_ang_error = 2.0f * M_PI - wrapped_ang_error;
    }
    env->ep_ang_sum += wrapped_ang_error;

    // Cache miss distance once so reward/telemetry match
    env->last_d_perp = laser_miss_distance(env);
    env->ep_dperp_sum += fabs(env->last_d_perp);  // Use absolute value
    env->ep_steps += 1;

    // NEW: push into rolling hist buffers
    int idx = env->log.hist_count % METRIC_BUF;
    env->log.ang_err_hist[idx] = env->angular_error;
    env->log.dperp_hist[idx] = env->last_d_perp;
    env->log.hist_count++;
    
    // Update for next step's jitter calculation
    env->last_angular_error = env->angular_error;
    
    // Reset semantics: deferred vs immediate (prevents obs/flags desync)
#if TENDRIL_DEFER_RESET
    // Defer reset to the *next* call to c_step (prevents obs/flags desync)
    env->pending_reset = (env->terminals[0] || env->truncations[0]);
    if (env->pending_reset) {
        env->last_episode_steps = env->ep_steps;
        
        // NEW: Update hit-rate EMA and advance curriculum
        const float EMA_ALPHA = 0.05f;  // Smoothing factor
        float ep_ang = (env->ep_steps > 0) ? (env->ep_ang_sum / env->ep_steps) : 0.0f;
        float ep_ang_deg = ep_ang * 180.0f / (float)M_PI;
        env->ang_err_ema = (1.0f - EMA_ALPHA) * env->ang_err_ema + EMA_ALPHA * ep_ang_deg;

        // Advance curriculum when average angular error is below gate threshold
        if (env->curriculum_stage < CURRICULUM_STAGES - 1 &&
            env->episode_count >= 500 &&
            env->ang_err_ema <= thr_deg) {
            env->curriculum_stage++;
            #if !defined(NDEBUG)
            printf("[CURRICULUM] Advanced to stage %d (episode %d)\n", env->curriculum_stage, env->episode_count);
            #endif
        }
        
        add_log(env);
    }
#else
    // Immediate auto-reset (original behavior)
    if (env->terminals[0] || env->truncations[0]) {
        env->last_episode_steps = env->ep_steps;
        add_log(env);
        c_reset(env);
    }
#endif
}

void c_render(Tendril* env) {
#ifdef TENDRIL_WITH_RAYLIB
    if (env->client == NULL) {
        env->client = make_client(env);
    }
    
    Client* client = env->client;
    
    // Handle keyboard input
    if (IsKeyPressed(KEY_ESCAPE)) {
        close_client(client);   // calls CloseWindow() and free()
        env->client = NULL;
        return;                 // let the caller decide what to do next
    }
    if (IsKeyPressed(KEY_TAB)) {
        ToggleFullscreen();
    }
    
    // Mouse controls
    
    // Right-click to generate new target (wide sampling with safe fallback)
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        generate_new_target(env);  // wide sampler
        ReachabilityResult r = validate_target_reachability(
            env->target_pos[0], env->target_pos[1], env->target_pos[2]);
        if (!r.is_reachable || r.confidence < 0.2f) {
            generate_reachable_target(env);  // conservative fallback
        }
        env->target_state = TARGET_ACTIVE;
        env->target_start_time = now_s();
        env->stability_timer = 0.0f;
        env->tick = 0;
    }
    
    BeginDrawing();
    ClearBackground((Color){6, 24, 24, 255}); // PUFF_BACKGROUND
    
    // STABLE 2D MULTI-VIEW RENDERING (same as tendril.c)
    draw_2d_views(env);
    
    // Status information
    draw_status_overlay(env);
    
    EndDrawing();
#else
    // Headless mode - no rendering
    (void)env;  // Suppress unused parameter warning
#endif
}

void c_close(Tendril* env) {
#ifdef TENDRIL_WITH_RAYLIB
    if (env->client) {
        close_client(env->client);  // calls CloseWindow() and free()
        env->client = NULL;
    }
#else
    // Headless mode - no cleanup needed
    (void)env;  // Suppress unused parameter warning
#endif
}

void add_log(Tendril* env) {
    // Calculate distance to target
    float dx = env->end_effector_pos[0] - env->target_pos[0];
    float dy = env->end_effector_pos[1] - env->target_pos[1]; 
    float dz = env->end_effector_pos[2] - env->target_pos[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

    bool success = (env->target_state == TARGET_SUCCESS) || (env->ep_hit == 1);

    env->log.success_rate += success ? 1.0f : 0.0f;
    env->log.avg_distance += distance;
    env->log.episode_length += env->tick;
    env->log.score += env->episode_return;

    // NEW: per-episode means (success pop reward fix)
    float ep_ang_mean = (env->ep_steps > 0) ? env->ep_ang_sum / env->ep_steps : env->angular_error;
    float ep_dperp_mean = (env->ep_steps > 0) ? env->ep_dperp_sum / env->ep_steps : env->last_d_perp;

    env->log.mean_angular_error += ep_ang_mean;
    env->log.mean_d_perp += ep_dperp_mean;
    env->log.hit_rate += success ? 1.0f : 0.0f;

    env->log.n += 1.0f; // count episodes
}

#ifdef TENDRIL_WITH_RAYLIB

Client* make_client(Tendril* env) {
    Client* client = (Client*)calloc(1, sizeof(Client));
    if (client == NULL) return NULL;
    
    // Simple 2D window initialization - much more stable
    InitWindow(WIDTH, HEIGHT, TDRL_TXT("🎯 Tendril Laser Pointer - 2D Multi-View (Stable)"));
    SetTargetFPS(60);
    
    // No complex 3D camera setup needed for 2D rendering
    // Just basic client state
    client->is_dragging = false;
    client->last_mouse_pos = (Vector2){0.0f, 0.0f};
    
    return client;
}

void close_client(Client* client) {
    if (client) {
        CloseWindow(); // Close raylib window
        free(client);
    }
}

// ============================================================================
// 2D RENDERING FUNCTIONS (Stable Visualization)
// ============================================================================

// Draw multiple 2D orthographic views (much more stable than 3D)
void draw_2d_views(Tendril* env) {
    float scale = 2.0f;  // Zoom factor for better visibility
    
    // View boundaries
    Rectangle top_view = {VIEW_TOP_X, VIEW_TOP_Y, VIEW_TOP_SIZE, VIEW_TOP_SIZE};
    Rectangle side_view = {VIEW_SIDE_X, VIEW_SIDE_Y, VIEW_SIDE_SIZE, VIEW_SIDE_SIZE};
    Rectangle front_view = {VIEW_FRONT_X, VIEW_FRONT_Y, VIEW_FRONT_SIZE, VIEW_FRONT_SIZE};
    
    // Draw view frames
    DrawRectangleLines(top_view.x-2, top_view.y-2, top_view.width+4, top_view.height+4, (Color){241, 241, 241, 241});
    DrawRectangleLines(side_view.x-2, side_view.y-2, side_view.width+4, side_view.height+4, (Color){241, 241, 241, 241});
    DrawRectangleLines(front_view.x-2, front_view.y-2, front_view.width+4, front_view.height+4, (Color){241, 241, 241, 241});
    
    // View labels
    DrawText("TOP VIEW (X-Y)", top_view.x, top_view.y-20, 14, (Color){241, 241, 241, 241});
    DrawText("SIDE VIEW (X-Z)", side_view.x, side_view.y-20, 14, (Color){241, 241, 241, 241});
    DrawText("FRONT VIEW (Y-Z)", front_view.x, front_view.y-20, 14, (Color){241, 241, 241, 241});
    
    // 1. TOP VIEW (X-Y plane) - Base rotation and horizontal positioning
    draw_top_view(env, top_view, scale);
    
    // 2. SIDE VIEW (X-Z plane) - Arm reach and vertical positioning  
    draw_side_view(env, side_view, scale);
    
    // 3. FRONT VIEW (Y-Z plane) - Front perspective
    draw_front_view(env, front_view, scale);
}

void draw_top_view(Tendril* env, Rectangle view, float scale) {
    Vector2 center = {view.x + view.width/2, view.y + view.height/2};
    
    // Draw workspace boundary (circle)
    float workspace_radius = (WORKSPACE_SIZE/2) * scale;
    DrawCircleLines(center.x, center.y, workspace_radius, GRAY);
    
    // Draw base (rectangle at center)
    float base_size = BASE_WIDTH * scale / 4;
    DrawRectangle(center.x - base_size/2, center.y - base_size/2, base_size, base_size, (Color){0, 187, 187, 255});
    
    // Calculate joint positions for top view (X-Y projection)
    float servo1_yaw = env->joint_angles[0] - M_PI/2;  // Center at 0°
    
    // Joint positions (simplified 2D projection)
    Vector2 joint1 = center;  // Base position
    Vector2 joint2 = {
        center.x + SEGMENT_LENGTH * cosf(servo1_yaw) * scale,
        center.y + SEGMENT_LENGTH * sinf(servo1_yaw) * scale
    };
    Vector2 end_pos = {
        center.x + env->end_effector_pos[0] * scale,
        center.y + env->end_effector_pos[1] * scale  
    };
    
    // Draw arm segments
    DrawLineEx(joint1, joint2, 4, (Color){187, 0, 0, 255});
    DrawLineEx(joint2, end_pos, 4, (Color){0, 187, 187, 255});
    
    // Draw joints
    DrawCircle(joint1.x, joint1.y, 6, (Color){241, 241, 241, 241});
    DrawCircle(joint2.x, joint2.y, 6, (Color){241, 241, 241, 241});
    DrawCircle(end_pos.x, end_pos.y, 8, (Color){0, 187, 0, 255});
    
    // Draw target
    Vector2 target_2d = {
        center.x + env->target_pos[0] * scale,
        center.y + env->target_pos[1] * scale
    };
    
    Color target_color = (env->target_state == TARGET_SUCCESS) ? (Color){0, 187, 0, 255} : YELLOW;
    DrawCircle(target_2d.x, target_2d.y, 10, target_color);
    
    // Draw laser pointing direction (top-down)
    Vector2 laser_end = {
        end_pos.x + env->pointing_direction[0] * 50 * scale,
        end_pos.y + env->pointing_direction[1] * 50 * scale
    };
    DrawLineEx(end_pos, laser_end, 2, (Color){187, 0, 0, 255});
}

void draw_side_view(Tendril* env, Rectangle view, float scale) {
    Vector2 center = {view.x + view.width/2, view.y + view.height/2};
    
    // Draw ground/table level
    DrawLine(view.x, center.y + BASE_DEPTH * scale, view.x + view.width, center.y + BASE_DEPTH * scale, GRAY);
    DrawText("TABLE", view.x + 5, center.y + BASE_DEPTH * scale + 5, 10, GRAY);
    
    // Draw base
    float base_size = BASE_WIDTH * scale / 4;
    DrawRectangle(center.x - base_size/2, center.y + BASE_DEPTH * scale - base_size, 
                  base_size, base_size, (Color){0, 187, 187, 255});
    
    // Side view positions (X-Z projection)
    Vector2 base_pos = {center.x, center.y + BASE_DEPTH * scale};
    Vector2 end_2d = {
        center.x + env->end_effector_pos[0] * scale,
        center.y + view.height/2 - env->end_effector_pos[2] * scale  // Flip Z for screen coords
    };
    
    // Draw arm (simplified as single line to end effector)
    DrawLineEx(base_pos, end_2d, 4, (Color){0, 187, 187, 255});
    DrawCircle(base_pos.x, base_pos.y, 6, (Color){241, 241, 241, 241});
    DrawCircle(end_2d.x, end_2d.y, 8, (Color){0, 187, 0, 255});
    
    // Draw target (side view)
    Vector2 target_2d = {
        center.x + env->target_pos[0] * scale,
        center.y + view.height/2 - env->target_pos[2] * scale
    };
    
    Color target_color = (env->target_state == TARGET_SUCCESS) ? (Color){0, 187, 0, 255} : YELLOW;
    DrawCircle(target_2d.x, target_2d.y, 10, target_color);
    
    // Show height measurements
    char height_text[32];
    sprintf(height_text, "H: %.1fmm", env->end_effector_pos[2]);
    DrawText(height_text, view.x + 5, view.y + 5, 10, (Color){241, 241, 241, 241});
}

void draw_front_view(Tendril* env, Rectangle view, float scale) {
    Vector2 center = {view.x + view.width/2, view.y + view.height/2};
    
    // Draw ground/table level
    DrawLine(view.x, center.y + BASE_DEPTH * scale, view.x + view.width, center.y + BASE_DEPTH * scale, GRAY);
    
    // Front view positions (Y-Z projection)
    Vector2 base_pos = {center.x, center.y + BASE_DEPTH * scale};
    Vector2 end_2d = {
        center.x + env->end_effector_pos[1] * scale,
        center.y + view.height/2 - env->end_effector_pos[2] * scale
    };
    
    // Draw arm
    DrawLineEx(base_pos, end_2d, 4, (Color){0, 187, 187, 255});
    DrawCircle(base_pos.x, base_pos.y, 6, (Color){241, 241, 241, 241});
    DrawCircle(end_2d.x, end_2d.y, 8, (Color){0, 187, 0, 255});
    
    // Draw target (front view)
    Vector2 target_2d = {
        center.x + env->target_pos[1] * scale,
        center.y + view.height/2 - env->target_pos[2] * scale
    };
    
    Color target_color = (env->target_state == TARGET_SUCCESS) ? (Color){0, 187, 0, 255} : YELLOW;
    DrawCircle(target_2d.x, target_2d.y, 10, target_color);
}

void draw_status_overlay(Tendril* env) {
    int y_offset = HEIGHT - 120;
    
    // Performance metrics
    char info[256];
    sprintf(info, "Joint Angles: [%.1f°, %.1f°, %.1f°]", 
            env->joint_angles[0] * 180/M_PI,
            env->joint_angles[1] * 180/M_PI, 
            env->joint_angles[2] * 180/M_PI);
    DrawText(info, 10, y_offset, 12, (Color){241, 241, 241, 241});
    
    sprintf(info, "End Effector: (%.1f, %.1f, %.1f)mm", 
            env->end_effector_pos[0], env->end_effector_pos[1], env->end_effector_pos[2]);
    DrawText(info, 10, y_offset + 15, 12, (Color){241, 241, 241, 241});
    
    sprintf(info, "Target: (%.1f, %.1f, %.1f)mm", 
            env->target_pos[0], env->target_pos[1], env->target_pos[2]);
    DrawText(info, 10, y_offset + 30, 12, (Color){241, 241, 241, 241});
    
    sprintf(info, "Angular Error: %.2f° | Stability: %.1fs", 
            env->angular_error * 180/M_PI, env->stability_timer);
    DrawText(info, 10, y_offset + 45, 12, (Color){241, 241, 241, 241});
    
    // Controls
    DrawText("ESC: Exit | TAB: Fullscreen | Right Click: New Target", 10, HEIGHT - 20, 12, GRAY);
    
    // Target state indicator
    const char* state_text = (env->target_state == TARGET_SUCCESS) ? "SUCCESS!" : "AIMING...";
    Color state_color = (env->target_state == TARGET_SUCCESS) ? (Color){0, 187, 0, 255} : YELLOW;
    DrawText(state_text, WIDTH - 120, 20, 16, state_color);
}

// ============================================================================
// AUTOMATIC EVALUATION SYSTEM (Drone-inspired) - Same as tendril.c
// ============================================================================

void init_evaluation_sequence(Tendril* env) {
    env->auto_eval_mode = true;
    env->sequence_start_time = now_s();
    
    // Reset evaluation metrics
    memset(&env->log.eval, 0, sizeof(EvaluationMetrics));
    env->log.eval.best_angular_error = M_PI;  // Start with worst possible
    env->log.eval.worst_angular_error = 0.0f;
    
    // Generate sequence of reachable targets
    generate_target_sequence(env);
    
    // Set first target
    env->target_pos[0] = env->target_sequence[0][0];
    env->target_pos[1] = env->target_sequence[0][1];
    env->target_pos[2] = env->target_sequence[0][2];
    
    printf(TDRL_TXT("🎯 EVALUATION SEQUENCE STARTED: %d targets\n"), EVAL_SEQUENCE_LENGTH);
}

void generate_target_sequence(Tendril* env) {
    printf("Generating evaluation target sequence...\n");
    
    for (int i = 0; i < EVAL_SEQUENCE_LENGTH; i++) {
        // Generate targets with varying difficulty levels
        float difficulty = (float)i / (float)(EVAL_SEQUENCE_LENGTH - 1);  // 0.0 to 1.0
        
        bool found_valid = false;
        int attempts = 0;
        
        while (!found_valid && attempts < 100) {
            float candidate[3];
            
            if (difficulty < 0.33f) {
                // EASY targets (close to center, high up)
                float radius = 20.0f + difficulty * 30.0f;
                float angle = randf_env(env, 0, 2*M_PI);
                candidate[0] = radius * cosf(angle);
                candidate[1] = radius * sinf(angle);
                candidate[2] = BASE_DEPTH + 40.0f + randf_env(env, 0, 20.0f);
            } else if (difficulty < 0.66f) {
                // MEDIUM targets (moderate distance)
                float radius = 30.0f + (difficulty - 0.33f) * 50.0f;
                float angle = randf_env(env, 0, 2*M_PI);
                candidate[0] = radius * cosf(angle);
                candidate[1] = radius * sinf(angle);
                candidate[2] = BASE_DEPTH + 25.0f + randf_env(env, 0, 40.0f);
            } else {
                // HARD targets (edge of workspace)
                float radius = 60.0f + (difficulty - 0.66f) * 40.0f;
                float angle = randf_env(env, 0, 2*M_PI);
                candidate[0] = radius * cosf(angle);
                candidate[1] = radius * sinf(angle);
                candidate[2] = BASE_DEPTH + 15.0f + randf_env(env, 0, 50.0f);
            }
            
            // Validate reachability
            ReachabilityResult reach = validate_target_reachability(candidate[0], candidate[1], candidate[2]);
            
            if (reach.is_reachable && reach.confidence > 0.3f) {
                env->target_sequence[i][0] = candidate[0];
                env->target_sequence[i][1] = candidate[1];
                env->target_sequence[i][2] = candidate[2];
                found_valid = true;
                printf("Target %d: (%.1f, %.1f, %.1f) - Confidence: %.2f\n", 
                       i+1, candidate[0], candidate[1], candidate[2], reach.confidence);
            }
            
            attempts++;
        }
        
        if (!found_valid) {
            // Fallback to safe target
            env->target_sequence[i][0] = 20.0f;
            env->target_sequence[i][1] = 0.0f;
            env->target_sequence[i][2] = BASE_DEPTH + 30.0f;
            printf("Target %d: FALLBACK - (20.0, 0.0, %.1f)\n", i+1, BASE_DEPTH + 30.0f);
        }
    }
}

void update_evaluation_metrics(Tendril* env) {
    EvaluationMetrics* eval = &env->log.eval;
    
    // Sample movement every 10 steps to avoid computational overhead
    if (env->tick % 10 == 0) {
        // Calculate movement metrics
        if (env->tick > 0) {
            float dx = env->end_effector_pos[0] - env->last_end_effector_pos[0];
            float dy = env->end_effector_pos[1] - env->last_end_effector_pos[1];
            float dz = env->end_effector_pos[2] - env->last_end_effector_pos[2];
            float movement_distance = sqrtf(dx*dx + dy*dy + dz*dz);
            
            eval->total_path_length += movement_distance;
            
            // Velocity calculation (mm/s)
            float dt = 10 * TAU;  // Time since last sample
            float velocity = movement_distance / dt;
            eval->avg_velocity = (eval->avg_velocity * env->tick/10 + velocity) / (env->tick/10 + 1);
            
            if (velocity > eval->max_velocity) {
                eval->max_velocity = velocity;
            }
            
            // Direction change detection (jitter)
            static float last_direction[3] = {0, 0, 0};
            float current_direction[3] = {dx, dy, dz};
            float magnitude = sqrtf(dx*dx + dy*dy + dz*dz);
            
            if (magnitude > 1.0f) {  // Only count significant movements
                // Normalize current direction
                current_direction[0] /= magnitude;
                current_direction[1] /= magnitude;
                current_direction[2] /= magnitude;
                
                // Check if direction changed significantly (dot product < 0.5)
                float dot = current_direction[0] * last_direction[0] +
                           current_direction[1] * last_direction[1] +
                           current_direction[2] * last_direction[2];
                
                if (dot < 0.5f && env->tick > 100) {  // Allow initial settling
                    eval->direction_changes++;
                }
                
                // Update last direction
                last_direction[0] = current_direction[0];
                last_direction[1] = current_direction[1];
                last_direction[2] = current_direction[2];
            }
        }
        
        // Store current position for next calculation
        env->last_end_effector_pos[0] = env->end_effector_pos[0];
        env->last_end_effector_pos[1] = env->end_effector_pos[1];
        env->last_end_effector_pos[2] = env->end_effector_pos[2];
    }
    
    // Update angular error statistics
    eval->avg_angular_error = (eval->avg_angular_error * env->tick + env->angular_error) / (env->tick + 1);
    
    if (env->angular_error < eval->best_angular_error) {
        eval->best_angular_error = env->angular_error;
    }
    if (env->angular_error > eval->worst_angular_error) {
        eval->worst_angular_error = env->angular_error;
    }
    
    // Detect training issues
    detect_training_issues(env);
}

void advance_to_next_target(Tendril* env) {
    EvaluationMetrics* eval = &env->log.eval;
    
    // Record time to reach current target
    float time_to_target = now_s() - env->target_start_time;
    eval->avg_time_to_target = (eval->avg_time_to_target * eval->current_target_index + time_to_target) / 
                               (eval->current_target_index + 1);
    
    // Move to next target
    eval->current_target_index++;
    
    if (eval->current_target_index >= EVAL_SEQUENCE_LENGTH) {
        // Evaluation sequence complete!
        printf("\n🏁 EVALUATION SEQUENCE COMPLETE!\n");
        print_evaluation_report(env);
        env->auto_eval_mode = false;
        return;
    }
    
    // Set next target
    env->target_pos[0] = env->target_sequence[eval->current_target_index][0];
    env->target_pos[1] = env->target_sequence[eval->current_target_index][1];
    env->target_pos[2] = env->target_sequence[eval->current_target_index][2];
    
    // Reset state for new target
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = now_s();
    env->stability_timer = 0.0f;
    env->tick = 0;  // Reset step counter for new target
    
    printf("→ Advanced to Target %d: (%.1f, %.1f, %.1f)\n", 
           eval->current_target_index + 1, env->target_pos[0], env->target_pos[1], env->target_pos[2]);
}

void detect_training_issues(Tendril* env) {
    EvaluationMetrics* eval = &env->log.eval;
    
    // Detect wiggliness (excessive direction changes)
    float jitter_rate = (float)eval->direction_changes / fmaxf(1.0f, env->tick / 100.0f);
    eval->is_wiggly = (jitter_rate > 3.0f);  // More than 3 direction changes per 100 steps
    
    // Detect slowness (high time to target)
    eval->is_slow = (eval->avg_time_to_target > 15.0f);  // Taking more than 15 seconds per target
    
    // Detect servo limit issues (tracking joint limit violations)
    eval->has_servo_limits_issue = false;
    for (int i = 0; i < NUM_JOINTS; i++) {
        if (env->joint_angles[i] <= 0.1f || env->joint_angles[i] >= JOINT_LIMIT_RAD - 0.1f) {
            eval->has_servo_limits_issue = true;
            break;
        }
    }
    
    // Calculate overall confidence score
    float success_rate = (float)eval->targets_completed / fmaxf(1.0f, eval->current_target_index + 1);
    float smoothness_factor = fmaxf(0.0f, 1.0f - jitter_rate / 5.0f);
    float speed_factor = fmaxf(0.0f, 1.0f - eval->avg_time_to_target / 30.0f);
    float accuracy_factor = fmaxf(0.0f, 1.0f - eval->avg_angular_error / ANGULAR_THRESHOLD_RAD);
    
    eval->confidence_score = (success_rate * 0.4f + smoothness_factor * 0.2f + 
                             speed_factor * 0.2f + accuracy_factor * 0.2f);
}

void print_evaluation_report(Tendril* env) {
    EvaluationMetrics* eval = &env->log.eval;
    float total_time = now_s() - env->sequence_start_time;
    
    printf("\n============================================================\n");
    printf("📊 TENDRIL EVALUATION REPORT\n");
    printf("============================================================\n");
    
    // Success metrics
    printf(TDRL_TXT("🎯 TARGET PERFORMANCE:\n"));
    printf("   Targets Completed: %d/%d (%.1f%%)\n", 
           eval->targets_completed, EVAL_SEQUENCE_LENGTH,
           100.0f * eval->targets_completed / EVAL_SEQUENCE_LENGTH);
    printf("   Average Time per Target: %.1fs\n", eval->avg_time_to_target);
    printf("   Total Evaluation Time: %.1fs\n", total_time);
    
    // Movement quality
    printf("\n🤖 MOVEMENT QUALITY:\n");
    printf("   Total Path Length: %.1fmm\n", eval->total_path_length);
    printf("   Average Velocity: %.1fmm/s\n", eval->avg_velocity);
    printf("   Max Velocity: %.1fmm/s\n", eval->max_velocity);
    printf("   Direction Changes: %d (jitter indicator)\n", eval->direction_changes);
    
    // Pointing accuracy
    printf(TDRL_TXT("\n🎯 POINTING ACCURACY:\n"));
    printf("   Average Angular Error: %.2f° (%.2f°)\n", 
           eval->avg_angular_error * 180/M_PI, ANGULAR_THRESHOLD_RAD * 180/M_PI);
    printf("   Best Accuracy: %.2f°\n", eval->best_angular_error * 180/M_PI);
    printf("   Worst Accuracy: %.2f°\n", eval->worst_angular_error * 180/M_PI);
    
    // Training feedback
    printf("\n🔧 TRAINING FEEDBACK:\n");
    printf("   Is Wiggly: %s\n", eval->is_wiggly ? TDRL_TXT("⚠️ YES - Consider reducing learning rate") : TDRL_TXT("✅ NO"));
    printf("   Is Slow: %s\n", eval->is_slow ? TDRL_TXT("⚠️ YES - Consider reward shaping") : TDRL_TXT("✅ NO"));
    printf("   Servo Limits Issue: %s\n", eval->has_servo_limits_issue ? TDRL_TXT("⚠️ YES - Check workspace") : TDRL_TXT("✅ NO"));
    printf("   Overall Confidence: %.1f%% %s\n", eval->confidence_score * 100, 
           eval->confidence_score > 0.8f ? TDRL_TXT("🟢 EXCELLENT") :
           eval->confidence_score > 0.6f ? TDRL_TXT("🟡 GOOD") :
           eval->confidence_score > 0.4f ? TDRL_TXT("🟠 NEEDS WORK") : TDRL_TXT("🔴 POOR"));
    
    printf("============================================================\n");
}

#else
// Headless mode - provide no-op stubs for rendering functions
Client* make_client(Tendril* env) { (void)env; return NULL; }
void close_client(Client* client) { (void)client; }
void draw_2d_views(Tendril* env) { (void)env; }
void draw_top_view(Tendril* env, Rectangle view, float scale) { (void)env; (void)view; (void)scale; }
void draw_side_view(Tendril* env, Rectangle view, float scale) { (void)env; (void)view; (void)scale; }
void draw_front_view(Tendril* env, Rectangle view, float scale) { (void)env; (void)view; (void)scale; }
void draw_status_overlay(Tendril* env) { (void)env; }
void init_evaluation_sequence(Tendril* env) { (void)env; }
void generate_target_sequence(Tendril* env) { (void)env; }
void update_evaluation_metrics(Tendril* env) { (void)env; }
void advance_to_next_target(Tendril* env) { (void)env; }
void detect_training_issues(Tendril* env) { (void)env; }
void print_evaluation_report(Tendril* env) { (void)env; }
#endif

// Python wrapper for C environment
typedef struct {
    PyObject_HEAD
    Tendril* env;
} TendrilObject;

static PyObject* tendril_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    TendrilObject* self = (TendrilObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->env = (Tendril*)calloc(1, sizeof(Tendril));
        if (self->env == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        allocate(self->env);
    }
    return (PyObject*)self;
}

static void tendril_dealloc(TendrilObject* self) {
    if (self->env) {
        free_allocated(self->env);
        c_close(self->env);
        free(self->env);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* tendril_reset(TendrilObject* self, PyObject* args) {
    int seed = 0;
    if (!PyArg_ParseTuple(args, "|i", &seed)) {
        return NULL;
    }
    
    seed_rng(self->env, seed);
    c_reset(self->env);
    Py_RETURN_NONE;
}

static PyObject* tendril_step(TendrilObject* self, PyObject* args) {
    c_step(self->env);
    Py_RETURN_NONE;
}

static PyObject* tendril_render(TendrilObject* self, PyObject* args) {
    c_render(self->env);
    Py_RETURN_NONE;
}

static PyObject* tendril_close(TendrilObject* self, PyObject* args) {
    c_close(self->env);
    Py_RETURN_NONE;
}

static PyMethodDef tendril_methods[] = {
    {"reset", (PyCFunction)tendril_reset, METH_VARARGS, "Reset environment"},
    {"step", (PyCFunction)tendril_step, METH_NOARGS, "Step environment"},  
    {"render", (PyCFunction)tendril_render, METH_NOARGS, "Render environment"},
    {"close", (PyCFunction)tendril_close, METH_NOARGS, "Close environment"},
    {NULL}
};

static PyTypeObject TendrilType = {
    PyVarObject_HEAD_INIT(NULL, 0)
};

// Vectorized environment management (with lifetime management)
typedef struct {
    Tendril** envs;
    int num_envs;
    // Keep Python array references alive to prevent use-after-free
    PyObject *obs_arr, *act_arr, *rew_arr, *term_arr, *trunc_arr;
} VectorizedTendril;

// Single-environment initialization
// NOTE: Caller must keep arrays alive for the environment's lifetime
// (or use vec_init which retains references)
static PyObject* env_init(PyObject* self, PyObject* args) {
    PyObject* obs_arr, *act_arr, *rew_arr, *term_arr, *trunc_arr;
    int env_id, seed;
    
    if (!PyArg_ParseTuple(args, "OOOOOii", &obs_arr, &act_arr, &rew_arr, 
                          &term_arr, &trunc_arr, &env_id, &seed)) {
        return NULL;
    }
    
    // Validate NumPy arrays early - fail fast instead of copying
    if (!expect_carray(obs_arr,  NPY_FLOAT32, "obs")
     || !expect_carray(act_arr,  NPY_FLOAT32, "act") 
     || !expect_carray(rew_arr,  NPY_FLOAT32, "rew")
     || !expect_carray(term_arr, NPY_BOOL,    "term")
     || !expect_carray(trunc_arr,NPY_BOOL,    "trunc")) {
        return NULL;
    }

    // Validate array shapes (expert fix: single-env needs shape validation too)
    PyArrayObject* oA = (PyArrayObject*)obs_arr;
    PyArrayObject* aA = (PyArrayObject*)act_arr;
    PyArrayObject* rA = (PyArrayObject*)rew_arr;
    PyArrayObject* tA = (PyArrayObject*)term_arr;
    PyArrayObject* cA = (PyArrayObject*)trunc_arr;

    if (PyArray_NDIM(oA) != 1 || PyArray_SIZE(oA) != 20) {
        PyErr_SetString(PyExc_ValueError, "obs must be 1D float32 of length 20");
        return NULL;
    }
    if (PyArray_NDIM(aA) != 1 || PyArray_SIZE(aA) != 3) {
        PyErr_SetString(PyExc_ValueError, "act must be 1D float32 of length 3");
        return NULL;
    }
    if (!((PyArray_NDIM(rA)==0 && PyArray_TYPE(rA)==NPY_FLOAT32) ||
          (PyArray_NDIM(rA)==1 && PyArray_SIZE(rA)==1 && PyArray_TYPE(rA)==NPY_FLOAT32))) {
        PyErr_SetString(PyExc_ValueError, "rew must be float32 scalar or shape (1,)");
        return NULL;
    }
    if (!((PyArray_NDIM(tA)==0 && PyArray_TYPE(tA)==NPY_BOOL) ||
          (PyArray_NDIM(tA)==1 && PyArray_SIZE(tA)==1 && PyArray_TYPE(tA)==NPY_BOOL))) {
        PyErr_SetString(PyExc_ValueError, "term must be bool scalar or shape (1,)");
        return NULL;
    }
    if (!((PyArray_NDIM(cA)==0 && PyArray_TYPE(cA)==NPY_BOOL) ||
          (PyArray_NDIM(cA)==1 && PyArray_SIZE(cA)==1 && PyArray_TYPE(cA)==NPY_BOOL))) {
        PyErr_SetString(PyExc_ValueError, "trunc must be bool scalar or shape (1,)");
        return NULL;
    }
    
    Tendril* env = (Tendril*)calloc(1, sizeof(Tendril));
    if (env == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    
    // Connect Python arrays to C pointers
    env->observations = (float*)PyArray_DATA((PyArrayObject*)obs_arr);
    env->actions = (float*)PyArray_DATA((PyArrayObject*)act_arr);
    env->rewards = (float*)PyArray_DATA((PyArrayObject*)rew_arr);
    env->terminals = (unsigned char*)PyArray_DATA((PyArrayObject*)term_arr);
    env->truncations = (unsigned char*)PyArray_DATA((PyArrayObject*)trunc_arr);
    
    init(env);
    seed_rng(env, seed);
    c_reset(env);
    
    return PyLong_FromVoidPtr(env);
}

static PyObject* env_close(PyObject* self, PyObject* args) {
    PyObject* env_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &env_ptr)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (env) {
        c_close(env);
        free(env);
    }
    
    Py_RETURN_NONE;
}

static PyObject* vectorize(PyObject* self, PyObject* args) {
    Py_ssize_t num_envs = PyTuple_Size(args);
    VectorizedTendril* vec = (VectorizedTendril*)calloc(1, sizeof(VectorizedTendril));
    if (vec == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    vec->envs = (Tendril**)calloc(num_envs, sizeof(Tendril*));
    if (vec->envs == NULL) {
        free(vec);
        PyErr_NoMemory();
        return NULL;
    }
    vec->num_envs = num_envs;
    
    for (Py_ssize_t i = 0; i < num_envs; i++) {
        PyObject* env_ptr = PyTuple_GetItem(args, i);
        vec->envs[i] = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    }
    
    return PyLong_FromVoidPtr(vec);
}

static PyObject* vec_reset(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    int seed;
    
    if (!PyArg_ParseTuple(args, "Oi", &vec_ptr, &seed)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    // Re-seed per-environment RNG if explicitly requested  
    if (seed >= 0) {
        for (int i = 0; i < vec->num_envs; i++) {
            seed_rng(vec->envs[i], seed + i);
        }
    }
    
    // Release GIL around tight loop to prevent blocking Python
    Py_BEGIN_ALLOW_THREADS
    for (int i = 0; i < vec->num_envs; i++) {
        c_reset(vec->envs[i]);
    }
    Py_END_ALLOW_THREADS
    
    Py_RETURN_NONE;
}

static PyObject* vec_step(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    
    // Release GIL around tight loop to prevent blocking Python
    Py_BEGIN_ALLOW_THREADS
    for (int i = 0; i < vec->num_envs; i++) {
        c_step(vec->envs[i]);
    }
    Py_END_ALLOW_THREADS
    
    Py_RETURN_NONE;
}

static PyObject* vec_render(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    int env_id;
    
    if (!PyArg_ParseTuple(args, "Oi", &vec_ptr, &env_id)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    
    if (env_id >= 0 && env_id < vec->num_envs) {
        c_render(vec->envs[env_id]);
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_close(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    
    for (int i = 0; i < vec->num_envs; i++) {
        c_close(vec->envs[i]);
        free(vec->envs[i]);
    }
    
    // Release Python array references to allow proper cleanup
    Py_XDECREF(vec->obs_arr);
    Py_XDECREF(vec->act_arr);
    Py_XDECREF(vec->rew_arr);
    Py_XDECREF(vec->term_arr);
    Py_XDECREF(vec->trunc_arr);
    
    free(vec->envs);
    free(vec);
    
    Py_RETURN_NONE;
}

static PyObject* vec_init(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* obs_arr, *act_arr, *rew_arr, *term_arr, *trunc_arr;
    int num_envs, seed;
    
    if (!PyArg_ParseTuple(args, "OOOOOii", &obs_arr, &act_arr, &rew_arr, 
                          &term_arr, &trunc_arr, &num_envs, &seed)) {
        return NULL;
    }
    
    // Validate NumPy arrays early - fail fast instead of copying  
    if (!expect_carray(obs_arr,  NPY_FLOAT32, "obs")
     || !expect_carray(act_arr,  NPY_FLOAT32, "act")
     || !expect_carray(rew_arr,  NPY_FLOAT32, "rew") 
     || !expect_carray(term_arr, NPY_BOOL,    "term")
     || !expect_carray(trunc_arr,NPY_BOOL,    "trunc")) {
        return NULL;
    }
    
    // Validate array shapes/strides we'll index into
    PyArrayObject* obs_array = (PyArrayObject*)obs_arr;
    PyArrayObject* act_array = (PyArrayObject*)act_arr;
    
    // Check that arrays have expected dimensionality and tight packing
    if (PyArray_NDIM(obs_array) != 2 || PyArray_SHAPE(obs_array)[1] != 20 ||
        PyArray_STRIDES(obs_array)[1] != sizeof(float)) {
        PyErr_SetString(PyExc_ValueError, "obs array must be (num_envs, 20) with tight float32 packing");
        return NULL;
    }
    
    if (PyArray_NDIM(act_array) != 2 || PyArray_SHAPE(act_array)[1] != 3 ||
        PyArray_STRIDES(act_array)[1] != sizeof(float)) {
        PyErr_SetString(PyExc_ValueError, "act array must be (num_envs, 3) with tight float32 packing");
        return NULL;
    }
    
    // Belt-and-suspenders: assert tightly packed along axis 0
    npy_intp obs_s0 = PyArray_STRIDES(obs_array)[0];
    npy_intp act_s0 = PyArray_STRIDES(act_array)[0];
    if (obs_s0 != 20 * (npy_intp)sizeof(float)) {
        PyErr_SetString(PyExc_ValueError, "obs array must be tightly packed along axis 0");
        return NULL;
    }
    if (act_s0 != 3 * (npy_intp)sizeof(float)) {
        PyErr_SetString(PyExc_ValueError, "act array must be tightly packed along axis 0");
        return NULL;
    }
    
    // Validate reward, terminal, and truncation array shapes
    PyArrayObject* rew_array = (PyArrayObject*)rew_arr;
    PyArrayObject* term_array = (PyArrayObject*)term_arr;
    PyArrayObject* trunc_array = (PyArrayObject*)trunc_arr;
    
    if (PyArray_NDIM(rew_array) != 1 || PyArray_SHAPE(rew_array)[0] != num_envs) {
        PyErr_SetString(PyExc_ValueError, "rew array must be (num_envs,) shape");
        return NULL;
    }
    
    if (PyArray_NDIM(term_array) != 1 || PyArray_SHAPE(term_array)[0] != num_envs) {
        PyErr_SetString(PyExc_ValueError, "term array must be (num_envs,) shape");
        return NULL;
    }
    
    if (PyArray_NDIM(trunc_array) != 1 || PyArray_SHAPE(trunc_array)[0] != num_envs) {
        PyErr_SetString(PyExc_ValueError, "trunc array must be (num_envs,) shape");
        return NULL;
    }
    
    // Verify num_envs matches leading dimension (redundant now but kept for clarity)
    if (PyArray_SHAPE(obs_array)[0] != num_envs || PyArray_SHAPE(act_array)[0] != num_envs) {
        PyErr_SetString(PyExc_ValueError, "Array leading dimensions must match num_envs");
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)calloc(1, sizeof(VectorizedTendril));
    if (vec == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    vec->envs = (Tendril**)calloc(num_envs, sizeof(Tendril*));
    if (vec->envs == NULL) {
        free(vec);
        PyErr_NoMemory();
        return NULL;
    }
    vec->num_envs = num_envs;
    
    // Keep Python array references alive to prevent use-after-free
    vec->obs_arr = obs_arr;     Py_INCREF(obs_arr);
    vec->act_arr = act_arr;     Py_INCREF(act_arr);
    vec->rew_arr = rew_arr;     Py_INCREF(rew_arr);
    vec->term_arr = term_arr;   Py_INCREF(term_arr);
    vec->trunc_arr = trunc_arr; Py_INCREF(trunc_arr);
    
    for (int i = 0; i < num_envs; i++) {
        Tendril* env = (Tendril*)calloc(1, sizeof(Tendril));
        if (env == NULL) {
            // Cleanup already allocated environments
            for (int j = 0; j < i; j++) {
                free(vec->envs[j]);
            }
            free(vec->envs);
            Py_DECREF(vec->obs_arr);
            Py_DECREF(vec->act_arr);
            Py_DECREF(vec->rew_arr);
            Py_DECREF(vec->term_arr);
            Py_DECREF(vec->trunc_arr);
            free(vec);
            PyErr_NoMemory();
            return NULL;
        }
        
        // Connect Python arrays to C pointers (stride by env index)
        env->observations = &((float*)PyArray_DATA((PyArrayObject*)obs_arr))[i * 20]; // 20D obs
        env->actions = &((float*)PyArray_DATA((PyArrayObject*)act_arr))[i * 3];       // 3D actions  
        env->rewards = &((float*)PyArray_DATA((PyArrayObject*)rew_arr))[i];
        env->terminals = &((unsigned char*)PyArray_DATA((PyArrayObject*)term_arr))[i];
        env->truncations = &((unsigned char*)PyArray_DATA((PyArrayObject*)trunc_arr))[i];
        
        init(env);
        seed_rng(env, seed + i);  // Initialize per-env thread-safe RNG
        c_reset(env);
        
        vec->envs[i] = env;
    }
    
    return PyLong_FromVoidPtr(vec);
}

static PyObject* vec_log(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    
    // Return aggregated log data across all environments
    PyObject* log_dict = PyDict_New();
    
    if (vec->num_envs > 0) {
        // Aggregate stats across all environments
        double total_hits = 0, total_episodes = 0, total_mean_ang = 0, total_mean_d = 0, total_n = 0;
        double total_limit_hits = 0, total_jitter = 0, total_log_steps = 0;
        
        // Collect histogram data from all environments
        int total_hist_samples = 0;
        for (int e = 0; e < vec->num_envs; e++) {
            Tendril* env = vec->envs[e];
            total_hits += env->log.hits;
            total_episodes += env->log.episodes;
            total_mean_ang += env->log.mean_angular_error;
            total_mean_d += env->log.mean_d_perp;
            total_n += env->log.n;
            total_limit_hits += env->limit_hits;
            total_jitter += env->jitter_sum_rad_per_s;
            total_log_steps += env->log_steps;
            
            // Count valid histogram samples
            int env_samples = env->log.hist_count < METRIC_BUF ? env->log.hist_count : METRIC_BUF;
            total_hist_samples += env_samples;
        }
        
        // Use first environment for histogram analysis (representative sample)
        Tendril* env = vec->envs[0];
        
        // ---- NEW: pull a wrapped window from the hist ring ----
        int cap = METRIC_BUF;                   // e.g., 512 in tendril.h
        int total = env->log.hist_count;        // how many samples ever pushed
        int n = total < cap ? total : cap;      // how many are valid to read now

        double meanA = 0.0, meanD = 0.0;
        float p50A = NAN, p90A = NAN, p50D = NAN, p90D = NAN;

        if (n > 0) {
            int start = (total - n) % cap; if (start < 0) start += cap;

            float *A = (float*)malloc(n*sizeof(float));
            float *D = (float*)malloc(n*sizeof(float));

            for (int i = 0; i < n; i++) {
                int j = (start + i) % cap;
                float a = env->log.ang_err_hist[j];
                float d = env->log.dperp_hist[j];

                // guards
                if (!(a >= 0.0f && a <= (float)M_PI)) a = fminf(fmaxf(a, 0.0f), (float)M_PI);
                if (!(d >= 0.0f)) d = fmaxf(d, 0.0f);

                A[i] = a; D[i] = d;
                meanA += a; meanD += d;
            }
            meanA /= n; meanD /= n;

            qsort(A, n, sizeof(float), cmp_float);
            qsort(D, n, sizeof(float), cmp_float);
            p50A = A[(int)(0.5f*(n-1))];
            p90A = A[(int)(0.9f*(n-1))];
            p50D = D[(int)(0.5f*(n-1))];
            p90D = D[(int)(0.9f*(n-1))];

            free(A); free(D);
        }

        // Keep only miss distance metrics (angular error handled below with canonical naming)
        PyDict_SetItemString(log_dict, "miss_distance_mean_mm",  PyFloat_FromDouble(meanD));
        PyDict_SetItemString(log_dict, "miss_distance_p50_mm",   PyFloat_FromDouble(p50D));
        PyDict_SetItemString(log_dict, "miss_distance_p90_mm",   PyFloat_FromDouble(p90D));

        // Aggregated metrics across all environments
        double hit_rate = (total_episodes > 0) ? (total_hits / total_episodes) : 0.0;
        PyDict_SetItemString(log_dict, "episodes", PyFloat_FromDouble(total_episodes));
        PyDict_SetItemString(log_dict, "hits",     PyFloat_FromDouble(total_hits));
        PyDict_SetItemString(log_dict, "hit_rate_episodes", PyFloat_FromDouble(hit_rate));
        
        // Traditional metrics (averaged across environments)
        PyDict_SetItemString(log_dict, "success_rate", PyFloat_FromDouble(total_n > 0 ? hit_rate : 0.0));
        PyDict_SetItemString(log_dict, "avg_distance", PyFloat_FromDouble(total_mean_d / vec->num_envs));
        PyDict_SetItemString(log_dict, "episode_length", PyFloat_FromDouble(total_episodes > 0 ? total_log_steps / total_episodes : 0.0));
        PyDict_SetItemString(log_dict, "score", PyFloat_FromDouble(hit_rate * 100.0));  // Simple score metric
        PyDict_SetItemString(log_dict, "n", PyFloat_FromDouble(total_n));
        
        // NEW: Aggregated reward metrics - properly average per-episode means
        double hit_rate_val = total_n > 0 ? hit_rate : 0.0;
        // total_mean_ang is already sum of per-episode means, divide by number of episodes not environments
        double avg_ang_rad = total_n > 0 ? (total_mean_ang / total_n) : 0.0;
        double avg_dperp_mm = total_n > 0 ? (total_mean_d / total_n) : 0.0;
        double ang_deg_val = avg_ang_rad * 180.0 / M_PI;
        
        PyDict_SetItemString(log_dict, "angular_error_rad_mean", PyFloat_FromDouble(avg_ang_rad));
        PyDict_SetItemString(log_dict, "angular_error_deg_mean", PyFloat_FromDouble(ang_deg_val));
        PyDict_SetItemString(log_dict, "d_perp_mm_mean", PyFloat_FromDouble(avg_dperp_mm));
        PyDict_SetItemString(log_dict, "hit_rate_mean", PyFloat_FromDouble(hit_rate_val));
        
        // NEW: Additional metrics requested by reviewer
        
        // forward_frac & cosang_mean computed over the same n/window
        double forward_count = 0.0, cosang_sum = 0.0;
        for (int i = 0; i < n; i++) {
            // reuse wrapped indices
            int j = ( (total - n) + i ) % cap; if (j < 0) j += cap;
            float a = env->log.ang_err_hist[j];
            if (!(a >= 0.0f && a <= (float)M_PI)) a = fminf(fmaxf(a, 0.0f), (float)M_PI);
            if (a < (float)M_PI * 0.5f) forward_count += 1.0;
            cosang_sum += cosf(a);
        }
        double forward_frac = (n > 0) ? (forward_count / n) : 0.0;
        double cosang_mean = (n > 0) ? (cosang_sum / n) : 0.0;
        PyDict_SetItemString(log_dict, "forward_frac", PyFloat_FromDouble(forward_frac));
        PyDict_SetItemString(log_dict, "cosang_mean", PyFloat_FromDouble(cosang_mean));
        
        // ---- NEW: CURRICULUM GATE TELEMETRY (Stage-based) ---- 
        int stage = env->curriculum_stage;
        float gate_thr_deg = TENDRIL_THRESHOLD_DEG[stage];
        float gate_hold_s = TENDRIL_HOLD_DURATIONS[stage];
        
        PyDict_SetItemString(log_dict, "curriculum_stage", PyFloat_FromDouble((double)stage));
        PyDict_SetItemString(log_dict, "ang_err_ema", PyFloat_FromDouble(env->ang_err_ema));
        PyDict_SetItemString(log_dict, "gate_thr_deg", PyFloat_FromDouble(gate_thr_deg));
        PyDict_SetItemString(log_dict, "gate_hold_s", PyFloat_FromDouble(gate_hold_s));
        
        // NEW: Action smoothing and limit metrics
        PyDict_SetItemString(log_dict, "max_step_deg", PyFloat_FromDouble(1.0));  // Slew rate limit
        
        // Limit streak stats (max across all environments and joints)
        int max_limit_streak = 0;
        for (int e = 0; e < vec->num_envs; e++) {
            for (int j = 0; j < NUM_JOINTS; j++) {
                if (vec->envs[e]->limit_streak[j] > max_limit_streak) {
                    max_limit_streak = vec->envs[e]->limit_streak[j];
                }
            }
        }
        PyDict_SetItemString(log_dict, "max_limit_streak", PyFloat_FromDouble((double)max_limit_streak));
        
        // ---- AGGREGATED SERVO BEHAVIOR TELEMETRY ----
        // Servo limit hit rate: % per 100 step·joint (aggregated across all envs)
        int dof = 3;  // NUM_JOINTS
        double denom_servo = total_log_steps * (double)dof;
        double servo_limit_hit_rate = (denom_servo > 0) ? (100.0 * total_limit_hits / denom_servo) : 0.0;
        PyDict_SetItemString(log_dict, "servo_limit_hit_rate", PyFloat_FromDouble(servo_limit_hit_rate));
        
        // Jitter rate: average angular velocity magnitude (aggregated)
        double jitter_rate = (total_log_steps > 0) ? (total_jitter / total_log_steps) : 0.0;
        PyDict_SetItemString(log_dict, "jitter_rate", PyFloat_FromDouble(jitter_rate));
        
        // steps_per_episode_last: from first env (representative)
        PyDict_SetItemString(log_dict, "steps_per_episode_last", PyFloat_FromDouble((double)env->last_episode_steps));
        
        // (Optional) sanity bump
        PyDict_SetItemString(log_dict, "tendril_binding_version", PyFloat_FromDouble(20250812.1));
        
        // 60-second verification: debug print to confirm metrics flow (commented for clean training)
        // if ((int)(env->log.n) % 50 == 0 && env->log.n > 0) {
        //     printf("[vec_log] hit=%.3f ang=%.2f° dperp=%.1fmm (n=%.0f)\n",
        //            hit_rate_val, ang_deg_val, dperp_val, env->log.n);
        //     fflush(stdout);
        // }
    }
    
    return log_dict;
}

static PyMethodDef module_methods[] = {
    {"env_init", env_init, METH_VARARGS, "Initialize environment"},
    {"env_close", env_close, METH_VARARGS, "Close single environment"},
    {"vectorize", vectorize, METH_VARARGS, "Create vectorized environments"},
    {"vec_init", (PyCFunction)vec_init, METH_VARARGS | METH_KEYWORDS, "Initialize vectorized environments"},
    {"vec_reset", vec_reset, METH_VARARGS, "Reset vectorized environments"},
    {"vec_step", vec_step, METH_VARARGS, "Step vectorized environments"},
    {"vec_render", vec_render, METH_VARARGS, "Render vectorized environments"},
    {"vec_close", vec_close, METH_VARARGS, "Close vectorized environments"},
    {"vec_log", vec_log, METH_VARARGS, "Get vectorized environment logs"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "binding",
    "Tendril environment C bindings",
    -1,
    module_methods
};

PyMODINIT_FUNC PyInit_binding(void) {
    import_array();
    
    PyObject* module = PyModule_Create(&module_definition);
    if (module == NULL) {
        return NULL;
    }
    
    // Initialize type object (MSVC-compatible)
    TendrilType.tp_name = "binding.Tendril";
    TendrilType.tp_doc = "Tendril environment";
    TendrilType.tp_basicsize = sizeof(TendrilObject);
    TendrilType.tp_itemsize = 0;
    TendrilType.tp_flags = Py_TPFLAGS_DEFAULT;
    TendrilType.tp_new = tendril_new;
    TendrilType.tp_dealloc = (destructor)tendril_dealloc;
    TendrilType.tp_methods = tendril_methods;
    
    if (PyType_Ready(&TendrilType) < 0) {
        return NULL;
    }
    
    Py_INCREF(&TendrilType);
    if (PyModule_AddObject(module, "Tendril", (PyObject*)&TendrilType) < 0) {
        Py_DECREF(&TendrilType);
        Py_DECREF(module);
        return NULL;
    }
    
    return module;
}