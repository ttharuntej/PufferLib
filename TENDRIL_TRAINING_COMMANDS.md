# 🎯 Tendril Training - Working Commands Documentation

## ✅ Verified Working Training Command

After extensive debugging and batch size configuration testing, this is the **confirmed working** command:

```bash
puffer train puffer_tendril \
  --wandb \
  --wandb-project "tendril-fixed-training" \
  --train.total-timesteps 10000000 \
  --vec.num-workers 1 \
  --vec.num-envs 1 \
  --train.batch-size 65536 \
  --train.learning-rate 2.5e-4
```

## 🔧 Critical Fixes Applied

### 1. Reward Function Fix
- **Problem**: Original reward penalized movement (-0.1 * |actions|) causing model to learn stillness
- **Solution**: Complete rewrite with positive movement rewards and success bonuses up to 37 points
- **Location**: `pufferlib/ocean/tendril/tendril.h` - `compute_reward()` function

### 2. Action Scaling Fix
- **Problem**: Mismatch between training (5°/step) and evaluation (0.1 rad/step ≈ 5.7°/step)
- **Solution**: Unified to 15°/step for realistic servo movement
- **Location**: `pufferlib/ocean/tendril/binding.c:69` - Action processing in `c_step()`

### 3. Servo Physics Validation
- **Confirmed**: 0-180° servo limits properly implemented and enforced
- **Location**: `pufferlib/ocean/tendril/tendril.h` - Joint limit constants

### 4. Interactive Target Placement
- **Added**: 3D ray-casting system for right-click target placement
- **Location**: `pufferlib/ocean/tendril/binding.c:140-185` - Interactive controls

### 5. Smart Stopping Mechanism
- **Added**: Target state management (ACTIVE/SUCCESS/TIMEOUT) with proper stopping conditions
- **Location**: `pufferlib/ocean/tendril/binding.c:84-106` - Target state logic

## 🚨 Batch Size Configuration Notes

**Critical**: PufferLib has constraint: `segments >= total_agents` where `segments = batch_size ÷ horizon`

### Failed Configurations (DO NOT USE):
```bash
# ❌ These will fail with "Total agents X <= segments Y" errors
--vec.num-envs 8 --train.batch-size 256
--vec.num-envs 4 --train.batch-size 512  
--vec.num-envs 2 --train.batch-size 1024
```

### Working Configuration Mathematics:
- `batch_size = 65536`
- `horizon = 128` (default)
- `segments = 65536 ÷ 128 = 512`
- `total_agents = num_workers × num_envs = 1 × 1 = 1`
- `512 >= 1` ✅ **VALID**

## 🌐 W&B Integration Setup

```bash
# Login to Weights & Biases
wandb login 4cda67ad3dd53dd482792634ffd2bb8f3671285b

# Or create .netrc file:
echo "machine api.wandb.ai" >> ~/.netrc
echo "  login user" >> ~/.netrc  
echo "  password 4cda67ad3dd53dd482792634ffd2bb8f3671285b" >> ~/.netrc
```

## 📊 Training Performance Expectations

After fixes, expect these improvements:
- **Rewards**: Should be **POSITIVE** (was negative before due to movement penalty)
- **Success Rate**: Should increase steadily (more GREEN targets)
- **Red Timeouts**: Should decrease significantly 
- **Movement**: Faster, more purposeful tendril motion
- **SPS**: ~16K steps per second with single environment
- **Training Time**: ~10-30 minutes for 10M steps

## 🧪 Post-Training Evaluation

Test the trained model with interactive 3D visualization:

```bash
puffer eval puffer_tendril --render-mode human --max-runs 1
```

**Interactive Controls**:
- Right-click: Place target anywhere in 3D workspace
- Left-click + drag: Rotate camera view
- Mouse wheel: Zoom in/out
- ESC: Exit
- TAB: Toggle fullscreen

## 📈 Monitoring Dashboard

W&B dashboard will show:
- Policy/Value loss curves (should decrease)
- Average reward (should be positive and increase)
- Success rate (percentage of green targets reached)
- Steps per second (training performance)

**Dashboard URL**: https://wandb.ai/[username]/tendril-fixed-training/runs/[run-id]

## 🔍 Diagnostic Tools Available

1. **Basic Training Test**: `python test_tendril_training.py`
2. **Reward Function Validation**: `python test_fixed_rewards.py`  
3. **Servo Limits Check**: `python test_servo_limits.py`
4. **Training Issue Diagnosis**: `python diagnose_training_issue.py`

## 🎯 Success Criteria

A successful training run should show:
- ✅ Policy loss decreasing over time
- ✅ Value loss decreasing over time  
- ✅ Average reward > 0 and increasing
- ✅ Success rate increasing towards 60-80%
- ✅ Explained variance > 0.8
- ✅ No "agent <= segments" errors

## 🚀 Next Steps After Training

1. Test model with interactive evaluation
2. Validate performance on various target placements
3. Consider longer training runs (50M+ steps) for expert performance
4. Experiment with curriculum learning for harder targets

---

**Last Updated**: July 30, 2025  
**Validated Configuration**: ✅ Confirmed working on macOS with 4-core hardware  
**W&B Integration**: ✅ Tested and functional