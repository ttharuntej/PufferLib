#include "tendril.h"

// randf is already defined in tendril.h

// Clamp value between min and max
static inline float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// 2D rendering doesn't need complex camera controls - much simpler and more stable!

// Create rendering client with 2D interface (STABLE VERSION)
Client* make_client(Tendril* env) {
    Client* client = (Client*)calloc(1, sizeof(Client));
    
    // Simple 2D window initialization - much more stable
    InitWindow(WIDTH, HEIGHT, "🎯 Tendril Laser Pointer - 2D Multi-View (Stable)");
    SetTargetFPS(60);
    
    // No complex 3D camera setup needed for 2D rendering
    // Just basic client state
    client->is_dragging = false;
    client->last_mouse_pos = (Vector2){0.0f, 0.0f};
    
    return client;
}

// Close rendering client
void close_client(Client* client) {
    CloseWindow();
    free(client);
}

void c_close(Tendril* env) {
    if (env->client) {
        close_client(env->client);
        env->client = NULL;
    }
}

// Add performance logging
void add_log(Tendril* env) {
    // Calculate distance to target
    float dx = env->end_effector_pos[0] - env->target_pos[0];
    float dy = env->end_effector_pos[1] - env->target_pos[1]; 
    float dz = env->end_effector_pos[2] - env->target_pos[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);
    
    // Success if within 10mm of target
    bool success = distance < 10.0f;
    
    env->log.success_rate += success ? 1.0f : 0.0f;
    env->log.avg_distance += distance;
    env->log.episode_length += env->tick;
    env->log.score += env->episode_return;
    env->log.n += 1.0f;
}

// Reset environment for new episode
void c_reset(Tendril* env) {
    env->episode_return = 0.0f;
    env->tick = 0;
    
    // Reset joint angles to center positions
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->joint_angles[i] = JOINT_LIMIT_RAD / 2.0f; // 90 degrees
        env->joint_velocities[i] = 0.0f;
    }
    
    // Generate servo-reachable target (same as training)
    generate_reachable_target(env);
    
    // CURRICULUM HELPER: Set base yaw toward target for easy episodes
    if (env->log.episodes < 50) {
        float yaw = atan2f(env->target_pos[1], env->target_pos[0]); // [-pi,pi]
        float servo1 = yaw + M_PI/2; // map to [0,pi] nominal
        float min_limit = 5.0f * M_PI/180.0f, max_limit = 175.0f * M_PI/180.0f;
        env->joint_angles[0] = clampf(servo1, min_limit, max_limit);
    }
    
    // Initialize target state
    env->target_state = TARGET_ACTIVE;
    env->target_start_time = (float)GetTime();
    env->previous_velocity = 0.0f;
    
    // Compute initial state
    compute_forward_kinematics(env);
    compute_observations(env);
    
    // Initialize last angular error for reward shaping
    env->last_angular_error = env->angular_error;
    
    // NEW: episode accounting
    env->log.episodes += 1;
    env->episode_success_recorded = false;
}

// Physics simulation step - FIXED VERSION
// void c_step(Tendril* env) {
//     env->tick++;
    
//     // PROCESS ACTIONS - Apply joint angle changes (only if target is active)
//     if (env->target_state == TARGET_ACTIVE) {
//         for (int i = 0; i < NUM_JOINTS; i++) {
//             // Actions are in range [-1, 1], convert to angle deltas
//             float delta = env->actions[i] * (15.0f * M_PI / 180.0f); // 15 degrees per step for faster movement
//             env->joint_angles[i] += delta;
            
//             // Clamp to joint limits (0 to 180 degrees in radians)
//             env->joint_angles[i] = clampf(env->joint_angles[i], 0.0f, JOINT_LIMIT_RAD);
            
//             // Update velocity (simple finite difference)
//             env->joint_velocities[i] = delta / TAU;
//         }
//     }
    
//     // Update forward kinematics
//     compute_forward_kinematics(env);
    
//     // Check stopping conditions (only during evaluation with visualization)
//     if (env->target_state == TARGET_ACTIVE) {
//         float dx = env->end_effector_pos[0] - env->target_pos[0];
//         float dy = env->end_effector_pos[1] - env->target_pos[1]; 
//         float dz = env->end_effector_pos[2] - env->target_pos[2];
//         float distance = sqrtf(dx*dx + dy*dy + dz*dz);
        
