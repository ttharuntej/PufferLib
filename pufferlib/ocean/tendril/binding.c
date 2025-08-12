// Python-C binding for Tendril environment
// Auto-generated from PufferLib ocean pattern

#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include "tendril.h"

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

// Auto-generate new target after success/timeout
static void generate_new_target(Tendril* env) {
    env->target_pos[0] = randf(-WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
    env->target_pos[1] = randf(-WORKSPACE_SIZE/2, WORKSPACE_SIZE/2);
    env->target_pos[2] = randf(BASE_DEPTH, BASE_DEPTH + WORKSPACE_SIZE/2);
    
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = (float)GetTime();
    env->stability_timer = 0.0f;
    env->tick = 0; // Reset step counter for new target
}

// Raylib utility functions
static float Clamp(float value, float min, float max) {
    return clampf(value, min, max);
}

static Vector2 Vector2Subtract(Vector2 v1, Vector2 v2) {
    Vector2 result = { v1.x - v2.x, v1.y - v2.y };
    return result;
}

// C function implementations
void c_reset(Tendril* env) {
    env->episode_return = 0.0f;
    env->tick = 0;
    
    // NEW: episode accounting
    env->log.episodes += 1;
    env->episode_success_recorded = false;
    
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
            float n1 = (randf(-6, 6)) * M_PI/180.0f;
            float n2 = (randf(-6, 6)) * M_PI/180.0f;
            float n3 = (randf(-6, 6)) * M_PI/180.0f;
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
    
    // Initialize target state
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = (float)GetTime();
    env->previous_velocity = 0.0f;
    
    // NEW: Initialize pointing state
    env->angular_error = 0.0f;
    env->stability_timer = 0.0f;
    env->last_angular_error = 0.0f;
    
    // Compute initial state
    compute_forward_kinematics(env);
    compute_observations(env);
    
    // Initialize angular error for reward shaping
    env->last_angular_error = env->angular_error;
}

// Simple c_step wrapper that calls the actual implementation from tendril.h
void c_step(Tendril* env) {
    env->tick++;
    
    // Apply joint actions with servo limits
    for (int i = 0; i < NUM_JOINTS; i++) {
        float delta = env->actions[i] * (7.5f * M_PI / 180.0f); // Fine precision control
        float old_angle = env->joint_angles[i];
        float new_angle = old_angle + delta;
        
        // Enforce servo limits (0-180°) 
        float min_limit = 5.0f * M_PI / 180.0f;   // 5° safety margin
        float max_limit = 175.0f * M_PI / 180.0f; // 175° safety margin
        env->joint_angles[i] = clampf(new_angle, min_limit, max_limit);
        
        // Update velocity
        env->joint_velocities[i] = (env->joint_angles[i] - old_angle) / TAU;
    }
    
    // Update kinematics and compute reward
    compute_forward_kinematics(env);
    env->rewards[0] = compute_reward(env);
    
    // Warm-start success gate (auto-tightens after EASY_EPISODES)
    float thr  = (env->log.episodes < EASY_EPISODES) ? (8.0f * M_PI/180.0f) : ANGULAR_THRESHOLD_RAD;
    float hold = (env->log.episodes < EASY_EPISODES) ? 0.6f : STABILITY_DURATION;

    if (env->angular_error < thr) {
        env->stability_timer += TAU;
        if (env->stability_timer >= hold) {
            if (!env->success_bonus_given) {
                env->rewards[0] += 50.0f;   // add bonus first
                env->success_bonus_given = true;
            }
            env->target_state = TARGET_SUCCESS;
        }
    } else {
        env->stability_timer = 0.0f;
    }

    // ✅ accumulate AFTER all modifications to rewards[0]
    env->episode_return += env->rewards[0];
    
    // Episode termination with dynamic length
    if (env->target_state == TARGET_SUCCESS) {
        env->terminals[0] = true;
        env->truncations[0] = false;
    } else {
        int max_steps = (env->log.episodes < 800) ? 400 : 800; // Dynamic episode length
        if (env->tick >= max_steps) {
            env->terminals[0] = false;
            env->truncations[0] = true;
        } else {
            env->terminals[0] = false;
            env->truncations[0] = false;
        }
    }
    
    // Update observations
    compute_observations(env);
    
    // Telemetry
    env->ep_ang_sum += env->angular_error;
    env->ep_dperp_sum += laser_miss_distance(env);
    env->ep_steps += 1;
    
    // Auto-reset after signaling done
    if (env->terminals[0] || env->truncations[0]) {
        env->last_episode_steps = env->ep_steps;

        // >>> add this line so per-episode stats are recorded
        add_log(env);

        c_reset(env);
    }
}

void c_render(Tendril* env) {
    if (env->client == NULL) {
        env->client = make_client(env);
    }
    
    Client* client = env->client;
    
    // Handle keyboard input
    if (IsKeyPressed(KEY_ESCAPE)) {
        close_client(client);
        CloseWindow();
        exit(0);
    }
    if (IsKeyPressed(KEY_TAB)) {
        ToggleFullscreen();
    }
    
    // Mouse controls
    
    // Right-click to generate new reachable target (for manual testing)
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        generate_reachable_target(env);
        env->target_state = TARGET_ACTIVE;
        env->target_start_time = (float)GetTime();
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
}

void c_close(Tendril* env) {
    if (env->client) {
        close_client(env->client);  // calls CloseWindow() and free()
        env->client = NULL;
    }
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

Client* make_client(Tendril* env) {
    Client* client = (Client*)calloc(1, sizeof(Client));
    if (client == NULL) return NULL;
    
    // Simple 2D window initialization - much more stable
    InitWindow(WIDTH, HEIGHT, "🎯 Tendril Laser Pointer - 2D Multi-View (Stable)");
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
    char* state_text = (env->target_state == TARGET_SUCCESS) ? "SUCCESS!" : "AIMING...";
    Color state_color = (env->target_state == TARGET_SUCCESS) ? (Color){0, 187, 0, 255} : YELLOW;
    DrawText(state_text, WIDTH - 120, 20, 16, state_color);
}

// ============================================================================
// AUTOMATIC EVALUATION SYSTEM (Drone-inspired) - Same as tendril.c
// ============================================================================

void init_evaluation_sequence(Tendril* env) {
    env->auto_eval_mode = true;
    env->sequence_start_time = (float)GetTime();
    
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
    
    printf("🎯 EVALUATION SEQUENCE STARTED: %d targets\n", EVAL_SEQUENCE_LENGTH);
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
                float angle = randf(0, 2*M_PI);
                candidate[0] = radius * cosf(angle);
                candidate[1] = radius * sinf(angle);
                candidate[2] = BASE_DEPTH + 40.0f + randf(0, 20.0f);
            } else if (difficulty < 0.66f) {
                // MEDIUM targets (moderate distance)
                float radius = 30.0f + (difficulty - 0.33f) * 50.0f;
                float angle = randf(0, 2*M_PI);
                candidate[0] = radius * cosf(angle);
                candidate[1] = radius * sinf(angle);
                candidate[2] = BASE_DEPTH + 25.0f + randf(0, 40.0f);
            } else {
                // HARD targets (edge of workspace)
                float radius = 60.0f + (difficulty - 0.66f) * 40.0f;
                float angle = randf(0, 2*M_PI);
                candidate[0] = radius * cosf(angle);
                candidate[1] = radius * sinf(angle);
                candidate[2] = BASE_DEPTH + 15.0f + randf(0, 50.0f);
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
    float time_to_target = (float)GetTime() - env->target_start_time;
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
    env->target_start_time = (float)GetTime();
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
    float total_time = (float)GetTime() - env->sequence_start_time;
    
    printf("\n============================================================\n");
    printf("📊 TENDRIL EVALUATION REPORT\n");
    printf("============================================================\n");
    
    // Success metrics
    printf("🎯 TARGET PERFORMANCE:\n");
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
    printf("\n🎯 POINTING ACCURACY:\n");
    printf("   Average Angular Error: %.2f° (%.2f°)\n", 
           eval->avg_angular_error * 180/M_PI, ANGULAR_THRESHOLD_RAD * 180/M_PI);
    printf("   Best Accuracy: %.2f°\n", eval->best_angular_error * 180/M_PI);
    printf("   Worst Accuracy: %.2f°\n", eval->worst_angular_error * 180/M_PI);
    
    // Training feedback
    printf("\n🔧 TRAINING FEEDBACK:\n");
    printf("   Is Wiggly: %s\n", eval->is_wiggly ? "⚠️ YES - Consider reducing learning rate" : "✅ NO");
    printf("   Is Slow: %s\n", eval->is_slow ? "⚠️ YES - Consider reward shaping" : "✅ NO");
    printf("   Servo Limits Issue: %s\n", eval->has_servo_limits_issue ? "⚠️ YES - Check workspace" : "✅ NO");
    printf("   Overall Confidence: %.1f%% %s\n", eval->confidence_score * 100, 
           eval->confidence_score > 0.8f ? "🟢 EXCELLENT" :
           eval->confidence_score > 0.6f ? "🟡 GOOD" :
           eval->confidence_score > 0.4f ? "🟠 NEEDS WORK" : "🔴 POOR");
    
    printf("============================================================\n");
}

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
    
    srand(seed);
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
    .tp_name = "tendril.Tendril",
    .tp_doc = "Tendril environment",
    .tp_basicsize = sizeof(TendrilObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = tendril_new,
    .tp_dealloc = (destructor)tendril_dealloc,
    .tp_methods = tendril_methods,
};

// Vectorized environment management (simplified)
typedef struct {
    Tendril** envs;
    int num_envs;
} VectorizedTendril;

static PyObject* env_init(PyObject* self, PyObject* args) {
    PyObject* obs_arr, *act_arr, *rew_arr, *term_arr, *trunc_arr;
    int env_id, seed;
    
    if (!PyArg_ParseTuple(args, "OOOOOii", &obs_arr, &act_arr, &rew_arr, 
                          &term_arr, &trunc_arr, &env_id, &seed)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)calloc(1, sizeof(Tendril));
    
    // Connect Python arrays to C pointers
    env->observations = (float*)PyArray_DATA((PyArrayObject*)obs_arr);
    env->actions = (float*)PyArray_DATA((PyArrayObject*)act_arr);
    env->rewards = (float*)PyArray_DATA((PyArrayObject*)rew_arr);
    env->terminals = (bool*)PyArray_DATA((PyArrayObject*)term_arr);
    env->truncations = (bool*)PyArray_DATA((PyArrayObject*)trunc_arr);
    
    init(env);
    srand(seed);
    c_reset(env);
    
    return PyLong_FromVoidPtr(env);
}

static PyObject* vectorize(PyObject* self, PyObject* args) {
    Py_ssize_t num_envs = PyTuple_Size(args);
    VectorizedTendril* vec = (VectorizedTendril*)calloc(1, sizeof(VectorizedTendril));
    vec->envs = (Tendril**)calloc(num_envs, sizeof(Tendril*));
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
    srand(seed);
    
    for (int i = 0; i < vec->num_envs; i++) {
        c_reset(vec->envs[i]);
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_step(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    
    for (int i = 0; i < vec->num_envs; i++) {
        c_step(vec->envs[i]);
    }
    
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
    
    VectorizedTendril* vec = (VectorizedTendril*)calloc(1, sizeof(VectorizedTendril));
    vec->envs = (Tendril**)calloc(num_envs, sizeof(Tendril*));
    vec->num_envs = num_envs;
    
    for (int i = 0; i < num_envs; i++) {
        Tendril* env = (Tendril*)calloc(1, sizeof(Tendril));
        
        // Connect Python arrays to C pointers (stride by env index)
        env->observations = &((float*)PyArray_DATA((PyArrayObject*)obs_arr))[i * 17]; // 17D obs
        env->actions = &((float*)PyArray_DATA((PyArrayObject*)act_arr))[i * 3];       // 3D actions  
        env->rewards = &((float*)PyArray_DATA((PyArrayObject*)rew_arr))[i];
        env->terminals = &((bool*)PyArray_DATA((PyArrayObject*)term_arr))[i];
        env->truncations = &((bool*)PyArray_DATA((PyArrayObject*)trunc_arr))[i];
        
        init(env);
        srand(seed + i);          // seed BEFORE reset (and per-env if you want variety)
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
    
    // Return aggregated log data with telemetry
    PyObject* log_dict = PyDict_New();
    
    if (vec->num_envs > 0) {
        Tendril* env = vec->envs[0];  // Use first environment's log
        
        // ---- NEW: compute rolling stats from first env ----
        int n = env->log.hist_count;
        double meanA = 0.0, meanD = 0.0;
        for (int i = 0; i < n; i++) { meanA += env->log.ang_err_hist[i]; meanD += env->log.dperp_hist[i]; }
        if (n > 0) { meanA /= n; meanD /= n; }

        float p50A = NAN, p90A = NAN, p50D = NAN, p90D = NAN;
        if (n > 0) {
            float* A = (float*)malloc(n*sizeof(float));
            float* D = (float*)malloc(n*sizeof(float));
            memcpy(A, env->log.ang_err_hist, n*sizeof(float));
            memcpy(D, env->log.dperp_hist, n*sizeof(float));
            qsort(A, n, sizeof(float), cmp_float);
            qsort(D, n, sizeof(float), cmp_float);
            p50A = A[(int)(0.5f*(n-1))];
            p90A = A[(int)(0.9f*(n-1))];
            p50D = D[(int)(0.5f*(n-1))];
            p90D = D[(int)(0.9f*(n-1))];
            free(A); free(D);
        }

        double to_deg = 180.0 / M_PI;
        PyDict_SetItemString(log_dict, "angular_error_mean_deg", PyFloat_FromDouble(meanA * to_deg));
        PyDict_SetItemString(log_dict, "angular_error_p50_deg",  PyFloat_FromDouble(p50A * to_deg));
        PyDict_SetItemString(log_dict, "angular_error_p90_deg",  PyFloat_FromDouble(p90A * to_deg));
        PyDict_SetItemString(log_dict, "miss_distance_mean_mm",  PyFloat_FromDouble(meanD));
        PyDict_SetItemString(log_dict, "miss_distance_p50_mm",   PyFloat_FromDouble(p50D));
        PyDict_SetItemString(log_dict, "miss_distance_p90_mm",   PyFloat_FromDouble(p90D));

        int episodes = env->log.episodes;
        int hits     = env->log.hits;
        double hit_rate = (episodes > 0) ? ((double)hits / (double)episodes) : 0.0;
        PyDict_SetItemString(log_dict, "episodes", PyFloat_FromDouble((double)episodes));
        PyDict_SetItemString(log_dict, "hits",     PyFloat_FromDouble((double)hits));
        PyDict_SetItemString(log_dict, "hit_rate_episodes", PyFloat_FromDouble(hit_rate));
        
        // Traditional metrics
        PyDict_SetItemString(log_dict, "success_rate", PyFloat_FromDouble(env->log.success_rate / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "avg_distance", PyFloat_FromDouble(env->log.avg_distance / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "episode_length", PyFloat_FromDouble(env->log.episode_length / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "score", PyFloat_FromDouble(env->log.score / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "n", PyFloat_FromDouble(env->log.n));
        
        // NEW: success pop reward fix metrics
        double denom = fmaxf(env->log.n, 1.0f);
        double hit_rate_val = env->log.hit_rate / denom;
        double ang_deg_val = (env->log.mean_angular_error / denom) * 180.0 / M_PI;
        double dperp_val = env->log.mean_d_perp / denom;
        
        PyDict_SetItemString(log_dict, "angular_error_rad_mean", PyFloat_FromDouble(env->log.mean_angular_error / denom));
        PyDict_SetItemString(log_dict, "angular_error_deg_mean", PyFloat_FromDouble(ang_deg_val));
        PyDict_SetItemString(log_dict, "d_perp_mm_mean", PyFloat_FromDouble(dperp_val));
        PyDict_SetItemString(log_dict, "hit_rate_mean", PyFloat_FromDouble(hit_rate_val));
        
        // NEW: Additional metrics requested by reviewer
        
        // forward_frac: fraction of pointing directions that are forward-facing  
        double forward_count = 0.0;
        for (int i = 0; i < n; i++) {
            // Compute cosine angle between pointing direction and target direction
            // For this, we approximate: forward if angular error < 90° (cos > 0)
            if (env->log.ang_err_hist[i] < (M_PI / 2.0)) {
                forward_count += 1.0;
            }
        }
        double forward_frac = (n > 0) ? (forward_count / n) : 0.0;
        PyDict_SetItemString(log_dict, "forward_frac", PyFloat_FromDouble(forward_frac));
        
        // cosang_mean: mean cosine of angular error (forward-pointing measure)
        double cosang_sum = 0.0;
        for (int i = 0; i < n; i++) {
            cosang_sum += cos(env->log.ang_err_hist[i]);
        }
        double cosang_mean = (n > 0) ? (cosang_sum / n) : 0.0;
        PyDict_SetItemString(log_dict, "cosang_mean", PyFloat_FromDouble(cosang_mean));
        
        // steps_per_episode_last: steps in the most recently completed episode  
        PyDict_SetItemString(log_dict, "steps_per_episode_last", PyFloat_FromDouble((double)env->last_episode_steps));
        
        // BINDING VERSION FOR SANITY CHECK  
        PyDict_SetItemString(log_dict, "tendril_binding_version", PyFloat_FromDouble(20250810.0));
        
        // 60-second verification: debug print to confirm metrics flow
        if ((int)(env->log.n) % 50 == 0 && env->log.n > 0) {
            printf("[vec_log] hit=%.3f ang=%.2f° dperp=%.1fmm (n=%.0f)\n",
                   hit_rate_val, ang_deg_val, dperp_val, env->log.n);
            fflush(stdout);
        }
    }
    
    return log_dict;
}

static PyMethodDef module_methods[] = {
    {"env_init", env_init, METH_VARARGS, "Initialize environment"},
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