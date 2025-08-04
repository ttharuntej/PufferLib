Tendril Project: Detailed Execution Plan & Technical Roadmap (v2)Objective: To systematically advance the Tendril project from its current state to a robust, high-performance, sim-to-real-ready robotic control system.Guiding Principle: This plan follows a "Fix-then-Measure-then-Improve" methodology. Each phase builds a stable foundation for the next. Do not skip steps. The goal is not just to get a result, but to build a reliable and reproducible research platform.Phase 0: Safety Net & Setup (Due: EOD, Day 1)Rationale: Before making any changes, we must establish a safety net. Version control is non-negotiable in research; it allows us to roll back to a known-good state if an experiment fails. A centralized logging platform (Weights & Biases) is our "lab notebook," ensuring every experimental result is captured and comparable.Action Items:Tag the Current Repository: Create a permanent reference point for the current codebase.Command:git tag v0.1-baseline -m "Initial baseline before implementing unified roadmap"
git push --tags
Create a Weights & Bienses (W&B) Project: This will be the central dashboard for all training runs, metrics, and evaluations.Action: Go to wandb.ai, log in, and create a new project named tendril-sim2real.⚠️ Prerequisite: Ensure the WANDB_API_KEY environment variable is set on the training machine. Otherwise, logs will be created locally and will not appear on the online dashboard.✅ Success Check: A v0.1-baseline tag exists in the Git repository, and an empty tendril-sim2real project is visible on your W&B dashboard.Phase 1: Bug Squash & Metric Plumbing (Due: Day 4)Rationale: The current code contains subtle logic errors that corrupt our data and prevent proper evaluation. We cannot trust any experimental results until these are fixed. This phase is about ensuring our measurements are correct and our training process is stable.Task 1.1: Fix last_angular_error Double UpdateThe Issue: The last_angular_error variable is updated in two different places (compute_reward and c_step). This means the "progress" calculation (env->last_angular_error - env->angular_error) is performed using stale data from two timesteps ago.The Fix: Enforce a single, authoritative update of last_angular_error at the very end of the c_step function.Implementation (tendril.c):// In function: float compute_reward(Tendril* env)
// ...
// env->last_angular_error = env->angular_error; // <-- DELETE THIS LINE
// ...

// In function: void c_step(Tendril* env)
// ...
// env->rewards[0] = compute_reward(env);
//
// // STATE UPDATE: Prepare for next step's reward calculation
// env->last_angular_error = env->angular_error; // <-- KEEP THIS AUTHORITATIVE UPDATE
// ...
✅ Verification: The logic is sound. (Optional) Create a small C-based unit test that feeds the c_step function a known sequence of observations and asserts that the calculated progress reward matches the expected value at each step.Task 1.2: Activate Evaluation MetricsThe Issue: The update_evaluation_metrics() function is never called.The Fix: Call this function within c_step when in evaluation mode. Throttle any printf statements within the function to avoid spamming the console.Implementation (tendril.c):// In function: void update_evaluation_metrics(Tendril* env)
// Wrap any printf calls to make them optional
#ifdef VERBOSE_EVAL
// e.g. printf("Updating metrics...\n");
#endif

// In function: void c_step(Tendril* env)
// ...
env->rewards[0] = compute_reward(env);

// ---------- Auto-eval metrics and target progression ----------
if (env->auto_eval_mode) {
    update_evaluation_metrics(env);
    if (env->target_state == TARGET_SUCCESS) {
        advance_to_next_target(env);
    }
}
// ...
✅ Verification: When running in evaluation mode, the logged EvaluationMetrics struct will now contain non-zero values for avg_velocity, direction_changes, etc.Task 1.3: Restore Soft Timeout for TrainingThe Issue: No timeout in training wastes compute on unsolvable episodes. Using wall-clock time (GetTime()) is not robust to debugging pauses.The Fix: Re-introduce a timeout based on simulation steps (env->tick) and apply a specific negative reward for timing out.Implementation (tendril.c):// In function: void c_step(Tendril* env)
// ...
// (After the auto-eval block from Task 1.2)