//         // Calculate velocity magnitude
//         float velocity = 0.0f;
//         for (int i = 0; i < NUM_JOINTS; i++) {
//             velocity += fabsf(env->joint_velocities[i]);
//         }
        
//         // Check success condition: close enough AND moving slowly
//         if (distance < DISTANCE_THRESHOLD && velocity < VELOCITY_THRESHOLD) {
//             env->target_state = TARGET_SUCCESS;
//         }
        
//         // Check timeout condition
//         float elapsed_time = (float)GetTime() - env->target_start_time;
//         if (elapsed_time > TIMEOUT_SECONDS) {
//             env->target_state = TARGET_TIMEOUT;
//         }
//     }
    
//     // Compute reward based on distance to target
//     env->rewards[0] = compute_reward(env);
    
//     // Check for episode termination
//     env->terminals[0] = (env->tick >= MAX_STEPS);
//     env->truncations[0] = false;
    
//     // Update observations
//     compute_observations(env);
// }

// Physics simulation step - FIXED VERSION
void c_step(Tendril* env) {
    env->tick++;
    
    // PROCESS ACTIONS - Apply joint angle changes with STRICT SERVO LIMITS
    for (int i = 0; i < NUM_JOINTS; i++) {
        // Actions are in range [-1, 1], convert to angle deltas
        float delta = env->actions[i] * (10.0f * M_PI / 180.0f); // Reduced to 10° per step for better control
        float old_angle = env->joint_angles[i]; // Store previous angle
        float new_angle = old_angle + delta;
        
        // CRITICAL: Enforce MG996R servo limits (0-180°) with safety margins
        float min_limit = 5.0f * M_PI / 180.0f;   // 5° minimum (servo deadband)
        float max_limit = 175.0f * M_PI / 180.0f; // 175° maximum (servo deadband)
        
        env->joint_angles[i] = clampf(new_angle, min_limit, max_limit);
        
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

    // REWARD CALCULATION: Use sophisticated reward function
    env->rewards[0] = compute_reward(env);
    
    // STATE UPDATE: Prepare for next step's reward calculation
    env->last_angular_error = env->angular_error;
    
    // NO TIMEOUT: Let agent try indefinitely (as user requested)
    
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

// 2D visualization of tendril (STABLE VERSION)
void c_render(Tendril* env) {
    // Handle window controls
    if (IsKeyDown(KEY_ESCAPE)) {
        c_close(env);
        exit(0);
    }
    
    if (IsKeyPressed(KEY_TAB)) {
        ToggleFullscreen();
    }
    
    // Initialize client if needed
    if (env->client == NULL) {
        env->client = make_client(env);
    }
    
    // Right-click to generate new VALIDATED reachable target (for manual testing)
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        printf("🎯 Generating new validated reachable target...\n");
        generate_reachable_target(env);
        
        // Validate the new target
        ReachabilityResult validation = validate_target_reachability(
            env->target_pos[0], env->target_pos[1], env->target_pos[2]);
        
        printf("Target: (%.1f, %.1f, %.1f)mm - %s (Confidence: %.1f%%)\n",
               env->target_pos[0], env->target_pos[1], env->target_pos[2],
               validation.is_reachable ? "REACHABLE" : "UNREACHABLE",
               validation.confidence * 100);
        
        env->target_state = TARGET_ACTIVE;
        env->target_start_time = (float)GetTime();
        env->stability_timer = 0.0f;
        env->tick = 0;
    }
    
    // Left-click to force target to cursor position (for testing unreachable targets)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = GetMousePosition();
        
        // Convert screen coordinates to world coordinates
        // Find which view the mouse is in
        Rectangle top_view = {50, 50, 300, 300};
        if (CheckCollisionPointRec(mouse_pos, top_view)) {
            Vector2 relative_pos = {
                mouse_pos.x - (top_view.x + top_view.width/2),
                mouse_pos.y - (top_view.y + top_view.height/2)
            };
            
            env->target_pos[0] = relative_pos.x / 2.0f; // Adjust for scale
            env->target_pos[1] = relative_pos.y / 2.0f;
            env->target_pos[2] = BASE_DEPTH + 40.0f; // Default height
            
            printf("🖱️ Manual target placed at: (%.1f, %.1f, %.1f)mm\n",
                   env->target_pos[0], env->target_pos[1], env->target_pos[2]);
            
            env->target_state = TARGET_ACTIVE;
            env->stability_timer = 0.0f;
            env->tick = 0;
        }
    }
    
    BeginDrawing();
    ClearBackground(PUFF_BACKGROUND);
    
    // STABLE 2D MULTI-VIEW RENDERING
    draw_2d_views(env);
    
    // Status information
    draw_status_overlay(env);
    
    EndDrawing();
}

