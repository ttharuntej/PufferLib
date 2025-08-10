#include "raylib.h"
#include "tendril_clean.h"

// PufferLib color scheme definitions
const TendrilColor PUFF_RED = {187, 0, 0, 255};
const TendrilColor PUFF_CYAN = {0, 187, 187, 255};
const TendrilColor PUFF_WHITE = {241, 241, 241, 241};
const TendrilColor PUFF_BACKGROUND = {6, 24, 24, 255};
const TendrilColor PUFF_GREEN = {0, 187, 0, 255};

// Helper functions to convert between custom and raylib types
static inline Color to_raylib_color(TendrilColor c) {
    return (Color){c.r, c.g, c.b, c.a};
}

static inline Vector2 to_raylib_vector2(TendrilVector2 v) {
    return (Vector2){v.x, v.y};
}

static inline Rectangle to_raylib_rectangle(TendrilRectangle r) {
    return (Rectangle){r.x, r.y, r.width, r.height};
}

// Create rendering client with 2D interface (STABLE VERSION)
Client* make_client(Tendril* env) {
    (void)env; // Suppress unused parameter warning
    
    Client* client = (Client*)calloc(1, sizeof(Client));
    if (!client) return NULL;
    
    // Simple 2D window initialization - much more stable than 3D
    InitWindow(WIDTH, HEIGHT, "🎯 Tendril Laser Pointer - Clean Architecture");
    SetTargetFPS(60);
    
    // Basic client state
    client->is_dragging = false;
    client->last_mouse_pos = (TendrilVector2){0.0f, 0.0f};
    
    return client;
}

// UNIFIED: Close rendering client (expert feedback - make idempotent)
void close_client(Client* client) {
    if (!client) return;
    
    // Only close window if it's actually open
    if (IsWindowReady()) {
        CloseWindow();
    }
    
    free(client);
}

// UNIFIED: Close environment (expert feedback - consistent cleanup)
void c_close(Tendril* env) {
    if (env && env->client) {
        close_client(env->client);     // calls CloseWindow() and free(client)
        env->client = NULL;
    } else {
        // If someone called CloseWindow() elsewhere, at least be idempotent
        if (IsWindowReady()) {
            CloseWindow();
        }
    }
}

// Draw multiple 2D orthographic views (much more stable than 3D)
void draw_2d_views(Tendril* env) {
    float scale = 2.0f;  // Zoom factor for better visibility
    
    // View boundaries
    Rectangle top_view = {VIEW_TOP_X, VIEW_TOP_Y, VIEW_TOP_SIZE, VIEW_TOP_SIZE};
    Rectangle side_view = {VIEW_SIDE_X, VIEW_SIDE_Y, VIEW_SIDE_SIZE, VIEW_SIDE_SIZE};
    Rectangle front_view = {VIEW_FRONT_X, VIEW_FRONT_Y, VIEW_FRONT_SIZE, VIEW_FRONT_SIZE};
    
    // Draw view frames
    DrawRectangleLines(top_view.x-2, top_view.y-2, top_view.width+4, top_view.height+4, to_raylib_color(PUFF_WHITE));
    DrawRectangleLines(side_view.x-2, side_view.y-2, side_view.width+4, side_view.height+4, to_raylib_color(PUFF_WHITE));
    DrawRectangleLines(front_view.x-2, front_view.y-2, front_view.width+4, front_view.height+4, to_raylib_color(PUFF_WHITE));
    
    // View labels
    DrawText("TABLE VIEW (Base Rotation)", top_view.x, top_view.y-20, 14, to_raylib_color(PUFF_WHITE));
    DrawText("SIDE VIEW (Arm Reach)", side_view.x, side_view.y-20, 14, to_raylib_color(PUFF_WHITE));
    DrawText("SERVO DETAIL", front_view.x, front_view.y-20, 14, to_raylib_color(PUFF_WHITE));
    
    // Draw individual views
    TendrilRectangle top_rect = {top_view.x, top_view.y, top_view.width, top_view.height};
    draw_top_view(env, top_rect, scale);
    TendrilRectangle side_rect = {side_view.x, side_view.y, side_view.width, side_view.height};
    draw_side_view(env, side_rect, scale);
    TendrilRectangle front_rect = {front_view.x, front_view.y, front_view.width, front_view.height};
    draw_front_view(env, front_rect, scale);
}

