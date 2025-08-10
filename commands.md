# PufferLib Tendril Training & Evaluation Commands

## 🚀 **Working Training Command (CPU)**

### **Memory-Optimized Training (Saves every 100k steps)**
```bash
puffer train puffer_tendril \
    --train.device cpu \
    --train.total-timesteps 10000000 \
    --wandb --wandb-project "tendril-servo-limits" \
    --vec.num-envs 4 \
    --vec.num-workers 4 \
    --train.batch-size 4096 \
    --train.minibatch-size 1024 \
    --train.update-epochs 4 \
    --train.learning-rate 0.0001 \
    --train.checkpoint-interval 100000
```

### **Alternative: Medium Frequency Saves (every 250k steps)**  
```bash
puffer train puffer_tendril \
    --train.device cpu \
    --train.total-timesteps 10000000 \
    --wandb --wandb-project "tendril-servo-limits" \
    --vec.num-envs 4 \
    --vec.num-workers 4 \
    --train.batch-size 4096 \
    --train.minibatch-size 1024 \
    --train.update-epochs 4 \
    --train.learning-rate 0.0001 \
    --train.checkpoint-interval 250000
```

### **Previous Working Command (1M checkpoints - caused performance issues)**
```bash
# WARNING: This caused system slowdown due to keeping 1M steps in memory
puffer train puffer_tendril \
    --train.device cpu \
    --train.total-timesteps 10000000 \
    --wandb --wandb-project "tendril-servo-limits" \
    --vec.num-envs 4 \
    --vec.num-workers 4 \
    --train.batch-size 4096 \
    --train.minibatch-size 1024 \
    --train.update-epochs 4 \
    --train.learning-rate 0.0001 \
    --train.checkpoint-interval 1000000
```

## 🎯 **Model Evaluation Command**

### **LATEST MODEL (August 3, 2025) - Remote Server Model**
```bash
# Latest model from remote server - Step 977, compatible architecture
puffer eval puffer_tendril \
    --load-model-path experiments/puffer_tendril_8ft4hilg/model_puffer_tendril_000977.pt \
    --render-mode human \
    --train.device cpu \
    --max-runs 2 \
    --fps 10
```

### **Previous Working Model (Local)**
```bash
puffer eval puffer_tendril \
    --load-model-path experiments/puffer_tendril_c1ldraya/model_puffer_tendril_002442.pt \
    --render-mode human \
    --train.device cpu \
    --max-runs 2 \
    --fps 10
```

### **Generic Evaluation Template**
```bash
puffer eval puffer_tendril \
    --load-model-path experiments/[EXPERIMENT_DIR]/[MODEL_FILE].pt \
    --render-mode human \
    --train.device cpu \
    --max-runs 2 \
    --fps 10
```

## 📊 **Training Results (August 3, 2025)**

### **LATEST MODEL - Remote Server**
- **Latest Model**: `experiments/puffer_tendril_8ft4hilg/model_puffer_tendril_000977.pt`
- **Source**: Remote server (root@203.57.40.187:10095)
- **Training Steps**: 977,000 (latest checkpoint)
- **File Size**: 26KB (compatible architecture)
- **Status**: ✅ Model loads successfully, ⚠️ Raylib memory issue

### **Previous Local Model (August 2, 2025)**
- **Model**: `experiments/puffer_tendril_c1ldraya/model_puffer_tendril_002442.pt`
- **Training Steps**: 2,442,000 (completed)
- **Training Time**: 1h 53m 22s
- **Explained Variance**: 0.913 (excellent!)
- **Entropy**: -0.744 (good convergence)
- **SPS**: ~1.0K steps/second

## 💡 **Key Parameters Explained**

- `--train.checkpoint-interval 100000`: Saves model every 100k steps (vs 1M) to reduce memory usage and system load
- `--vec.num-envs 4`: Number of parallel environments (matches CPU cores)
- `--vec.num-workers 4`: Number of worker processes
- `--train.batch-size 4096`: Training batch size
- `--train.minibatch-size 1024`: Mini-batch size for gradient updates
- `--train.learning-rate 0.0001`: Learning rate (stable for long training)

## ⚠️ **Performance Notes**

- **100k checkpoint interval**: Recommended for system stability
- **250k checkpoint interval**: Good balance between performance and saves
- **1M checkpoint interval**: Only use if you have >16GB RAM and can tolerate system slowdown

The 100k checkpoint interval will create more model files but keep your system responsive during training.

## 🐛 **Known Issues**

### **Raylib Memory Management Issue**
- **Problem**: Double free error in raylib/OpenGL system during evaluation
- **Affects**: All evaluation commands with rendering
- **Workaround**: Model loads successfully, but visualization crashes
- **Status**: Model architecture is compatible, rendering system needs fix

### **Model Compatibility**
- ✅ **Remote model** (`puffer_tendril_8ft4hilg`) loads without state dict errors
- ✅ **Local model** (`puffer_tendril_c1ldraya`) works but has same rendering issue
- ❌ **Old model** (`puffer_tendril_j2bkiz50`) has incompatible architecture 