// Draw multiple 2D orthographic views (much more stable than 3D)
void draw_2d_views(Tendril* env) {
    float scale = 2.0f;  // Zoom factor for better visibility
    
    // View boundaries
    Rectangle top_view = {VIEW_TOP_X, VIEW_TOP_Y, VIEW_TOP_SIZE, VIEW_TOP_SIZE};
    Rectangle side_view = {VIEW_SIDE_X, VIEW_SIDE_Y, VIEW_SIDE_SIZE, VIEW_SIDE_SIZE};
    Rectangle front_view = {VIEW_FRONT_X, VIEW_FRONT_Y, VIEW_FRONT_SIZE, VIEW_FRONT_SIZE};
    
    // Draw view frames
    DrawRectangleLines(top_view.x-2, top_view.y-2, top_view.width+4, top_view.height+4, PUFF_WHITE);
    DrawRectangleLines(side_view.x-2, side_view.y-2, side_view.width+4, side_view.height+4, PUFF_WHITE);
    DrawRectangleLines(front_view.x-2, front_view.y-2, front_view.width+4, front_view.height+4, PUFF_WHITE);
    
    // View labels - REALISTIC HARDWARE VIEWS
    DrawText("TABLE VIEW (Base Rotation)", top_view.x, top_view.y-20, 14, PUFF_WHITE);
    DrawText("SIDE VIEW (Arm Reach)", side_view.x, side_view.y-20, 14, PUFF_WHITE);
    DrawText("SERVO DETAIL", front_view.x, front_view.y-20, 14, PUFF_WHITE);
    
    // 1. TOP VIEW (X-Y plane) - Base rotation and horizontal positioning
    draw_top_view(env, top_view, scale);
    
    // 2. SIDE VIEW (X-Z plane) - Arm reach and vertical positioning  
    draw_side_view(env, side_view, scale);
    
    // 3. FRONT VIEW (Y-Z plane) - Front perspective
    draw_front_view(env, front_view, scale);
}

