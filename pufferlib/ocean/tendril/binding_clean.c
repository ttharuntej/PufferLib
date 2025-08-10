// Python-C binding for Tendril environment
// CLEAN VERSION: Python wrapper only, no duplicate function bodies

#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include "tendril_clean.h"
#include <math.h>

static int cmp_float(const void* a, const void* b) {
    float x = *(const float*)a, y = *(const float*)b;
    return (x > y) - (x < y);
}

// Python module initialization
static PyObject* env_init(PyObject* self, PyObject* args) {
    PyArrayObject *observations, *actions, *rewards, *terminals, *truncations;
    int env_id, seed;
    
    if (!PyArg_ParseTuple(args, "OOOOOii", &observations, &actions, &rewards, &terminals, &truncations, &env_id, &seed)) {
        return NULL;
    }
    
    // Allocate and initialize environment
    Tendril* env = (Tendril*)malloc(sizeof(Tendril));
    if (!env) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate Tendril environment");
        return NULL;
    }
    
    // Set up PufferLib arrays
    env->observations = (float*)PyArray_DATA(observations);
    env->actions = (float*)PyArray_DATA(actions);
    env->rewards = (float*)PyArray_DATA(rewards);
    env->terminals = (bool*)PyArray_DATA(terminals);
    env->truncations = (bool*)PyArray_DATA(truncations);
    
    // Initialize environment (calls into tendril_core.c)
    init(env);
    
    // Set random seed
    srand(seed);
    
    // Reset to initial state (calls into tendril_core.c)
    c_reset(env);
    
    // Return environment pointer as Python object
    return PyLong_FromVoidPtr(env);
}

static PyObject* env_reset(PyObject* self, PyObject* args) {
    PyObject* env_ptr;
    int seed;
    
    if (!PyArg_ParseTuple(args, "Oi", &env_ptr, &seed)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (!env) {
        PyErr_SetString(PyExc_ValueError, "Invalid environment pointer");
        return NULL;
    }
    
    // Set seed and reset (calls into tendril_core.c)
    srand(seed);
    c_reset(env);
    
    Py_RETURN_NONE;
}

static PyObject* env_step(PyObject* self, PyObject* args) {
    PyObject* env_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &env_ptr)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (!env) {
        PyErr_SetString(PyExc_ValueError, "Invalid environment pointer");
        return NULL;
    }
    
    // Execute one step (calls into tendril_core.c)
    c_step(env);
    
    Py_RETURN_NONE;
}

static PyObject* env_render(PyObject* self, PyObject* args) {
    PyObject* env_ptr;
    int mode;
    
    if (!PyArg_ParseTuple(args, "Oi", &env_ptr, &mode)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (!env) {
        PyErr_SetString(PyExc_ValueError, "Invalid environment pointer");
        return NULL;
    }
    
    // Render environment (calls into render_2d.c)
    if (mode == 0) {  // Human mode
        c_render(env);
    }
    
    Py_RETURN_NONE;
}

static PyObject* env_close(PyObject* self, PyObject* args) {
    PyObject* env_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &env_ptr)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (!env) {
        PyErr_SetString(PyExc_ValueError, "Invalid environment pointer");
        return NULL;
    }
    
    // Close rendering (calls into render_2d.c)
    c_close(env);
    
    // Free environment
    free(env);
    
    Py_RETURN_NONE;
}

static PyObject* env_log(PyObject* self, PyObject* args) {
    PyObject* env_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &env_ptr)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (!env) {
        PyErr_SetString(PyExc_ValueError, "Invalid environment pointer");
        return NULL;
    }
    
    add_log(env);  // keep your existing aggregates if you like

    PyObject* log_dict = PyDict_New();

    // ---- NEW: compute rolling stats ----
    int n = env->log.hist_count;
    double meanA = 0.0, meanD = 0.0;
    for (int i = 0; i < n; i++) { meanA += env->log.ang_err_hist[i]; meanD += env->log.dperp_hist[i]; }
    if (n > 0) { meanA /= n; meanD /= n; }

    float p50A = NAN, p90A = NAN, p50D = NAN, p90D = NAN;
    if (n > 0) {
        float* A = (float*)malloc(n*sizeof(float));
        float* D = (float*)malloc(n*sizeof(float));
        memcpy(A, env->log.ang_err_hist, n*sizeof(float));
        memcpy(D, env->log.dperp_hist, n*sizeof(float));
        qsort(A, n, sizeof(float), cmp_float);
        qsort(D, n, sizeof(float), cmp_float);
        p50A = A[(int)(0.5f*(n-1))];
        p90A = A[(int)(0.9f*(n-1))];
        p50D = D[(int)(0.5f*(n-1))];
        p90D = D[(int)(0.9f*(n-1))];
        free(A); free(D);
    }

    double to_deg = 180.0 / M_PI;
    PyDict_SetItemString(log_dict, "angular_error_mean_deg", PyFloat_FromDouble(meanA * to_deg));
    PyDict_SetItemString(log_dict, "angular_error_p50_deg",  PyFloat_FromDouble(p50A * to_deg));
    PyDict_SetItemString(log_dict, "angular_error_p90_deg",  PyFloat_FromDouble(p90A * to_deg));
    PyDict_SetItemString(log_dict, "miss_distance_mean_mm",  PyFloat_FromDouble(meanD));
    PyDict_SetItemString(log_dict, "miss_distance_p50_mm",   PyFloat_FromDouble(p50D));
    PyDict_SetItemString(log_dict, "miss_distance_p90_mm",   PyFloat_FromDouble(p90D));

    int episodes = env->log.episodes;
    int hits     = env->log.hits;
    double hit_rate = (episodes > 0) ? ((double)hits / (double)episodes) : 0.0;
    PyDict_SetItemString(log_dict, "episodes", PyFloat_FromDouble((double)episodes));
    PyDict_SetItemString(log_dict, "hits",     PyFloat_FromDouble((double)hits));
    PyDict_SetItemString(log_dict, "hit_rate", PyFloat_FromDouble(hit_rate));

    // keep prior fields if you want them in User Stats as well:
    PyDict_SetItemString(log_dict, "success_rate",   PyFloat_FromDouble(env->log.success_rate / (env->log.n + 1e-8)));
    PyDict_SetItemString(log_dict, "avg_distance",   PyFloat_FromDouble(env->log.avg_distance  / (env->log.n + 1e-8)));
    PyDict_SetItemString(log_dict, "episode_length", PyFloat_FromDouble(env->log.episode_length/ (env->log.n + 1e-8)));
    PyDict_SetItemString(log_dict, "score",          PyFloat_FromDouble(env->log.score         / (env->log.n + 1e-8)));
    PyDict_SetItemString(log_dict, "n",              PyFloat_FromDouble(env->log.n));
    
    return log_dict;
}

