# PufferLib Tendril Environment - Build Instructions

## Overview
This document provides step-by-step instructions for compiling the Tendril environment after making changes to the C source code.

## Prerequisites
- Python 3.11+
- Raylib 5.5 (macOS version)
- NumPy
- C compiler (clang on macOS)

## File Structure
```
pufferlib/ocean/tendril/
├── tendril.h          # Core C header with physics + reward functions
├── tendril.c          # Main C implementation (for standalone demo)
├── binding.c          # Python-C interface for PufferLib
├── setup_binding.py   # Build script for Python extension
├── binding.so         # Compiled Python extension (generated)
└── BUILD.md          # This file
```

## When to Rebuild

**ALWAYS rebuild after modifying any of these files:**
- `tendril.h` (reward functions, physics constants)
- `tendril.c` (main simulation loop)  
- `binding.c` (Python interface, logging)

**Files that DON'T require rebuild:**
- `tendril.py` (Python wrapper)
- Configuration files
- Training scripts

## Build Steps

### 1. Clean Previous Build (Recommended)
```bash
cd /Users/jaswitharun/Documents/Development/PufferLib/pufferlib/ocean/tendril
rm -f binding.cpython-*.so *.o build/
```

### 2. Compile the Python Extension
```bash
python setup_binding.py build_ext --inplace
```

**Note**: The Makefile approach (`make clean && make -j`) may have issues with certain configurations. Use the Python setup approach above for reliable builds.

**Expected output:**
- Should complete without errors
- May show warnings (usually safe to ignore)
- Generates `binding.so` file

### 3. Verify Compilation Success
Check that the shared library was created:
```bash
ls -la binding.cpython-*.so
```

Should show a file with recent timestamp (e.g., `binding.cpython-311-darwin.so`).

### 4. Force Reload Binding (After Changes)
If you've made changes to C code, force Python to reload the binding:
```bash
python -c "import importlib, pufferlib; import pufferlib.ocean.tendril.binding as b; importlib.reload(b); print('✓ binding reloaded')"
```

### 5. Test the Binding (Optional)
```bash
python -c "from pufferlib.ocean.tendril import binding; print('✓ Import successful')"
```

### 6. Test Full Environment (Optional)
```bash
python tendril.py
```

Should show "PufferLib Tendril Environment Demo" and open a 3D window.

## Common Build Issues

### Issue: "No module named 'numpy'"
**Solution:** Install numpy first:
```bash
pip install numpy
```

### Issue: "raylib not found"
**Solution:** Check raylib path in `setup_binding.py`:
```python
raylib_include = "/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/include"
raylib_lib = "/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/lib"
```

### Issue: "binding.cpython-*.so: No such file"
**Solution:** Run the full build process:
```bash
rm -f binding.cpython-*.so
python setup_binding.py build_ext --inplace
```

### Issue: "symbol not found in flat namespace '_c_step'"
**Solution:** This indicates a C function linking issue. Rebuild completely:
```bash
rm -f binding.cpython-*.so *.o
python setup_binding.py build_ext --inplace
python -c "import importlib, pufferlib; import pufferlib.ocean.tendril.binding as b; importlib.reload(b)"
```

### Issue: Compilation warnings
**Common warnings (usually safe):**
- `unused variable 'reward'` - Expected after reward function changes
- `unused function` - Expected for helper functions not used in binding

**Serious warnings (must fix):**
- `undefined symbol` - Missing function implementation
- `incompatible types` - Type mismatch errors

## Training Integration

After successful compilation, verify training works:

```bash
puffer train puffer_tendril --train.device cpu --train.total-timesteps 10000 --vec.num-envs 1 --vec.num-workers 1
```

Should show binding version in logs: `tendril_binding_version: 20250810.0`

**Expected with surgical tweaks:**
- Episodes increment rapidly (✅ confirmed: 4→127 episodes in 50K steps)
- Episode length: 400 steps for early episodes, 800 for later episodes (✅ confirmed: steps_per_episode_last=400.0)
- More frequent resets for faster discovery (✅ confirmed: 2.5x more episodes per training time)

## Development Workflow

**For code changes:**
1. Edit C files (`tendril.h`, `binding.c`, etc.)
2. Run build steps above
3. Test with small training run
4. If successful, run full training

**For hyperparameter changes:**
1. Edit Python files only
2. No rebuild needed
3. Run training directly

## Raylib Runtime Setup (macOS)

**When running compiled code, set library path:**
```bash
export DYLD_LIBRARY_PATH="/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/lib:$DYLD_LIBRARY_PATH"
```

Or run with explicit path:
```bash
DYLD_LIBRARY_PATH=/Users/jaswitharun/Documents/Development/PufferLib/raylib-5.5_macos/lib python tendril.py
```

## Build Verification Checklist

- [ ] `binding.so` file created
- [ ] No compilation errors
- [ ] Python import works: `from pufferlib.ocean.tendril import binding`
- [ ] Environment creation works: `env = Tendril()`
- [ ] Binding version appears in training logs
- [ ] Training starts without crashes

## Emergency Rebuild

If things are completely broken:

```bash
cd /Users/jaswitharun/Documents/Development/PufferLib/pufferlib/ocean/tendril
rm -rf build/ dist/ *.so *.o
python setup_binding.py clean --all
python setup_binding.py build_ext --inplace
```

## Version Tracking

Current binding version: `20250810.0`

This version includes:
- **Surgical Tweaks for First Hits** (latest improvements):
  - Hinge reward function (2.0x forward bonus, 0.2x back penalty)
  - Dynamic episode length (400 steps for episodes <800, 800 steps after)
  - Finer action control (7.5° vs 10° per step)
- Ray-gated miss distance
- Extended 800-episode curriculum  
- IK warm-start initialization
- Stronger reward coefficients
- Success bonus fix (no double rewards)

---

**Last Updated:** August 10, 2025  
**Environment Version:** tendril-laser-pop-A-B-log  
**Raylib Version:** 5.5 (macOS)