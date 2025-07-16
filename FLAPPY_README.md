# Flappy Environment

A simple 2-row grid environment following the PufferLib tutorial for learning RL basics.

## Environment Details
- **Grid**: 2 rows x 10 columns
- **Agent**: Moves UP (0) or DOWN (1) 
- **Observation**: Agent's Y position (0 or 1)
- **Reward**: 0 (simple case)
- **Terminal**: True after each step

## Setup

### Using UV (Recommended)
```bash
# Install uv if not already installed
curl -LsSf https://astral.sh/uv/install.sh | sh

# Create virtual environment with Python 3.11.9
uv venv --python 3.11.9

# Activate virtual environment
source .venv/bin/activate

# Install dependencies
uv pip install -e .
```

### Alternative: Regular venv
```bash
# Create virtual environment
python -m venv .venv

# Activate virtual environment
source .venv/bin/activate

# Install dependencies
pip install -e .
```

## Training

```bash
# Activate virtual environment
source .venv/bin/activate

# Train the agent (1000 timesteps, CPU, adam optimizer)
python -m pufferlib.pufferl train flappy --train.device cpu --train.total-timesteps 1000 --train.optimizer adam
```

**Expected Output:**
- Training Speed: ~2.9K steps/second on CPU
- Model Size: 323 parameters
- Compatible with macOS Ventura 13.05+ and older PyTorch versions

## Evaluation

```bash
# Evaluate with trained model (replace with your actual model path)
puffer eval flappy --load-model-path experiments/175259232883/model_007813.pt

# Or evaluate with random agent
puffer eval flappy --render-mode ansi
```

**Expected Output:**
```
+----------+
|A.........|
|..........|
+----------+
```

## Files Structure
```
pufferlib/
├── environments/flappy/
│   ├── __init__.py          # Module exports
│   ├── environment.py       # Main environment implementation
│   └── torch.py            # Neural network policy
└── config/
    └── flappy.ini          # Training configuration
```

## Configuration (flappy.ini)
- `num_envs`: 2 (parallel environments)
- `batch_size`: 128 (training batch size)
- `optimizer`: adam (compatible with older systems)
- `compile`: false (disabled for compatibility)

## Troubleshooting

### Common Issues:
1. **"No config for env_name flappy"**: Make sure you're on the `flappy-environment` branch
2. **Batch size errors**: Config is optimized for older macOS systems
3. **PyTorch compatibility**: Uses adam optimizer instead of muon for older systems

### Switch to flappy branch:
```bash
git checkout flappy-environment
```

Built following the [PufferLib tutorial](https://puffer.ai/docs.html) for creating simple RL environments. 