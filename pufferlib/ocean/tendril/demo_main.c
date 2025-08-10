// Standalone demo for clean tendril architecture
// Compile with: cmake --build . --target tendril_demo_clean

#include "tendril_clean.h"

int main() {
    printf("🎯 Tendril Clean Architecture Demo\n");
    printf("==================================\n");
    
    // Initialize random seed
    srand(time(NULL));
    
    // Allocate environment
    Tendril env;
    memset(&env, 0, sizeof(Tendril));
    
    // Allocate observation/action arrays manually
    env.observations = (float*)calloc(17, sizeof(float));
    env.actions = (float*)calloc(3, sizeof(float));
    env.rewards = (float*)calloc(1, sizeof(float));
    env.terminals = (bool*)calloc(1, sizeof(bool));
    env.truncations = (bool*)calloc(1, sizeof(bool));
    
    // Initialize environment
    init(&env);
    c_reset(&env);
    
    printf("✅ Environment initialized successfully\n");
    printf("📊 Observation space: 17D\n");
    printf("🎮 Action space: 3D\n");
    printf("🎯 Target position: (%.1f, %.1f, %.1f)mm\n", 
           env.target_pos[0], env.target_pos[1], env.target_pos[2]);
    
    // Validate target reachability
    ReachabilityResult result = validate_target_reachability(
        env.target_pos[0], env.target_pos[1], env.target_pos[2]);
    
    printf("🔍 Target reachability: %s (Confidence: %.1f%%)\n",
           result.is_reachable ? "✅ REACHABLE" : "❌ UNREACHABLE",
           result.confidence * 100);
    
    if (result.is_reachable) {
        printf("🔧 Required servo angles: %.0f°, %.0f°, %.0f°\n",
               result.joint_angles[0] * 180/M_PI,
               result.joint_angles[1] * 180/M_PI,
               result.joint_angles[2] * 180/M_PI);
    }
    
    printf("\n🚀 Starting simulation loop...\n");
    printf("📋 Press ESC to exit rendering window\n");
    
    // Run simulation with rendering
    int step = 0;
    while (step < MAX_STEPS && !env.terminals[0] && !env.truncations[0]) {
        // Generate random actions for demo
        for (int i = 0; i < NUM_JOINTS; i++) {
            env.actions[i] = randf(-0.2f, 0.2f);  // Small random movements
        }
        
        // Step environment
        c_step(&env);
        
        // Render every 10 steps to avoid overwhelming output
        if (step % 10 == 0) {
            printf("⏱️  Step %d: Angular error %.1f°, Reward %.2f\n", 
                   step, env.angular_error * 180/M_PI, env.rewards[0]);
            
            // Try rendering (will create window if not exists)
            c_render(&env);
        }
        
        step++;
        
        // Break if target hit
        if (env.target_state == TARGET_SUCCESS) {
            printf("🎯 TARGET HIT! Angular error: %.1f°\n", 
                   env.angular_error * 180/M_PI);
            break;
        }
    }
    
    printf("\n📈 Final Results:\n");
    printf("   Steps taken: %d\n", step);
    printf("   Final angular error: %.1f°\n", env.angular_error * 180/M_PI);
    printf("   Target state: %s\n", 
           env.target_state == TARGET_SUCCESS ? "SUCCESS" : "ACTIVE");
    printf("   Episode return: %.2f\n", env.episode_return);
    
    // Clean up
    c_close(&env);
    free(env.observations);
    free(env.actions);
    free(env.rewards);
    free(env.terminals);
    free(env.truncations);
    
    printf("✅ Demo completed successfully!\n");
    
    return 0;
}