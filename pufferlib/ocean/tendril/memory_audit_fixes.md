# Memory Management Audit & Fixes Applied

## Issues Found & Fixed

### 1. Missing NULL Checks After Allocation
**Issue**: Several `calloc()` calls lacked NULL checks, which could lead to segfaults if memory allocation failed.

**Fixed**:
- `binding.c:135` - Added NULL check for `Tendril* env` allocation
- `binding.c:153-154` - Added NULL checks for `VectorizedTendril` and `envs` array allocation
- `tendril.h:allocate()` - Added comprehensive error handling for all array allocations

### 2. Improved Error Handling in `allocate()` Function
**Issue**: Original `allocate()` function had no error handling and could leave partially initialized structs.

**Fixed**:
- Changed return type from `void` to `int` (0=success, -1=failure)
- Added cascading cleanup on allocation failure
- Ensures no memory leaks during partial allocation failures

### 3. Defensive `free_allocated()` Function
**Issue**: Original function didn't check for NULL pointers before freeing, and didn't NULL out pointers after freeing.

**Fixed**:
- Added NULL checks before each `free()` call
- Set pointers to NULL after freeing to prevent double-free bugs
- Safe to call on partially allocated or already freed structs

### 4. Proper Error Propagation
**Issue**: Allocation failures weren't properly reported to Python or calling code.

**Fixed**:
- Added Python exception setting for memory allocation failures
- Updated all callers to handle new return codes from `allocate()`
- Demo program now exits gracefully on allocation failure

## Memory Safety Features Added

- **NULL Pointer Safety**: All allocation sites now check for NULL returns
- **Cascading Cleanup**: Failed allocations properly clean up partial state
- **Double-Free Protection**: Pointers are nulled after freeing
- **Exception Propagation**: Python bindings properly report memory errors
- **Graceful Degradation**: Demo program exits cleanly on memory issues

## Build Verification

All fixes verified with:
- ✅ Strict compilation flags (`-Wall -Wextra -Werror -Wpedantic`)
- ✅ Unit tests pass
- ✅ No ODR violations detected
- ✅ No memory leaks in typical usage patterns

## Files Modified

1. **tendril.h**: Enhanced `allocate()` and `free_allocated()` functions
2. **binding.c**: Added NULL checks and error handling for Python bindings
3. **tendril.c**: Updated demo program to handle allocation failures

These improvements significantly increase the robustness of the codebase against memory-related failures and undefined behavior.