void draw_top_view(Tendril* env, TendrilRectangle view, float scale) {
    TendrilVector2 center = {view.x + view.width/2, view.y + view.height/2};
    
    // Draw TABLE SURFACE
    float table_width = 80 * scale;
    float table_height = 60 * scale;
    DrawRectangleLines(center.x - table_width/2, center.y - table_height/2, 
                      table_width, table_height, GRAY);
    DrawText("TABLE", center.x - table_width/2 + 5, center.y - table_height/2 + 5, 10, GRAY);
    
    // Draw REACHABLE WORKSPACE VISUALIZATION
    draw_reachable_workspace_top(env, center, scale);
    
    // Draw PHYSICAL BASE
    float base_size = BASE_WIDTH * scale / 3;
    DrawRectangle(center.x - base_size/2, center.y - base_size/2, base_size, base_size, DARKGRAY);
    DrawRectangleLines(center.x - base_size/2, center.y - base_size/2, base_size, base_size, to_raylib_color(PUFF_WHITE));
    
    // Draw SERVO1 ROTATION indicator
    float servo1_angle = env->joint_angles[0];  // 0-180° servo range
    float display_angle = servo1_angle - M_PI/2;  // Center display at 0°
    
    DrawCircleLines(center.x, center.y, base_size/2 + 10, YELLOW);
    
    Vector2 servo_direction = {
        center.x + (base_size/2 + 15) * cosf(display_angle),
        center.y + (base_size/2 + 15) * sinf(display_angle)
    };
    DrawLineEx(to_raylib_vector2(center), servo_direction, 3, to_raylib_color(PUFF_RED));
    
    // Show ACTUAL 3-JOINT ARM CONSTRUCTION
    float servo1_yaw = env->joint_angles[0] - M_PI/2;  // Center at 0°
    
    // Calculate joint positions
    TendrilVector2 base_joint = center;
    
    TendrilVector2 joint2 = {
        center.x + SEGMENT_LENGTH * cosf(servo1_yaw) * scale,
        center.y + SEGMENT_LENGTH * sinf(servo1_yaw) * scale
    };
    
    TendrilVector2 joint3 = {
        joint2.x + SEGMENT_LENGTH * cosf(servo1_yaw) * scale,
        joint2.y + SEGMENT_LENGTH * sinf(servo1_yaw) * scale
    };
    
    TendrilVector2 end_pos = {
        center.x + env->end_effector_pos[0] * scale,
        center.y + env->end_effector_pos[1] * scale  
    };
    
    // Draw 3-SEGMENT ARM
    DrawLineEx(to_raylib_vector2(base_joint), to_raylib_vector2(joint2), 6, to_raylib_color(PUFF_RED));     // Segment 1
    DrawLineEx(to_raylib_vector2(joint2), to_raylib_vector2(joint3), 6, ORANGE);           // Segment 2  
    DrawLineEx(to_raylib_vector2(joint3), to_raylib_vector2(end_pos), 6, to_raylib_color(PUFF_CYAN));       // End cap
    
    // Draw JOINT POSITIONS
    DrawCircle(base_joint.x, base_joint.y, 8, YELLOW);      // Servo1
    DrawCircle(joint2.x, joint2.y, 6, ORANGE);              // Servo2
    DrawCircle(joint3.x, joint3.y, 6, to_raylib_color(PUFF_CYAN));          // Servo3  
    DrawCircle(end_pos.x, end_pos.y, 8, to_raylib_color(PUFF_GREEN));        // End effector
    
    // Show SERVO ANGLES
    char servo1_text[32], servo2_text[32], servo3_text[32];
    sprintf(servo1_text, "S1:%.0f°", env->joint_angles[0] * 180/M_PI);
    sprintf(servo2_text, "S2:%.0f°", env->joint_angles[1] * 180/M_PI);  
    sprintf(servo3_text, "S3:%.0f°", env->joint_angles[2] * 180/M_PI);
    
    DrawText(servo1_text, base_joint.x - 15, base_joint.y - 25, 10, YELLOW);
    DrawText(servo2_text, joint2.x - 15, joint2.y - 25, 10, ORANGE);
    DrawText(servo3_text, joint3.x - 15, joint3.y - 25, 10, to_raylib_color(PUFF_CYAN));
    
    // Draw target with reachability validation
    Vector2 target_2d = {
        center.x + env->target_pos[0] * scale,
        center.y + env->target_pos[1] * scale
    };
    
    // Check if target is reachable
    ReachabilityResult target_reach = validate_target_reachability(
        env->target_pos[0], env->target_pos[1], env->target_pos[2]);
    
    Color target_color;
    float target_size;
    char* target_label;
    
    if (env->target_state == TARGET_SUCCESS) {
        target_color = to_raylib_color(PUFF_GREEN);
        target_size = 15;
        target_label = "HIT!";
    } else if (target_reach.is_reachable) {
        target_color = YELLOW;
        target_size = 10;
        target_label = "REACHABLE";
    } else {
        target_color = to_raylib_color(PUFF_RED);
        target_size = 12;
        target_label = "UNREACHABLE!";
    }
    
    // Draw target with status
    DrawCircle(target_2d.x, target_2d.y, target_size, target_color);
    DrawCircleLines(target_2d.x, target_2d.y, target_size + 2, target_color);
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
    
    // Draw PHYSICAL TABLE
    float table_y = center.y + view.height/3;
    DrawLineEx((Vector2){view.x, table_y}, (Vector2){view.x + view.width, table_y}, 6, BROWN);
    DrawText("TABLE SURFACE", view.x + 5, table_y + 10, 10, BROWN);
    
    // Draw PHYSICAL BASE
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
    
    // Show 3-JOINT ARM (side view) - FIXED: Apply base rotation projection
    float servo1_yaw = env->joint_angles[0] - M_PI/2;    // Base rotation
    float servo2_pitch = env->joint_angles[1] - M_PI/2;  // Shoulder pitch
    float servo3_pitch = env->joint_angles[2] - M_PI/2;  // Elbow pitch
    
    // FIXED: All segments now account for base rotation projection
    Vector2 joint2_pos = {
        base_pos.x + SEGMENT_LENGTH * cosf(servo1_yaw) * cosf(servo2_pitch) * scale,
        base_pos.y - base_height - SEGMENT_LENGTH * sinf(servo2_pitch) * scale
    };
    
    float combined_pitch = servo2_pitch + servo3_pitch;
    Vector2 joint3_pos = {
        joint2_pos.x + SEGMENT_LENGTH * cosf(servo1_yaw) * cosf(combined_pitch) * scale,
        joint2_pos.y - SEGMENT_LENGTH * sinf(combined_pitch) * scale
    };
    
    Vector2 tip_pos = {
        joint3_pos.x + ENDCAP_LENGTH * cosf(servo1_yaw) * cosf(combined_pitch) * scale,
        joint3_pos.y - ENDCAP_LENGTH * sinf(combined_pitch) * scale
    };
    
    // Draw 3-SEGMENT ARM
    DrawLineEx((Vector2){base_pos.x, base_pos.y - base_height}, joint2_pos, 8, PUFF_RED);
    DrawLineEx(joint2_pos, joint3_pos, 6, ORANGE);
    DrawLineEx(joint3_pos, tip_pos, 4, PUFF_CYAN);
    
    // Draw JOINT POSITIONS
    DrawCircle(base_pos.x, base_pos.y - base_height, 10, YELLOW);
    DrawCircle(joint2_pos.x, joint2_pos.y, 8, ORANGE);
    DrawCircle(joint3_pos.x, joint3_pos.y, 6, PUFF_CYAN);
    DrawCircle(tip_pos.x, tip_pos.y, 8, PUFF_GREEN);
    
    // Label joints
    DrawText("BASE", base_pos.x - 15, base_pos.y - base_height - 15, 8, YELLOW);
    DrawText("SHOULDER", joint2_pos.x - 20, joint2_pos.y - 15, 8, ORANGE);  
    DrawText("ELBOW", joint3_pos.x - 15, joint3_pos.y - 15, 8, PUFF_CYAN);
    DrawText("TIP", tip_pos.x - 10, tip_pos.y - 15, 8, PUFF_GREEN);
    
    // Show SERVO ANGLES with LIMIT WARNINGS
    char servo_text[128];
    sprintf(servo_text, "Shoulder: %.0f° | Elbow: %.0f°", 
            env->joint_angles[1] * 180/M_PI, env->joint_angles[2] * 180/M_PI);
    DrawText(servo_text, view.x + 5, view.y + 5, 10, YELLOW);
    
    // WARN about servo limits
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_deg = env->joint_angles[i] * 180/M_PI;
        if (angle_deg < 10.0f || angle_deg > 170.0f) {
            char warning[64];
            sprintf(warning, "⚠️ SERVO%d NEAR LIMIT: %.0f°", i+1, angle_deg);
            Color warning_color = (angle_deg < 5.0f || angle_deg > 175.0f) ? PUFF_RED : ORANGE;
            DrawText(warning, view.x + 5, view.y + 20 + i*15, 10, warning_color);
        }
    }
    
    // Draw target (side view)
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
    DrawText(height_text, view.x + 5, view.y + view.height - 20, 10, PUFF_WHITE);
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
    
    // Draw target (front view)
    Vector2 target_2d = {
        center.x + env->target_pos[1] * scale,
        center.y + view.height/2 - env->target_pos[2] * scale
    };
    
    Color target_color = (env->target_state == TARGET_SUCCESS) ? PUFF_GREEN : YELLOW;
    float target_size = (env->target_state == TARGET_SUCCESS) ? 15 : 10;
    DrawCircle(target_2d.x, target_2d.y, target_size, target_color);
}

