# PufferLib Core Library Improvement Opportunities

This document tracks opportunities to improve the core PufferLib library based on real development experiences.

## 1. Vectorized Environment Auto-Reset Documentation

### Issue Discovered
**Problem**: Custom vectorized environments that don't implement auto-reset cause subtle training failures
- **Symptom**: Episodes get stuck at low counts (e.g., stuck at 2), `clipfrac→0`, no learning
- **Root Cause**: PufferLib expects environments to auto-reset internally after episode termination
- **Developer Impact**: High - very difficult to diagnose, can waste days of debugging

### Current State
- Core PufferLib works correctly when environments follow expected patterns
- Existing Ocean environments presumably implement this correctly
- **Missing**: Clear documentation of the required auto-reset pattern

### Proposed Improvement
**Add to PufferLib documentation/examples:**

#### 1. Clear Documentation
State explicitly in vectorized environment docs:
> "Vectorized environments MUST auto-reset internally when episodes terminate. After setting `terminals/truncations` flags, call your reset function if any environment is done."

#### 2. Standard Pattern Template
```c
// In your c_step function:

// 1. Set episode termination flags
env->terminals[0] = (episode_finished);
env->truncations[0] = (episode_timeout);

// 2. Compute final observations (PPO needs terminal observations)
compute_observations(env);

// 3. CRITICAL: Auto-reset if episode ended
if (env->terminals[0] || env->truncations[0]) {
    c_reset(env);  // Reset this environment for next episode
}
```

#### 3. Debugging Guide
```markdown
## Common Vectorized Environment Issues

**Symptom**: Episodes stuck at low count, `clipfrac=0`, no learning
**Likely Cause**: Missing auto-reset in episode termination
**Fix**: Add auto-reset after setting terminals/truncations
**Test**: Run environment manually - episodes should increment beyond initial count
```

#### 4. Ocean Environment Reference
Make existing ocean environments' auto-reset patterns more visible as reference implementations.

### Priority
**Medium-High** - Affects custom environment developers significantly, but doesn't impact typical users

### Implementation Effort
**Low** - Documentation/example changes only, no core code modification needed

---

## 2. [Future Opportunities]

*This section will be expanded as we discover more opportunities to improve core PufferLib during development.*

### Template for New Entries:
```
## N. [Opportunity Name]

### Issue Discovered
- Problem description
- Symptom pattern
- Root cause analysis

### Current State
- What works/doesn't work
- User impact assessment

### Proposed Improvement
- Specific changes suggested
- Code examples if applicable

### Priority
- High/Medium/Low with rationale

### Implementation Effort
- Estimated complexity
```

---

## Guidelines for Adding Entries

**When to add an entry:**
- Discovered a pattern that could confuse other developers
- Found missing documentation that caused debugging time
- Identified a common pitfall in custom development
- Spotted an opportunity for better developer experience

**What NOT to add:**
- Issues specific to our tendril environment
- Features that would change PufferLib's core API
- Minor convenience improvements with low impact

**Format:**
- Be specific about the problem and solution
- Include code examples where helpful
- Assess impact realistically (not everything needs to be "critical")
- Focus on helping future developers avoid the same issues