// Vectorized operations
static PyObject* vec_reset(PyObject* self, PyObject* args) {
    PyObject* env_list;
    int seed;
    
    if (!PyArg_ParseTuple(args, "Oi", &env_list, &seed)) {
        return NULL;
    }
    
    if (!PyList_Check(env_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected list of environments");
        return NULL;
    }
    
    Py_ssize_t n_envs = PyList_Size(env_list);
    for (Py_ssize_t i = 0; i < n_envs; i++) {
        PyObject* env_ptr = PyList_GetItem(env_list, i);
        Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
        if (env) {
            srand(seed + i);
            c_reset(env);
        }
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_step(PyObject* self, PyObject* args) {
    PyObject* env_list;
    
    if (!PyArg_ParseTuple(args, "O", &env_list)) {
        return NULL;
    }
    
    if (!PyList_Check(env_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected list of environments");
        return NULL;
    }
    
    Py_ssize_t n_envs = PyList_Size(env_list);
    for (Py_ssize_t i = 0; i < n_envs; i++) {
        PyObject* env_ptr = PyList_GetItem(env_list, i);
        Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
        if (env) {
            c_step(env);
        }
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_render(PyObject* self, PyObject* args) {
    PyObject* env_list;
    int mode;
    
    if (!PyArg_ParseTuple(args, "Oi", &env_list, &mode)) {
        return NULL;
    }
    
    if (!PyList_Check(env_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected list of environments");
        return NULL;
    }
    
    // Only render first environment to avoid multiple windows
    if (PyList_Size(env_list) > 0) {
        PyObject* env_ptr = PyList_GetItem(env_list, 0);
        Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
        if (env && mode == 0) {
            c_render(env);
        }
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_close(PyObject* self, PyObject* args) {
    PyObject* env_list;
    
    if (!PyArg_ParseTuple(args, "O", &env_list)) {
        return NULL;
    }
    
    if (!PyList_Check(env_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected list of environments");
        return NULL;
    }
    
    Py_ssize_t n_envs = PyList_Size(env_list);
    for (Py_ssize_t i = 0; i < n_envs; i++) {
        PyObject* env_ptr = PyList_GetItem(env_list, i);
        Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
        if (env) {
            c_close(env);
            free(env);
        }
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_log(PyObject* self, PyObject* args) {
    PyObject* env_list;
    
    if (!PyArg_ParseTuple(args, "O", &env_list)) {
        return NULL;
    }
    
    if (!PyList_Check(env_list)) {
        PyErr_SetString(PyExc_TypeError, "Expected list of environments");
        return NULL;
    }
    
    Py_ssize_t n_envs = PyList_Size(env_list);
    if (n_envs == 0) {
        return PyDict_New();
    }
    
    // For simplicity, just use the first environment's telemetry
    // (as suggested in the one-pager)
    PyObject* env_ptr = PyList_GetItem(env_list, 0);
    Tendril* env = (Tendril*)PyLong_AsVoidPtr(env_ptr);
    if (!env) {
        return PyDict_New();
    }
    
    // Use the same logic as env_log for the first environment
    add_log(env);
    PyObject* log_dict = PyDict_New();

    // ---- NEW: compute rolling stats from first env ----
    int n = env->log.hist_count;
    double meanA = 0.0, meanD = 0.0;
    for (int i = 0; i < n; i++) { meanA += env->log.ang_err_hist[i]; meanD += env->log.dperp_hist[i]; }
    if (n > 0) { meanA /= n; meanD /= n; }

    float p50A = NAN, p90A = NAN, p50D = NAN, p90D = NAN;
    if (n > 0) {
        float* A = (float*)malloc(n*sizeof(float));
        float* D = (float*)malloc(n*sizeof(float));
        memcpy(A, env->log.ang_err_hist, n*sizeof(float));
        memcpy(D, env->log.dperp_hist, n*sizeof(float));
        qsort(A, n, sizeof(float), cmp_float);
        qsort(D, n, sizeof(float), cmp_float);
        p50A = A[(int)(0.5f*(n-1))];
        p90A = A[(int)(0.9f*(n-1))];
        p50D = D[(int)(0.5f*(n-1))];
        p90D = D[(int)(0.9f*(n-1))];
        free(A); free(D);
    }

    double to_deg = 180.0 / M_PI;
    PyDict_SetItemString(log_dict, "angular_error_mean_deg", PyFloat_FromDouble(meanA * to_deg));
    PyDict_SetItemString(log_dict, "angular_error_p50_deg",  PyFloat_FromDouble(p50A * to_deg));
    PyDict_SetItemString(log_dict, "angular_error_p90_deg",  PyFloat_FromDouble(p90A * to_deg));
    PyDict_SetItemString(log_dict, "miss_distance_mean_mm",  PyFloat_FromDouble(meanD));
    PyDict_SetItemString(log_dict, "miss_distance_p50_mm",   PyFloat_FromDouble(p50D));
    PyDict_SetItemString(log_dict, "miss_distance_p90_mm",   PyFloat_FromDouble(p90D));

    int episodes = env->log.episodes;
    int hits     = env->log.hits;
    double hit_rate = (episodes > 0) ? ((double)hits / (double)episodes) : 0.0;
    PyDict_SetItemString(log_dict, "episodes", PyFloat_FromDouble((double)episodes));
    PyDict_SetItemString(log_dict, "hits",     PyFloat_FromDouble((double)hits));
    PyDict_SetItemString(log_dict, "hit_rate", PyFloat_FromDouble(hit_rate));
    
    // Aggregate traditional metrics from all environments
    float total_success = 0, total_distance = 0, total_length = 0, total_score = 0, total_n = 0;
    for (Py_ssize_t i = 0; i < n_envs; i++) {
        PyObject* env_ptr_i = PyList_GetItem(env_list, i);
        Tendril* env_i = (Tendril*)PyLong_AsVoidPtr(env_ptr_i);
        if (env_i) {
            add_log(env_i);
            total_success += env_i->log.success_rate;
            total_distance += env_i->log.avg_distance;
            total_length += env_i->log.episode_length;
            total_score += env_i->log.score;
            total_n += env_i->log.n;
        }
    }
    
    // Add traditional aggregated metrics
    PyDict_SetItemString(log_dict, "success_rate", PyFloat_FromDouble(total_success / (total_n + 1e-8)));
    PyDict_SetItemString(log_dict, "avg_distance", PyFloat_FromDouble(total_distance / (total_n + 1e-8)));
    PyDict_SetItemString(log_dict, "episode_length", PyFloat_FromDouble(total_length / (total_n + 1e-8)));
    PyDict_SetItemString(log_dict, "score", PyFloat_FromDouble(total_score / (total_n + 1e-8)));
    PyDict_SetItemString(log_dict, "n", PyFloat_FromDouble(total_n));
    
    return log_dict;
}

static PyObject* vectorize(PyObject* self, PyObject* args) {
    // Simply return the list of environments as-is
    // PufferLib handles vectorization at the Python level
    return PyTuple_GetSlice(args, 0, PyTuple_Size(args));
}

// Method definitions
static PyMethodDef BindingMethods[] = {
    {"env_init", env_init, METH_VARARGS, "Initialize environment"},
    {"env_reset", env_reset, METH_VARARGS, "Reset environment"},
    {"env_step", env_step, METH_VARARGS, "Step environment"},
    {"env_render", env_render, METH_VARARGS, "Render environment"},
    {"env_close", env_close, METH_VARARGS, "Close environment"},
    {"env_log", env_log, METH_VARARGS, "Get environment log"},
    {"vec_reset", vec_reset, METH_VARARGS, "Reset vectorized environments"},
    {"vec_step", vec_step, METH_VARARGS, "Step vectorized environments"},
    {"vec_render", vec_render, METH_VARARGS, "Render vectorized environments"},
    {"vec_close", vec_close, METH_VARARGS, "Close vectorized environments"},
    {"vec_log", vec_log, METH_VARARGS, "Get vectorized environment logs"},
    {"vectorize", vectorize, METH_VARARGS, "Vectorize environments"},
    {NULL, NULL, 0, NULL}
};

// Module definition
static struct PyModuleDef BindingModule = {
    PyModuleDef_HEAD_INIT,
    "binding",
    NULL,
    -1,
    BindingMethods
};

// Module initialization
PyMODINIT_FUNC PyInit_binding(void) {
    import_array();
    return PyModule_Create(&BindingModule);
}