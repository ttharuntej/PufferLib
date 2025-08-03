#include "raylib.h"
#include <stdio.h>
#include <math.h>

int main() {
    printf("Creating LARGE, VISIBLE window...\n");
    
    // Create a large, centered window
    SetConfigFlags(FLAG_WINDOW_ALWAYS_RUN | FLAG_WINDOW_TOPMOST);
    InitWindow(800, 600, "*** BIG VISIBLE TEST WINDOW ***");
    
    if (!IsWindowReady()) {
        printf("ERROR: Failed to create window!\n");
        return -1;
    }
    
    printf("*** LARGE WINDOW SHOULD BE VISIBLE NOW ***\n");
    printf("*** LOOK FOR: '*** BIG VISIBLE TEST WINDOW ***' ***\n");
    printf("*** Window size: 800x600 ***\n");
    printf("Press ESC or close to exit\n");
    
    SetTargetFPS(30);
    
    int frameCount = 0;
    while (!WindowShouldClose() && frameCount < 300) { // 10 seconds at 30fps
        BeginDrawing();
        
        // Flashing bright colors to be extra visible
        Color bg = (frameCount % 30 < 15) ? RED : BLUE;
        ClearBackground(bg);
        
        DrawText("*** BIG VISIBLE TEST ***", 200, 200, 40, WHITE);
        DrawText("If you see this, graphics work!", 150, 280, 30, YELLOW);
        DrawText(TextFormat("Frame: %d/300", frameCount), 300, 350, 20, WHITE);
        
        // Big moving rectangle
        int rectX = 100 + (int)(200 * sinf(frameCount * 0.1f));
        DrawRectangle(rectX, 400, 100, 100, GREEN);
        
        EndDrawing();
        frameCount++;
        
        if (frameCount % 30 == 0) {
            printf("Frame %d/300 - Window should be flashing red/blue!\n", frameCount);
        }
    }
    
    printf("Test completed - did you see the flashing window?\n");
    CloseWindow();
    return 0;
}