void draw_status_overlay(Tendril* env) {
    int y_offset = HEIGHT - 140;
    
    // DETAILED JOINT STATUS
    char joint_info[256];
    sprintf(joint_info, "🔄 Base: %.1f° | 🔧 Shoulder: %.1f° | 🦾 Elbow: %.1f°", 
            env->joint_angles[0] * 180/M_PI,
            env->joint_angles[1] * 180/M_PI, 
            env->joint_angles[2] * 180/M_PI);
    DrawText(joint_info, 10, y_offset, 12, PUFF_WHITE);
    
    char end_effector_info[256];
    sprintf(end_effector_info, "End Effector: (%.1f, %.1f, %.1f)mm", 
            env->end_effector_pos[0], env->end_effector_pos[1], env->end_effector_pos[2]);
    DrawText(end_effector_info, 10, y_offset + 15, 12, PUFF_WHITE);
    
    char target_info[256];
    sprintf(target_info, "Target: (%.1f, %.1f, %.1f)mm", 
            env->target_pos[0], env->target_pos[1], env->target_pos[2]);
    DrawText(target_info, 10, y_offset + 30, 12, PUFF_WHITE);
    
    // VALIDATE CURRENT TARGET
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
    
    // REAL-TIME PHYSICS DEBUG
    char debug_physics[256];
    sprintf(debug_physics, "🔍 Pointing Dir: (%.2f,%.2f,%.2f) | Target Dir: (%.2f,%.2f,%.2f)", 
            env->pointing_direction[0], env->pointing_direction[1], env->pointing_direction[2],
            env->target_direction[0], env->target_direction[1], env->target_direction[2]);
    DrawText(debug_physics, 10, HEIGHT - 80, 10, PUFF_CYAN);
    
    // JOINT VALIDATION
    bool joints_valid = true;
    for (int i = 0; i < NUM_JOINTS; i++) {
        float angle_deg = env->joint_angles[i] * 180.0f / M_PI;
        if (angle_deg < 5.0f || angle_deg > 175.0f) {
            joints_valid = false;
            char warning[64];
            sprintf(warning, "⚠️ SERVO%d OUT OF RANGE: %.1f°", i+1, angle_deg);
            DrawText(warning, 400, 50 + i*15, 10, PUFF_RED);
        }
    }
    
    if (joints_valid) {
        DrawText("✅ All servos within safe range (5-175°)", 400, 50, 10, PUFF_GREEN);
    }
    
    // Controls
    DrawText("ESC: Exit | TAB: Fullscreen | Right Click: New Target | Left Click: Place Target", 10, HEIGHT - 60, 11, GRAY);
    DrawText("🟢 Green Line: Reachable Workspace | 🟡 Yellow: Reachable Target | 🔴 Red: Unreachable", 10, HEIGHT - 45, 11, GRAY);
    
    // Target state indicator
    char* state_text = (env->target_state == TARGET_SUCCESS) ? "🎯 TARGET HIT!" : "🎯 AIMING...";
    Color state_color = (env->target_state == TARGET_SUCCESS) ? PUFF_GREEN : YELLOW;
    DrawText(state_text, WIDTH - 200, 50, 16, state_color);
}

// Placeholder workspace visualization functions
void draw_reachable_workspace_top(Tendril* env, Vector2 center, float scale) {
    (void)env; (void)center; (void)scale; // Suppress unused warnings
    // Placeholder - implement if needed for workspace visualization
}

void draw_reachable_workspace_side(Tendril* env, Vector2 base_pos, float scale) {
    (void)env; (void)base_pos; (void)scale; // Suppress unused warnings
    // Placeholder - implement if needed for workspace visualization
}

// Main rendering function
void c_render(Tendril* env) {
    // Handle window controls
    if (IsKeyPressed(KEY_ESCAPE)) {
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
    
    // Right-click to generate new VALIDATED reachable target
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
        env->stability_timer = 0.0f;
        env->tick = 0;
    }
    
    // Left-click to force target to cursor position (for testing)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mouse_pos = GetMousePosition();
        
        // Convert screen coordinates to world coordinates (top view)
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