void draw_top_view(Tendril* env, Rectangle view, float scale) {
    Vector2 center = {view.x + view.width/2, view.y + view.height/2};
    
    // Draw TABLE SURFACE (rectangular like real table)
    float table_width = 80 * scale;
    float table_height = 60 * scale;
    DrawRectangleLines(center.x - table_width/2, center.y - table_height/2, 
                      table_width, table_height, GRAY);
    DrawText("TABLE", center.x - table_width/2 + 5, center.y - table_height/2 + 5, 10, GRAY);
    
    // Draw REACHABLE WORKSPACE VISUALIZATION (critical for evaluation)
    draw_reachable_workspace_top(env, center, scale);
    
    // Draw PHYSICAL BASE (60x60mm like STL file)
    float base_size = BASE_WIDTH * scale / 3;
    DrawRectangle(center.x - base_size/2, center.y - base_size/2, base_size, base_size, DARKGRAY);
    DrawRectangleLines(center.x - base_size/2, center.y - base_size/2, base_size, base_size, PUFF_WHITE);
    
    // Draw SERVO1 ROTATION (show actual hardware rotation)
    float servo1_angle = env->joint_angles[0];  // 0-180° servo range
    float display_angle = servo1_angle - M_PI/2;  // Center display at 0°
    
    // Draw servo rotation indicator (arc showing 180° range)
    DrawCircleLines(center.x, center.y, base_size/2 + 10, YELLOW);
    
    // Draw current servo position (red line showing rotation)
    Vector2 servo_direction = {
        center.x + (base_size/2 + 15) * cosf(display_angle),
        center.y + (base_size/2 + 15) * sinf(display_angle)
    };
    DrawLineEx(center, servo_direction, 3, PUFF_RED);
    
    // Show ACTUAL 3-JOINT ARM CONSTRUCTION (realistic hardware representation)
    float servo1_yaw = env->joint_angles[0] - M_PI/2;  // Center at 0°
    float servo2_pitch = env->joint_angles[1] - M_PI/2; // Convert to display angle
    float servo3_pitch = env->joint_angles[2] - M_PI/2;
    
    // Calculate ACTUAL joint positions (3 segments like real hardware)
    Vector2 base_joint = center;  // Servo1 position (base)
    
    // Joint2 position (end of first segment)
    Vector2 joint2 = {
        center.x + SEGMENT_LENGTH * cosf(servo1_yaw) * scale,
        center.y + SEGMENT_LENGTH * sinf(servo1_yaw) * scale
    };
    
    // Joint3 position (end of second segment) - affected by both servo1 and servo2
    float total_yaw_2 = servo1_yaw;  // Yaw carries through
    Vector2 joint3 = {
        joint2.x + SEGMENT_LENGTH * cosf(total_yaw_2) * scale,
        joint2.y + SEGMENT_LENGTH * sinf(total_yaw_2) * scale
    };
    
    // End effector (tip) - affected by all 3 servos
    Vector2 end_pos = {
        center.x + env->end_effector_pos[0] * scale,
        center.y + env->end_effector_pos[1] * scale  
    };
    
    // Draw REALISTIC 3-SEGMENT ARM
    DrawLineEx(base_joint, joint2, 6, PUFF_RED);     // Segment 1 (controlled by servo1)
    DrawLineEx(joint2, joint3, 6, ORANGE);           // Segment 2 (controlled by servo1+servo2)  
    DrawLineEx(joint3, end_pos, 6, PUFF_CYAN);       // End cap (controlled by all 3)
    
    // Draw JOINT POSITIONS with different colors for each servo
    DrawCircle(base_joint.x, base_joint.y, 8, YELLOW);      // Servo1 (base yaw)
    DrawCircle(joint2.x, joint2.y, 6, ORANGE);              // Servo2 (shoulder pitch)
    DrawCircle(joint3.x, joint3.y, 6, PUFF_CYAN);          // Servo3 (elbow pitch)  
    DrawCircle(end_pos.x, end_pos.y, 8, PUFF_GREEN);        // End effector (laser tip)
    
    // Show INDIVIDUAL SERVO CONTRIBUTIONS
    char servo1_text[32], servo2_text[32], servo3_text[32];
    sprintf(servo1_text, "S1:%.0f°", env->joint_angles[0] * 180/M_PI);
    sprintf(servo2_text, "S2:%.0f°", env->joint_angles[1] * 180/M_PI);  
    sprintf(servo3_text, "S3:%.0f°", env->joint_angles[2] * 180/M_PI);
    
    DrawText(servo1_text, base_joint.x - 15, base_joint.y - 25, 10, YELLOW);
    DrawText(servo2_text, joint2.x - 15, joint2.y - 25, 10, ORANGE);
    DrawText(servo3_text, joint3.x - 15, joint3.y - 25, 10, PUFF_CYAN);
    
    // Show COMPREHENSIVE SERVO STATUS
    char status_text[64];
    sprintf(status_text, "Base Rotation: %.0f°", env->joint_angles[0] * 180/M_PI);
    DrawText(status_text, view.x + 5, view.y + view.height - 20, 10, YELLOW);
    
    // Draw target with REACHABILITY VALIDATION
    Vector2 target_2d = {
        center.x + env->target_pos[0] * scale,
        center.y + env->target_pos[1] * scale
    };
    
    // Check if target is reachable and style accordingly
    ReachabilityResult target_reach = validate_target_reachability(
        env->target_pos[0], env->target_pos[1], env->target_pos[2]);
    
    Color target_color;
    float target_size;
    char* target_label;
    
    if (env->target_state == TARGET_SUCCESS) {
        target_color = PUFF_GREEN;
        target_size = 15;
        target_label = "HIT!";
    } else if (target_reach.is_reachable) {
        target_color = YELLOW;
        target_size = 10;
        target_label = "REACHABLE";
    } else {
        target_color = PUFF_RED;
        target_size = 12;
        target_label = "UNREACHABLE!";
    }
    
    // Draw target with reachability indication
    DrawCircle(target_2d.x, target_2d.y, target_size, target_color);
    DrawCircleLines(target_2d.x, target_2d.y, target_size + 2, target_color);
    
    // Draw status text
    DrawText(target_label, target_2d.x - 25, target_2d.y - 30, 10, target_color);
    
    if (target_reach.is_reachable) {
        char confidence_text[32];
        sprintf(confidence_text, "%.0f%%", target_reach.confidence * 100);
        DrawText(confidence_text, target_2d.x - 10, target_2d.y - 15, 8, target_color);
    }
    
    // Draw laser pointing direction (top-down)
    Vector2 laser_end = {
        end_pos.x + env->pointing_direction[0] * 50 * scale,
        end_pos.y + env->pointing_direction[1] * 50 * scale
    };
    DrawLineEx(end_pos, laser_end, 2, PUFF_RED);
    
    // DEBUG: Draw line from end effector to target (shows desired direction)
    DrawLineEx(end_pos, target_2d, 1, GRAY);
}

