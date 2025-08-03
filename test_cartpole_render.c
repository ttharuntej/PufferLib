#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "raylib.h"

#define GRAVITY 9.8f
#define MASSCART 1.0f
#define MASSPOLE 0.1f
#define TOTAL_MASS (MASSPOLE + MASSCART)
#define LENGTH 0.5f // half pole length
#define POLEMASS_LENGTH (MASSPOLE * LENGTH)
#define FORCE_MAG 10.0f
#define TAU 0.02f // timestep duration

#define X_THRESHOLD 2.4f
#define THETA_THRESHOLD_RADIANS (12 * 2 * M_PI / 360)
#define MAX_STEPS 200
#define WIDTH 600
#define HEIGHT 200
#define SCALE 100

const Color PUFF_RED = (Color){187, 0, 0, 255};
const Color PUFF_CYAN = (Color){0, 187, 187, 255};
const Color PUFF_WHITE = (Color){241, 241, 241, 241};
const Color PUFF_BACKGROUND = (Color){6, 24, 24, 255};

typedef struct {
    float x;
    float x_dot;
    float theta;
    float theta_dot;
    int tick;
} Cartpole;

void reset_cartpole(Cartpole* env) {
    env->x = ((float)rand() / (float)RAND_MAX) * 0.08f - 0.04f;
    env->x_dot = ((float)rand() / (float)RAND_MAX) * 0.08f - 0.04f;
    env->theta = ((float)rand() / (float)RAND_MAX) * 0.08f - 0.04f;
    env->theta_dot = ((float)rand() / (float)RAND_MAX) * 0.08f - 0.04f;
    env->tick = 0;
}

void step_cartpole(Cartpole* env, float force) {
    float costheta = cosf(env->theta);
    float sintheta = sinf(env->theta);

    float temp = (force + POLEMASS_LENGTH * env->theta_dot * env->theta_dot * sintheta) / TOTAL_MASS;
    float thetaacc = (GRAVITY * sintheta - costheta * temp) / 
                     (LENGTH * (4.0f / 3.0f - MASSPOLE * costheta * costheta / TOTAL_MASS));
    float xacc = temp - POLEMASS_LENGTH * thetaacc * costheta / TOTAL_MASS;

    env->x += TAU * env->x_dot;
    env->x_dot += TAU * xacc;
    env->theta += TAU * env->theta_dot;
    env->theta_dot += TAU * thetaacc;

    env->tick += 1;
}

void render_cartpole(Cartpole* env) {
    BeginDrawing();
    ClearBackground(PUFF_BACKGROUND);
    DrawLine(0, HEIGHT / 1.5, WIDTH, HEIGHT / 1.5, PUFF_CYAN);
    
    float cart_x = WIDTH / 2 + env->x * SCALE;
    float cart_y = HEIGHT / 1.6;
    DrawRectangle((int)(cart_x - 20), (int)(cart_y - 10), 40, 20, PUFF_CYAN);
    
    float pole_length = 2.0f * 0.5f * SCALE;
    float pole_x2 = cart_x + sinf(env->theta) * pole_length;
    float pole_y2 = cart_y - cosf(env->theta) * pole_length;
    DrawLineEx((Vector2){cart_x, cart_y}, (Vector2){pole_x2, pole_y2}, 5, PUFF_RED);
    
    DrawText(TextFormat("Steps: %i", env->tick), 10, 10, 20, PUFF_WHITE);
    DrawText(TextFormat("Cart Position: %.2f", env->x), 10, 40, 20, PUFF_WHITE);
    DrawText(TextFormat("Pole Angle: %.2f", env->theta * 180.0f / M_PI), 10, 70, 20, PUFF_WHITE);
    DrawText("Press LEFT/RIGHT arrows to control, ESC to exit", 10, HEIGHT - 30, 16, PUFF_WHITE);
    EndDrawing();
}

int main() {
    InitWindow(WIDTH, HEIGHT, "PufferLib Cartpole Demo");
    SetTargetFPS(60);
    
    Cartpole env = {0};
    reset_cartpole(&env);
    
    while (!WindowShouldClose()) {
        if (IsKeyDown(KEY_ESCAPE)) break;
        
        float force = 0.0f;
        if (IsKeyDown(KEY_LEFT)) force = -FORCE_MAG;
        if (IsKeyDown(KEY_RIGHT)) force = FORCE_MAG;
        
        step_cartpole(&env, force);
        
        // Reset if episode is done
        bool terminated = env.x < -X_THRESHOLD || env.x > X_THRESHOLD ||
                    env.theta < -THETA_THRESHOLD_RADIANS || env.theta > THETA_THRESHOLD_RADIANS;
        bool truncated = env.tick >= MAX_STEPS;
        
        if (terminated || truncated) {
            reset_cartpole(&env);
        }
        
        render_cartpole(&env);
    }
    
    CloseWindow();
    return 0;
}