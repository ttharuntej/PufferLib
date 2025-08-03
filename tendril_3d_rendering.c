
// 🎨 3D TENDRIL VISUALIZATION (Add to binding.c)

Client* make_client(Tendril* env) {
    Client* client = (Client*)calloc(1, sizeof(Client));
    
    // Initialize raylib 3D window
    InitWindow(WIDTH, HEIGHT, "PufferLib Tendril 3D - Expert Trained Model");
    SetTargetFPS(60);
    
    // Setup 3D camera
    client->camera.position = (Vector3){ 200.0f, 100.0f, 200.0f };
    client->camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    client->camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    client->camera.fovy = 45.0f;
    client->camera.projection = CAMERA_PERSPECTIVE;
    
    client->camera_distance = 200.0f;
    client->camera_azimuth = 45.0f;
    client->camera_elevation = 30.0f;
    client->is_dragging = false;
    
    return client;
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
    
    // Mouse camera controls
    Vector2 mouse_pos = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (client->is_dragging) {
            Vector2 delta = Vector2Subtract(mouse_pos, client->last_mouse_pos);
            client->camera_azimuth += delta.x * 0.5f;
            client->camera_elevation += delta.y * 0.5f;
            client->camera_elevation = Clamp(client->camera_elevation, -80.0f, 80.0f);
        }
        client->is_dragging = true;
    } else {
        client->is_dragging = false;
    }
    client->last_mouse_pos = mouse_pos;
    
    // Mouse wheel zoom
    float wheel = GetMouseWheelMove();
    client->camera_distance += wheel * 10.0f;
    client->camera_distance = Clamp(client->camera_distance, 50.0f, 500.0f);
    
    // Update camera position
    float rad_azimuth = client->camera_azimuth * DEG2RAD;
    float rad_elevation = client->camera_elevation * DEG2RAD;
    
    client->camera.position.x = client->camera_distance * cosf(rad_elevation) * cosf(rad_azimuth);
    client->camera.position.y = client->camera_distance * sinf(rad_elevation);
    client->camera.position.z = client->camera_distance * cosf(rad_elevation) * sinf(rad_azimuth);
    
    // Render 3D scene
    BeginDrawing();
    ClearBackground(PUFF_BACKGROUND);
    
    BeginMode3D(client->camera);
    
    // Draw coordinate system
    DrawLine3D((Vector3){-100, 0, 0}, (Vector3){100, 0, 0}, RED);    // X-axis
    DrawLine3D((Vector3){0, -100, 0}, (Vector3){0, 100, 0}, GREEN);  // Y-axis  
    DrawLine3D((Vector3){0, 0, -100}, (Vector3){0, 0, 100}, BLUE);   // Z-axis
    
    // Draw workspace boundary
    DrawCubeWires((Vector3){0, 0, BASE_DEPTH}, WORKSPACE_SIZE*2, WORKSPACE_SIZE*2, 10, GRAY);
    
    // Draw tendril base
    DrawCube((Vector3){0, 0, 0}, BASE_WIDTH, BASE_HEIGHT, BASE_DEPTH, PUFF_CYAN);
    
    // Draw tendril segments (forward kinematics)
    Vector3 pos = {0, 0, BASE_DEPTH};
    float cumulative_angle = 0;
    
    for (int i = 0; i < NUM_JOINTS; i++) {
        cumulative_angle += env->joint_angles[i];
        
        Vector3 next_pos = {
            pos.x + SEGMENT_LENGTH * cosf(cumulative_angle),
            pos.y + SEGMENT_LENGTH * sinf(cumulative_angle), 
            pos.z
        };
        
        // Draw segment
        DrawCylinder(pos, next_pos, 3.0f, 3.0f, 8, PUFF_WHITE);
        
        // Draw joint
        DrawSphere(pos, 5.0f, PUFF_RED);
        
        pos = next_pos;
    }
    
    // Draw end effector
    DrawSphere(pos, 8.0f, PUFF_GREEN);
    
    // Draw target
    Vector3 target = {env->target_pos[0], env->target_pos[1], env->target_pos[2]};
    DrawSphere(target, 6.0f, YELLOW);
    DrawSphereWires(target, 12.0f, 8, 8, YELLOW);
    
    // Draw connection line (distance visualization)
    DrawLine3D(pos, target, WHITE);
    
    EndMode3D();
    
    // 2D UI overlay
    char info[256];
    sprintf(info, "Tendril Expert Model - Distance: %.1fmm", 
            Vector3Distance(pos, target));
    DrawText(info, 10, 10, 20, WHITE);
    
    sprintf(info, "Joint Angles: [%.1f°, %.1f°, %.1f°]", 
            env->joint_angles[0] * RAD2DEG,
            env->joint_angles[1] * RAD2DEG, 
            env->joint_angles[2] * RAD2DEG);
    DrawText(info, 10, 35, 16, LIGHTGRAY);
    
    DrawText("Mouse: Drag to rotate, Wheel: Zoom, ESC: Exit", 10, HEIGHT-25, 16, DARKGRAY);
    
    EndDrawing();
}

void close_client(Client* client) {
    if (client) {
        free(client);
    }
}
