// Python-C binding for Tendril environment
// Auto-generated from PufferLib ocean pattern

#define TENDRIL_MATH_ONLY  // Avoid raylib dependency for pip install
#include <Python.h>
#include <time.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpy/arrayobject.h>
#include "tendril.h"

// Platform-neutral replacements for raylib dependencies
#ifdef TENDRIL_MATH_ONLY
typedef struct { float x, y; } Vector2;  // Local Vector2 definition for math-only mode
#endif

static inline float GetTime(void) {
    return (float)clock() / CLOCKS_PER_SEC;  // Platform-neutral time
}

// Utility functions - using shared randf from tendril.h

// randf function now defined in tendril.h

// Utility functions available from tendril.h: randf_env(), clampf()

static Vector2 Vector2Subtract(Vector2 v1, Vector2 v2) {
    Vector2 result = { v1.x - v2.x, v1.y - v2.y };
    return result;
}

// C function implementations - extern link to main implementations in tendril.c
// These are declared in tendril.h and implemented in tendril.c to avoid ODR violations
// Functions c_reset, c_step, c_render, c_close, make_client, close_client, add_log are extern-linked

// ============================================================================  
// PYTHON BINDING CODE STARTS HERE - All C implementations are in tendril.c
// ============================================================================

/*  The tough bits live in tendril.c. Here we only provide the
    Python <--> C glue; call the real functions via the header. */

// Python wrapper for C environment
typedef struct {
    PyObject_HEAD
    Tendril* env;
} TendrilObject;

static PyObject* tendril_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    TendrilObject* self = (TendrilObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->env = (Tendril*)calloc(1, sizeof(Tendril));
        if (self->env == NULL) {
            Py_DECREF(self);
            return NULL;
        }
        if (allocate(self->env) != 0) {
            free(self->env);
            self->env = NULL;  // Prevent double-free in dealloc
            Py_DECREF(self);
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate Tendril arrays");
            return NULL;
        }
    }
    return (PyObject*)self;
}

