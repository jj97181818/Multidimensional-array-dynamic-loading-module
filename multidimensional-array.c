#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emacs-module.h"
// #include <emacs-module.h>

int plugin_is_GPL_compatible;

typedef struct {
    void *contents;
    int dim;
    int *sizes;
} Array;

Array* make_array(emacs_env *env, int dim, int *sizes, void *initial_val) {
    Array *array = malloc(sizeof(Array));
    int total_size = 1;

    array->dim = dim;
    array->sizes = malloc(dim * sizeof(int));

    for (int i = 0; i < dim; i++) {
        total_size *= sizes[i];
        array->sizes[i] = sizes[i];
    }
    
    // put initial value
    array->contents = malloc(sizeof(emacs_value) * total_size);
    for (int i = 0; i < total_size; i++) {
        emacs_value protected_val = env->make_global_ref(env, initial_val);
        ((emacs_value*)array->contents)[i] = protected_val;
    }
    return array;
}

int convert_to_1d_index(Array *array, int *index) {
    for (int i = 0; i < array->dim; i++) {
        if (index[i] < 0 || index[i] >= array->sizes[i]) {
            return -1;
        }
    }
    int target_index = 0, tmp = 1;
    for (int i = array->dim - 1; i >= 0; i--) {
        target_index += index[i] * tmp;
        tmp *= array->sizes[i];
    }
    return target_index;
}

void* ref_array(Array *array, int *index){
    int target_index = convert_to_1d_index(array, index);
    if (target_index == -1) return NULL;
    return ((emacs_value*)array->contents)[target_index];
}

void set_array(emacs_env *env, Array *array, int *index, void *val) {
    int target_index = convert_to_1d_index(array, index);
    
    emacs_value old_val = ((emacs_value*)array->contents)[target_index];
    env->free_global_ref(env, old_val);

    emacs_value protected_val = env->make_global_ref(env, val);
    ((emacs_value*)array->contents)[target_index] = protected_val;
}

void free_array_finalizer(void *arr) {
    
}

void free_array(emacs_env *env, void *arr) {
    Array *array = (Array*)arr;
    
    int total_size = 1;
    for (int i = 0; i < total_size; i++) {
        emacs_value val = ((emacs_value*)array->contents)[i];
        env->free_global_ref(env, val);
    }
    free(array->contents);
    free(array->sizes);
    free(array);
}

// defun ref-array (arr coords)
// args[0]: arr ptr(emacs_val), args[1]: coords
static emacs_value ref_array_wrapper(emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data) {

    Array *array = (Array*)(env->get_user_ptr(env, args[0]));
    
    // Get index
    emacs_value safe_length = env->funcall(env, env->intern (env, "safe-length"), 1, &args[1]);
    int dim = env->extract_integer(env, safe_length);

    int pos[dim];
    
    for (int i = 0; i < dim; i++) {
        emacs_value index = env->make_integer(env, i);
        emacs_value nth_args[] = {index, args[1]};
        emacs_value nth = env->funcall (env, env->intern (env, "nth"), 2, nth_args);
        pos[i] = env->extract_integer(env, nth);
    }

    void *val = ref_array(array, pos);

    return (emacs_value)val;
}

// defun set-array (arr coords val)
// args[0]: arr, args[1]: coords, args[2]: val
static emacs_value set_array_wrapper(emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data) {
    Array *array = (Array*)(env->get_user_ptr(env, args[0]));
    
    // Get index
    emacs_value safe_length = env->funcall(env, env->intern (env, "safe-length"), 1, &args[1]);
    int dim = env->extract_integer(env, safe_length);

    int pos[dim];
    
    for (int i = 0; i < dim; i++) {
        emacs_value index = env->make_integer(env, i);
        emacs_value nth_args[] = {index, args[1]};
        emacs_value nth = env->funcall (env, env->intern (env, "nth"), 2, nth_args);
        pos[i] = env->extract_integer(env, nth);
    }

    emacs_value val = args[2];
    set_array(env, array, pos, val);

    return val;
}

// defun free-array (arr)
// args[0]: arr
static emacs_value free_array_wrapper(emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data) {
    Array *array = (Array*)(env->get_user_ptr(env, args[0]));
    free_array(env, array);
    return env->make_integer(env, 0);
}

// defun make-array (dims &optional val)
// args[0]: dims, args[1]: val
static emacs_value make_array_wrapper (emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data)
{
    emacs_value safe_length = env->funcall(env, env->intern (env, "safe-length"), 1, &args[0]);
    int dim = env->extract_integer(env, safe_length);

    int *sizes = (int*)malloc(dim * sizeof(int));
    
    for (int i = 0; i < dim; i++) {
        emacs_value index = env->make_integer(env, i);
        emacs_value nth_args[] = {index, args[0]};
        emacs_value nth = env->funcall (env, env->intern (env, "nth"), 2, nth_args);
        sizes[i] = env->extract_integer(env, nth);
    }
    
    emacs_value initial_val = args[1];

    // Create C array
    Array *C_array;
    C_array = make_array(env, dim, sizes, initial_val);

    emacs_value result = env->make_user_ptr(env, &free_array_finalizer, C_array);
    free(sizes);

    return result;
}

int emacs_module_init (struct emacs_runtime *runtime)
{
    emacs_env *env = runtime->get_environment (runtime);
    
    /* Arranges for a C function make_array to be callable as make-array from Lisp */
    // An Emacs function created from the C function make_array_wrapper
    emacs_value func = env->make_function (env, 2, 2,
                                            make_array_wrapper, "Make array sizes, and initial value", NULL);
    // Bind the Lisp function to a symbol, so that Lisp code could call function by symbol
    emacs_value function_symbol = env->intern (env, "make-array");
    emacs_value args[] = { function_symbol, func };
    env->funcall (env, env->intern (env, "defalias"), 2, args);

    /* Arranges for a C function ref_array to be callable as ref-array from Lisp */
    emacs_value ref_array_func = env->make_function (env, 2, 2,
                                            ref_array_wrapper, "Reference specific position of array", NULL);
    emacs_value ref_array_symbol = env->intern (env, "ref-array");
    emacs_value ref_array_args[] = { ref_array_symbol, ref_array_func };
    env->funcall (env, env->intern (env, "defalias"), 2, ref_array_args);

    /* Arranges for a C function set_array to be callable as set-array from Lisp */
    emacs_value set_array_func = env->make_function (env, 3, 3,
                                            set_array_wrapper, "Set a value at a specific position of array", NULL);
    emacs_value set_array_symbol = env->intern (env, "set-array");
    emacs_value set_array_args[] = { set_array_symbol, set_array_func };
    env->funcall (env, env->intern (env, "defalias"), 2, set_array_args);

    /* Arranges for a C function set_array to be callable as set-array from Lisp */
    emacs_value free_array_func = env->make_function (env, 1, 1,
                                            free_array_wrapper, "Free an array whose element type is emacs_value", NULL);
    emacs_value free_array_symbol = env->intern (env, "free-array");
    emacs_value free_array_args[] = { free_array_symbol, free_array_func };
    env->funcall (env, env->intern (env, "defalias"), 2, free_array_args);

    // Indicates that the module can be used in other Emacs Lisp code
    emacs_value module_symbol = env->intern(env, "multidimensional-array");
    emacs_value provide_args[] = { module_symbol };
    env->funcall(env, env->intern(env, "provide"), 1, provide_args);
    return 0;
}