void draw_side_view(Tendril* env, Rectangle view, float scale) {
    Vector2 center = {view.x + view.width/2, view.y + view.height/2};
    
    // Draw PHYSICAL TABLE (thick line showing table surface)
    float table_y = center.y + view.height/3;  // Position table in lower third
    DrawLineEx((Vector2){view.x, table_y}, (Vector2){view.x + view.width, table_y}, 6, BROWN);
    DrawText("TABLE SURFACE", view.x + 5, table_y + 10, 10, BROWN);
    
    // Draw PHYSICAL BASE MOUNTED ON TABLE (realistic proportions)
    float base_width = BASE_WIDTH * scale / 3;
    float base_height = BASE_DEPTH * scale;
    Vector2 base_pos = {center.x, table_y};
    
    DrawRectangle(base_pos.x - base_width/2, base_pos.y - base_height, 
                  base_width, base_height, DARKGRAY);
    DrawRectangleLines(base_pos.x - base_width/2, base_pos.y - base_height, 
                      base_width, base_height, PUFF_WHITE);
    DrawText("BASE", base_pos.x - 15, base_pos.y - base_height/2, 8, PUFF_WHITE);
    
    // Draw REACHABLE WORKSPACE in side view
    draw_reachable_workspace_side(env, base_pos, scale);
    
    // Show REALISTIC 3-JOINT ARM (side view - shows pitch movements)
    float servo1_yaw = env->joint_angles[0] - M_PI/2;    // Base rotation (not visible in side view)
    float servo2_pitch = env->joint_angles[1] - M_PI/2;  // Shoulder pitch (main visible movement)
    float servo3_pitch = env->joint_angles[2] - M_PI/2;  // Elbow pitch (secondary movement)
    
    // SEGMENT 1: Base to Joint2 (controlled by servo2 pitch) - FIXED: Apply base rotation projection
    Vector2 joint2_pos = {
        base_pos.x + SEGMENT_LENGTH * cosf(servo1_yaw) * cosf(servo2_pitch) * scale,
        base_pos.y - base_height - SEGMENT_LENGTH * sinf(servo2_pitch) * scale
    };
    
    // SEGMENT 2: Joint2 to Joint3 (controlled by servo2 + servo3 combined pitch) - FIXED: Apply base rotation projection
    float combined_pitch = servo2_pitch + servo3_pitch;
    Vector2 joint3_pos = {
        joint2_pos.x + SEGMENT_LENGTH * cosf(servo1_yaw) * cosf(combined_pitch) * scale,
        joint2_pos.y - SEGMENT_LENGTH * sinf(combined_pitch) * scale
    };
    
    // END CAP: Joint3 to tip (final pointing direction) - FIXED: Apply base rotation projection
    Vector2 tip_pos = {
        joint3_pos.x + ENDCAP_LENGTH * cosf(servo1_yaw) * cosf(combined_pitch) * scale,
        joint3_pos.y - ENDCAP_LENGTH * sinf(combined_pitch) * scale
    };
    
    // Draw 3-SEGMENT ARM with color coding for each joint
    DrawLineEx((Vector2){base_pos.x, base_pos.y - base_height}, joint2_pos, 8, PUFF_RED);    // Segment 1
    DrawLineEx(joint2_pos, joint3_pos, 6, ORANGE);                                          // Segment 2  
    DrawLineEx(joint3_pos, tip_pos, 4, PUFF_CYAN);                                         // End cap
    
    // Draw JOINT POSITIONS with labels
    DrawCircle(base_pos.x, base_pos.y - base_height, 10, YELLOW);  // Servo1 (base - yaw only)
    DrawCircle(joint2_pos.x, joint2_pos.y, 8, ORANGE);            // Servo2 (shoulder pitch)
    DrawCircle(joint3_pos.x, joint3_pos.y, 6, PUFF_CYAN);         // Servo3 (elbow pitch)
    DrawCircle(tip_pos.x, tip_pos.y, 8, PUFF_GREEN);              // Laser tip
    
    // Label each joint with its function
    DrawText("BASE", base_pos.x - 15, base_pos.y - base_height - 15, 8, YELLOW);
    DrawText("SHOULDER", joint2_pos.x - 20, joint2_pos.y - 15, 8, ORANGE);  
    DrawText("ELBOW", joint3_pos.x - 15, joint3_pos.y - 15, 8, PUFF_CYAN);
    DrawText("TIP", tip_pos.x - 10, tip_pos.y - 15, 8, PUFF_GREEN);
    
    // Show SERVO ANGLES with LIMIT WARNINGS
    char servo_text[128];
    sprintf(servo_text, "Shoulder: %.0f° | Elbow: %.0f°", 
            env->joint_angles[1] * 180/M_PI, env->joint_angles[2] * 180/M_PI);
    DrawText(servo_text, view.x + 5, view.y + 5, 10, YELLOW);
    
    // WARN about servo limits (critical for training evaluation)
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_deg = env->joint_angles[i] * 180/M_PI;
        if (angle_deg < 5.0f || angle_deg > 175.0f) {
            char warning[64];
            sprintf(warning, "⚠️ SERVO%d AT LIMIT: %.0f°", i+1, angle_deg);
            Color warning_color = (angle_deg < 5.0f || angle_deg > 175.0f) ? PUFF_RED : ORANGE;
            DrawText(warning, view.x + 5, view.y + 20 + i*15, 10, warning_color);
        }
    }
    
    // Draw target (side view) with clear success feedback
    Vector2 target_2d = {
        center.x + env->target_pos[0] * scale,
        center.y + view.height/2 - env->target_pos[2] * scale
    };
    
    Color target_color = (env->target_state == TARGET_SUCCESS) ? PUFF_GREEN : YELLOW;
    float target_size = (env->target_state == TARGET_SUCCESS) ? 15 : 10;
    DrawCircle(target_2d.x, target_2d.y, target_size, target_color);
    
    // Show height measurements
    char height_text[32];
    sprintf(height_text, "H: %.1fmm", env->end_effector_pos[2]);
    DrawText(height_text, view.x + 5, view.y + 5, 10, PUFF_WHITE);
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
    DrawLineEx(base_pos, end_2d, 4, PUFF_CYAN);
    DrawCircle(base_pos.x, base_pos.y, 6, PUFF_WHITE);
    DrawCircle(end_2d.x, end_2d.y, 8, PUFF_GREEN);
    
    // Draw target (front view) with clear success feedback
    Vector2 target_2d = {
        center.x + env->target_pos[1] * scale,
        center.y + view.height/2 - env->target_pos[2] * scale
    };
    
    Color target_color = (env->target_state == TARGET_SUCCESS) ? PUFF_GREEN : YELLOW;
    float target_size = (env->target_state == TARGET_SUCCESS) ? 15 : 10;
    DrawCircle(target_2d.x, target_2d.y, target_size, target_color);
}