static void tendril_dealloc(TendrilObject* self) {
    if (self->env) {
        free_allocated(self->env);
        c_close(self->env);
        free(self->env);
        self->env = NULL;  // Prevent use-after-free
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* tendril_reset(TendrilObject* self, PyObject* args) {
    int seed = 0;
    if (!PyArg_ParseTuple(args, "|i", &seed)) {
        return NULL;
    }
    
    c_reset(self->env, (uint32_t)seed);
    Py_RETURN_NONE;
}

static PyObject* tendril_step(TendrilObject* self, PyObject* args) {
    c_step(self->env);
    Py_RETURN_NONE;
}

static PyObject* tendril_render(TendrilObject* self, PyObject* args) {
    c_render(self->env);
    Py_RETURN_NONE;
}

static PyObject* tendril_close(TendrilObject* self, PyObject* args) {
    c_close(self->env);
    Py_RETURN_NONE;
}

static PyMethodDef tendril_methods[] = {
    {"reset", (PyCFunction)tendril_reset, METH_VARARGS, "Reset environment"},
    {"step", (PyCFunction)tendril_step, METH_NOARGS, "Step environment"},  
    {"render", (PyCFunction)tendril_render, METH_NOARGS, "Render environment"},
    {"close", (PyCFunction)tendril_close, METH_NOARGS, "Close environment"},
    {NULL}
};

static PyTypeObject TendrilType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tendril.Tendril",
    .tp_doc = "Tendril environment",
    .tp_basicsize = sizeof(TendrilObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = tendril_new,
    .tp_dealloc = (destructor)tendril_dealloc,
    .tp_methods = tendril_methods,
};

// Vectorized environment management (simplified)
typedef struct {
    Tendril** envs;
    int num_envs;
} VectorizedTendril;

static PyObject* env_init(PyObject* self, PyObject* args) {
    PyObject* obs_arr, *act_arr, *rew_arr, *term_arr, *trunc_arr;
    int env_id, seed;
    
    if (!PyArg_ParseTuple(args, "OOOOOii", &obs_arr, &act_arr, &rew_arr, 
                          &term_arr, &trunc_arr, &env_id, &seed)) {
        return NULL;
    }
    
    Tendril* env = (Tendril*)calloc(1, sizeof(Tendril));
    if (env == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate Tendril environment");
        return NULL;
    }
    
    // Connect Python arrays to C pointers
    env->observations = (float*)PyArray_DATA((PyArrayObject*)obs_arr);
    env->actions = (float*)PyArray_DATA((PyArrayObject*)act_arr);
    env->rewards = (float*)PyArray_DATA((PyArrayObject*)rew_arr);
    env->terminals = (bool*)PyArray_DATA((PyArrayObject*)term_arr);
    env->truncations = (bool*)PyArray_DATA((PyArrayObject*)trunc_arr);
    
    init(env);
    c_reset(env, (uint32_t)seed);
    
    return PyLong_FromVoidPtr(env);
}

static PyObject* vectorize(PyObject* self, PyObject* args) {
    Py_ssize_t num_envs = PyTuple_Size(args);
    VectorizedTendril* vec = (VectorizedTendril*)calloc(1, sizeof(VectorizedTendril));
    if (vec == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate VectorizedTendril");
        return NULL;
    }
    
    vec->envs = (Tendril**)calloc(num_envs, sizeof(Tendril*));
    if (vec->envs == NULL) {
        free(vec);
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate environment array");
        return NULL;
    }
    vec->num_envs = num_envs;
    
    for (Py_ssize_t i = 0; i < num_envs; i++) {
        PyObject* env_ptr = PyTuple_GetItem(args, i);
        vec->envs[i] = (Tendril*)PyLong_AsVoidPtr(env_ptr);
        if (PyErr_Occurred()) {
            free(vec->envs);
            free(vec);
            PyErr_SetString(PyExc_TypeError, "Expected integer pointer for environment");
            return NULL;
        }
    }
    
    return PyLong_FromVoidPtr(vec);
}

static PyObject* vec_reset(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    int seed;
    
    if (!PyArg_ParseTuple(args, "Oi", &vec_ptr, &seed)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    if (PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Expected integer pointer for vectorized environment");
        return NULL;
    }
    for (int i = 0; i < vec->num_envs; i++) {
        // Use different seed for each environment for independent RNG streams
        c_reset(vec->envs[i], (uint32_t)(seed + i * 1000));
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_step(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    if (PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Expected integer pointer for vectorized environment");
        return NULL;
    }
    
    for (int i = 0; i < vec->num_envs; i++) {
        c_step(vec->envs[i]);
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_render(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    int env_id;
    
    if (!PyArg_ParseTuple(args, "Oi", &vec_ptr, &env_id)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    if (PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Expected integer pointer for vectorized environment");
        return NULL;
    }
    
    if (env_id >= 0 && env_id < vec->num_envs) {
        c_render(vec->envs[env_id]);
    }
    
    Py_RETURN_NONE;
}

static PyObject* vec_close(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    if (PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Expected integer pointer for vectorized environment");
        return NULL;
    }
    
    for (int i = 0; i < vec->num_envs; i++) {
        c_close(vec->envs[i]);
        free(vec->envs[i]);
    }
    
    free(vec->envs);
    free(vec);
    
    Py_RETURN_NONE;
}

static PyObject* vec_log(PyObject* self, PyObject* args) {
    PyObject* vec_ptr;
    
    if (!PyArg_ParseTuple(args, "O", &vec_ptr)) {
        return NULL;
    }
    
    VectorizedTendril* vec = (VectorizedTendril*)PyLong_AsVoidPtr(vec_ptr);
    if (PyErr_Occurred()) {
        PyErr_SetString(PyExc_TypeError, "Expected integer pointer for vectorized environment");
        return NULL;
    }
    
    // Return aggregated log data
    PyObject* log_dict = PyDict_New();
    
    if (vec->num_envs > 0) {
        Tendril* env = vec->envs[0];  // Use first environment's log
        PyDict_SetItemString(log_dict, "success_rate", PyFloat_FromDouble(env->log.success_rate / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "avg_distance", PyFloat_FromDouble(env->log.avg_distance / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "episode_length", PyFloat_FromDouble(env->log.episode_length / fmaxf(env->log.n, 1.0f)));
        PyDict_SetItemString(log_dict, "score", PyFloat_FromDouble(env->log.score));
        PyDict_SetItemString(log_dict, "episodes", PyFloat_FromDouble(env->log.n));
    }
    
    return log_dict;
}

static PyMethodDef module_methods[] = {
    {"env_init", env_init, METH_VARARGS, "Initialize environment"},
    {"vectorize", vectorize, METH_VARARGS, "Create vectorized environments"},
    {"vec_reset", vec_reset, METH_VARARGS, "Reset vectorized environments"},
    {"vec_step", vec_step, METH_VARARGS, "Step vectorized environments"},
    {"vec_render", vec_render, METH_VARARGS, "Render vectorized environments"},
    {"vec_close", vec_close, METH_VARARGS, "Close vectorized environments"},
    {"vec_log", vec_log, METH_VARARGS, "Get vectorized environment logs"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module_definition = {
    PyModuleDef_HEAD_INIT,
    "binding",
    "Tendril environment C bindings",
    -1,
    module_methods
};

PyMODINIT_FUNC PyInit_binding(void) {
    import_array();
    
    PyObject* module = PyModule_Create(&module_definition);
    if (module == NULL) {
        return NULL;
    }
    
    if (PyType_Ready(&TendrilType) < 0) {
        return NULL;
    }
    
    Py_INCREF(&TendrilType);
    if (PyModule_AddObject(module, "Tendril", (PyObject*)&TendrilType) < 0) {
        Py_DECREF(&TendrilType);
        Py_DECREF(module);
        return NULL;
    }
    
    return module;
}