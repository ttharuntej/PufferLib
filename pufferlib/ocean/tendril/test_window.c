#include "raylib.h"
#include <stdio.h>
#include <math.h>

int main() {
    printf("Testing raylib window creation...\n");
    
    // Try to create a simple window
    InitWindow(400, 300, "Raylib Test Window");
    
    if (!IsWindowReady()) {
        printf("ERROR: Failed to create window!\n");
        return -1;
    }
    
    printf("Window created successfully!\n");
    printf("Close window or press ESC to exit\n");
    
    SetTargetFPS(60);
    
    int frameCount = 0;
    while (!WindowShouldClose() && frameCount < 600) { // Auto-exit after 10 seconds
        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        DrawText("Raylib Test Window", 50, 100, 20, DARKGRAY);
        DrawText("If you can see this, graphics work!", 50, 130, 16, DARKGRAY);
        DrawText(TextFormat("Frame: %d", frameCount), 50, 160, 16, DARKGRAY);
        
        // Draw a simple moving circle
        DrawCircle(200 + 50 * sinf(frameCount * 0.1f), 200, 20, RED);
        
        EndDrawing();
        frameCount++;
    }
    
    printf("Test completed. Closing window.\n");
    CloseWindow();
    return 0;
}