void draw_status_overlay(Tendril* env) {
    int y_offset = HEIGHT - 120;
    
    // DETAILED JOINT STATUS for training evaluation
    char joint_info[256];
    sprintf(joint_info, "🔄 Base: %.1f° | 🔧 Shoulder: %.1f° | 🦾 Elbow: %.1f°", 
            env->joint_angles[0] * 180/M_PI,
            env->joint_angles[1] * 180/M_PI, 
            env->joint_angles[2] * 180/M_PI);
    DrawText(joint_info, 10, y_offset, 12, PUFF_WHITE);
    
    // SERVO LIMIT ALERTS (critical for identifying training issues)
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_deg = env->joint_angles[i] * 180/M_PI;
        if (angle_deg < 10.0f || angle_deg > 170.0f) {
            char* servo_names[] = {"BASE", "SHOULDER", "ELBOW"};
            char limit_warning[64];
            sprintf(limit_warning, "⚠️ %s NEAR LIMIT: %.0f°", servo_names[i], angle_deg);
            DrawText(limit_warning, 10, y_offset - 15 - i*12, 11, PUFF_RED);
        }
    }
    
    char end_effector_info[256];
    sprintf(end_effector_info, "End Effector: (%.1f, %.1f, %.1f)mm", 
            env->end_effector_pos[0], env->end_effector_pos[1], env->end_effector_pos[2]);
    DrawText(end_effector_info, 10, y_offset + 15, 12, PUFF_WHITE);
    
    char info2[256];
    sprintf(info2, "Target: (%.1f, %.1f, %.1f)mm", 
            env->target_pos[0], env->target_pos[1], env->target_pos[2]);
    DrawText(info2, 10, y_offset + 30, 12, PUFF_WHITE);
    
    // VALIDATE CURRENT TARGET (critical for training evaluation)
    ReachabilityResult current_reach = validate_target_reachability(
        env->target_pos[0], env->target_pos[1], env->target_pos[2]);
    
    char reachability_info[128];
    if (current_reach.is_reachable) {
        sprintf(reachability_info, "✅ Target Reachable (Confidence: %.1f%%)", 
                current_reach.confidence * 100);
        DrawText(reachability_info, 10, y_offset + 45, 11, PUFF_GREEN);
    } else {
        sprintf(reachability_info, "❌ TARGET UNREACHABLE - Training Issue!");
        DrawText(reachability_info, 10, y_offset + 45, 11, PUFF_RED);
    }
    
    // ANGULAR ERROR with color-coded feedback (ENHANCED DEBUG)
    float error_degrees = env->angular_error * 180.0f / M_PI;
    char angular_debug[128];
    sprintf(angular_debug, "🎯 Angular Error: %.1f° (Target: <5°) | Stability: %.1fs", 
            error_degrees, env->stability_timer);
    
    Color error_color;
    if (error_degrees < 5.0f) error_color = PUFF_GREEN;      // Success threshold
    else if (error_degrees < 15.0f) error_color = YELLOW;     // Getting close
    else error_color = PUFF_RED;                              // Need improvement
    
    DrawText(angular_debug, 10, y_offset + 60, 12, error_color);
    
    // REAL-TIME PHYSICS DEBUG (Enhanced for troubleshooting)
    char debug_physics[256];
    sprintf(debug_physics, "🔍 Pointing Dir: (%.2f,%.2f,%.2f) | Target Dir: (%.2f,%.2f,%.2f)", 
            env->pointing_direction[0], env->pointing_direction[1], env->pointing_direction[2],
            env->target_direction[0], env->target_direction[1], env->target_direction[2]);
    DrawText(debug_physics, 10, HEIGHT - 60, 10, PUFF_CYAN);
    
    // JOINT VALIDATION (Critical for servo debugging)
    bool joints_valid = true;
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_deg = env->joint_angles[i] * 180.0f / M_PI;
        if (angle_deg < -5.0f || angle_deg > 185.0f) {
            joints_valid = false;
            char warning[64];
            sprintf(warning, "⚠️ SERVO%d OUT OF RANGE: %.1f°", i+1, angle_deg);
            DrawText(warning, 400, 50 + i*15, 10, PUFF_RED);
        }
    }
    
    if (joints_valid) {
        DrawText("✅ All servos within valid range (0-180°)", 400, 50, 10, PUFF_GREEN);
    }
    
    // Enhanced Controls
    DrawText("ESC: Exit | TAB: Fullscreen | Right Click: New Reachable Target | Left Click: Place Target", 10, HEIGHT - 35, 11, GRAY);
    DrawText("🟢 Green Line: Reachable Workspace | 🟡 Yellow: Reachable Target | 🔴 Red: Unreachable", 10, HEIGHT - 20, 11, GRAY);
    
    // Target state indicator - BIG AND CLEAR like Pong score
    char* state_text = (env->target_state == TARGET_SUCCESS) ? "🎯 TARGET HIT!" : "🎯 AIMING...";
    Color state_color = (env->target_state == TARGET_SUCCESS) ? PUFF_GREEN : YELLOW;
    int font_size = (env->target_state == TARGET_SUCCESS) ? 24 : 16; // Bigger on success!
    DrawText(state_text, WIDTH - 200, 20, font_size, state_color);
    
    // Show reward like Pong score
    char reward_text[32];
    sprintf(reward_text, "Reward: +%.1f", env->rewards[0]);
    Color reward_color = (env->rewards[0] > 5.0f) ? PUFF_GREEN : 
                        (env->rewards[0] > 0.0f) ? YELLOW : GRAY;
    DrawText(reward_text, WIDTH - 200, 50, 14, reward_color);
}