// Soft timeout for training mode to prevent getting stuck
float elapsed_sim_time = env->tick * TAU;
if (!env->auto_eval_mode && elapsed_sim_time > 30.0f) {
    env->target_state = TARGET_TIMEOUT;
    env->terminals[0] = true;
    env->rewards[0] -= 5.0f; // Explicit penalty for failure
}
// ...
✅ Verification: In training mode, stuck episodes terminate after 30.0 / TAU steps. The W&B reward curve will show distinct negative dips corresponding to these timeout penalties.Task 1.4: Integrate PufferLib EvalCallbackThe Issue: We need a robust, automated way to run evaluations and log detailed metrics to W&B.The Fix: Use the PufferLib EvalCallback, ensuring the C++ binding is correctly configured to pass custom evaluation metrics back to the Python-based callback.Implementation (C++ Binding - binding.cpp or similar):Your PufferLib binding needs to be modified to handle the auto_eval_mode flag and to populate the info dictionary on episode termination, which the EvalCallback reads.// In your binding's step function, when an episode ends (terminal or truncation)
if (terminal || truncation) {
    if (env->auto_eval_mode) {
        // This is the crucial step: copy your C struct into the info dict
        info["eval"]["targets_completed"] = env->log.eval.targets_completed;
        info["eval"]["avg_time_to_target"] = env->log.eval.avg_time_to_target;
        info["eval"]["avg_angular_error"] = env->log.eval.avg_angular_error;
        info["eval"]["direction_changes"] = env->log.eval.direction_changes;
        info["eval"]["confidence_score"] = env->log.eval.confidence_score;
    }
}
Implementation (Python Training Script):The Python script remains largely the same, but be aware of what the callback is logging.# ... (imports and binding definition) ...
# Note on EvalCallback behavior:
# By default, SB3's EvalCallback logs the environment's mean reward as `eval/mean_reward`.
# Since our new reward function might be small, this can be misleading.
# The primary metrics to watch on W&B will be the custom ones we added to the info dict,
# which will appear as `eval/targets_completed`, `eval/avg_angular_error`, etc.
✅ Verification: A training run successfully logs to W&B. The dashboard shows charts for eval/targets_completed, eval/avg_angular_error, and the other custom metrics.Phase 2: Reward & Action Smoothing (Due: Day 6)Rationale: The core problem is "jitter," caused by the agent "reward hacking" the progress term. We will fix this by re-architecting the reward to overwhelmingly favor stability and by smoothing the agent's noisy actions before they affect the servos.Task 2.1: Re-architect the Reward FunctionThe Fix: Implement the new stability-focused reward function, ensuring the stability timer contribution is capped to prevent reward inflation after success.Implementation (tendril.c):// In function: float compute_reward(Tendril* env)
// Replace the entire function body with this:
float reward = 0.0f;
// ... (progress, action penalty, etc. from previous plan) ...

// 2. Stability Reward - The primary objective, now capped
float stability_reward = 0.0f;
if (env->angular_error < ANGULAR_THRESHOLD_RAD) {
    float stable_seconds = fminf(env->stability_timer, STABILITY_DURATION);
    stability_reward = (stable_seconds / STABILITY_DURATION) * 5.0f;
}
reward += stability_reward;
// ...
✅ Verification: A new W&B run (labeled reward_v2) shows a significant increase in eval/targets_completed and a decrease in eval/direction_changes.Task 2.2: Implement Action FilterThe Fix: Apply a simple IIR filter to the actions. Crucially, initialize the filter's state at reset and update last_actions with the filtered value to ensure the consistency penalty works correctly.Implementation (tendril.h & tendril.c):Add to Tendril struct in tendril.h:struct Tendril {
    // ... existing fields
    float filtered_actions[NUM_JOINTS]; // For IIR action filter
};
Initialize in c_reset in tendril.c:void c_reset(Tendril* env) {
    // ... existing reset code
    for (int i = 0; i < NUM_JOINTS; i++) {
        env->filtered_actions[i] = 0.0f; // CRITICAL: Zero-init on reset
    }
    // ...
}
Modify c_step in tendril.c:// In function: void c_step(Tendril* env)
// ...
// --- Action Filter (IIR Low-pass) ---
float alpha = 0.2f;
for (int i = 0; i < NUM_JOINTS; i++) {
    env->filtered_actions[i] = alpha * env->actions[i] + (1.0f - alpha) * env->filtered_actions[i];
}

// PROCESS ACTIONS - Apply FILTERED actions
// ... (use env->filtered_actions[i] in the delta calculation) ...

// ...
// STORE FILTERED ACTIONS for next step's consistency calculation
memcpy(env->last_actions, env->filtered_actions, sizeof(env->actions));
// ...
✅ Verification: An A/B test on W&B shows the filtered runs have a quantifiably lower eval/direction_changes and a lower standard deviation of angular_error during evaluation.Phase 3 & Beyond: The Research FrontierPhase 3: Curriculum & Domain Randomization:⚠️ Implementation Note: When randomizing geometric parameters like SEGMENT_LENGTH, ensure the randomized value is used in both the physics simulation (compute_forward_kinematics) and the visualization code (draw_2d_views). If they diverge, the visualization will become misleading, making debugging extremely difficult.Phase 4: Live Diagnostics: Proceed as planned.Phase 5: Advanced Policy Experiments: Proceed as planned.Project-Wide Best PracticesIncorporate these professional habits throughout the project lifecycle.Continuous Integration: Set up a minimal GitHub Action that compiles the C code on every push. This prevents merging code that doesn't build.Documentation: Maintain a changelog.md file. After every significant change (like a reward function tweak or bug fix), add a bullet point explaining what changed and why. This will be invaluable for writing papers and onboarding new lab members.Memory Safety: The filtered_actions array is new mutable state. It is safe for now, but be mindful not to make it a static or global variable if you ever move to multi-threaded environments. Run valgrind --leak-check=full on a test executable once a week to catch potential memory leaks from libraries like Raylib.This revised plan is technically sound, addresses all expert feedback, and sets the project on a clear path to success.