// Draw reachable workspace based on actual servo constraints
void draw_reachable_workspace_top(Tendril* env, Vector2 center, float scale) {
    // Sample workspace at different angles to show reachable area
    int num_samples = 36; // Every 10 degrees
    Vector2 reachable_points[36];
    int valid_points = 0;
    
    for (int i = 0; i < num_samples; i++) {
        float angle = (float)i * 2.0f * M_PI / (float)num_samples;
        
        // Test multiple distances at this angle
        for (float distance = 20.0f; distance <= 100.0f; distance += 10.0f) {
            float test_x = distance * cosf(angle);
            float test_y = distance * sinf(angle);
            float test_z = BASE_DEPTH + 30.0f; // Mid-height
            
            ReachabilityResult result = validate_target_reachability(test_x, test_y, test_z);
            
            if (result.is_reachable && result.confidence > 0.3f) {
                // Found maximum reach in this direction
                reachable_points[valid_points].x = center.x + test_x * scale;
                reachable_points[valid_points].y = center.y + test_y * scale;
                valid_points++;
                break; // Move to next angle
            }
        }
    }
    
    // Draw reachable workspace boundary
    if (valid_points > 2) {
        for (int i = 0; i < valid_points - 1; i++) {
            DrawLineEx(reachable_points[i], reachable_points[i + 1], 2, GREEN);
        }
        // Close the polygon
        if (valid_points > 0) {
            DrawLineEx(reachable_points[valid_points - 1], reachable_points[0], 2, GREEN);
        }
    }
    
    // Draw workspace zones with different colors
    float max_reach = 2.0f * SEGMENT_LENGTH;
    float min_reach = 20.0f;
    
    // Inner zone (high confidence)
    DrawCircleLines(center.x, center.y, min_reach * scale * 2, DARKGREEN);
    DrawText("SAFE ZONE", center.x + min_reach * scale * 1.5f, center.y, 8, DARKGREEN);
    
    // Outer boundary (theoretical max)
    DrawCircleLines(center.x, center.y, max_reach * scale, LIGHTGRAY);
    DrawText("MAX REACH", center.x + max_reach * scale - 30, center.y, 8, LIGHTGRAY);
}

void draw_reachable_workspace_side(Tendril* env, Vector2 base_pos, float scale) {
    // Show reachable height range at different horizontal distances
    for (float x_dist = 0; x_dist <= 80.0f; x_dist += 20.0f) {
        float min_z = BASE_DEPTH + 10.0f;
        float max_z = BASE_DEPTH + 80.0f;
        
        // Find reachable height range at this distance
        for (float test_z = min_z; test_z <= max_z; test_z += 5.0f) {
            ReachabilityResult result = validate_target_reachability(x_dist, 0.0f, test_z);
            
            if (result.is_reachable && result.confidence > 0.3f) {
                Vector2 reachable_point = {
                    base_pos.x + x_dist * scale,
                    base_pos.y - (test_z - BASE_DEPTH) * scale
                };
                DrawCircle(reachable_point.x, reachable_point.y, 2, GREEN);
            }
        }
    }
}

// ============================================================================
// AUTOMATIC EVALUATION SYSTEM (Drone-inspired)
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

// Main demo function (for standalone testing)
#ifdef TENDRIL_STANDALONE
int main() {
    srand(time(NULL));
    
    Tendril env = {0};
    allocate(&env);
    c_reset(&env);
    
    printf("PufferLib Tendril Demo\\n");
    printf("Hardware specs: %d joints, %.0fmm segments\\n", NUM_JOINTS, SEGMENT_LENGTH);
    printf("Observation space: 12D, Action space: 3D\\n\\n");
    printf("Creating 3D visualization window...\\n");
    
    // Initialize the window first
    c_render(&env); // This creates the window
    
    printf("3D Window created! Controls:\\n");
    printf("- Mouse drag: Rotate camera\\n");
    printf("- Mouse wheel: Zoom\\n");
    printf("- ESC: Exit\\n\\n");
    
    int frameCount = 0;
    while (!WindowShouldClose() && frameCount < 1800) { // Auto-exit after 30 seconds
        // Random actions for demo (gentle movements)
        for (int i = 0; i < NUM_JOINTS; i++) {
            env.actions[i] = randf(-0.3f, 0.3f); // Gentle random motions
        }
        
        c_step(&env);
        c_render(&env);
        
        frameCount++;
        
        // Print progress every 60 frames (1 second)
        if (frameCount % 60 == 0) {
            printf("Demo running... Frame %d/1800 (ESC to exit early)\\n", frameCount);
        }
    }
    
    printf("Demo completed!\\n");
    c_close(&env);
    free_allocated(&env);
    return 0;
}
#endif // TENDRIL_STANDALONE