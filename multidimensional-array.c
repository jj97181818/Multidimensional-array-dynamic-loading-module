#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emacs-module.h"
// #include <emacs-module.h>

int plugin_is_GPL_compatible;

typedef enum {
    INT,
    DOUBLE,
    STRING,
    EMACS_VALUE
} Type;

typedef struct MultiDimArray Array;
struct MultiDimArray {
    Type type;
    void *contents;
    int dim;
    int *sizes;
    Array **subarray;
};


Array* make_nested_array(emacs_env *env, Type type, int dim, int level, int *sizes, void *initial_val) {
    Array* array = malloc(sizeof(Array));
    array->type = type;
    array->contents = NULL;
    array->dim = dim;
    array->sizes = malloc(dim * sizeof(int));
    memcpy(array->sizes, sizes, dim * sizeof(int));
    array->subarray = NULL;
    
    int size = sizes[level];

    if (level == dim - 1) {
        // put initial value
        if (type == INT) {
            array->contents = malloc(sizeof(int) * size);
            int val = *((int*)initial_val);
            for (int i = 0; i < size; i++) {
                ((int*)array->contents)[i] = val;
            }
        }
        else if (type == DOUBLE) {
            array->contents = malloc(sizeof(double) * size);
            double val = *((double*)initial_val);
            for (int i = 0; i < size; i++) {
                ((double*)array->contents)[i] = val;
            }
        }
        else if (type == STRING) {
            array->contents = malloc(sizeof(char*) * size);
            for (int i = 0; i < size; i++) {
                ((char**)array->contents)[i] = NULL;
            }
            for (int i = 0; i < size; i++) {
                ((char**)array->contents)[i] = strdup((char*)initial_val);
            }
        }
        else {
            array->contents = malloc(sizeof(emacs_value) * size);
            for (int i = 0; i < size; i++) {
                emacs_value protected_val = env->make_global_ref(env, initial_val);
                ((emacs_value*)array->contents)[i] = protected_val;
            }
        }
    }
    else {
        array->subarray = malloc(sizeof(Array*) * size);
        for (int i = 0; i < size; i++) {
            array->subarray[i] = make_nested_array(env, type, dim, level + 1, sizes, initial_val);
        }
    }
    return array;
}


Array* make_array(emacs_env *env, Type type, int dim, int *sizes, void *initial_val) {
    Array *array = malloc(sizeof(Array));
    // int total_size = 1;

    array->type = type;
    array->dim = dim;
    array->sizes = malloc(dim * sizeof(int));

    for (int i = 0; i < dim; i++) {
        // total_size *= sizes[i];
        array->sizes[i] = sizes[i];
    }
    
    array = make_nested_array(env, type, dim, 0, sizes, initial_val);

    return array;
}

void* ref_array(Array *array, int *index, int level){
    int target_index = index[level];
    if (array->subarray == NULL) {
        if (array->type == INT) {
            return &((int*)array->contents)[target_index];
        }
        else if (array->type == DOUBLE) {
            return &((double*)array->contents)[target_index];
        }
        else if (array->type == STRING) {
            return ((char**)array->contents)[target_index];
        }
        else if (array->type == EMACS_VALUE) {
            return ((emacs_value*)array->contents)[target_index];
        }
    }
    else {
        return ref_array(array->subarray[target_index], index, level + 1);
    }
    return NULL;
}

void set_array(emacs_env *env, Array *array, int *index, void *val, int level) {
    int target_index = index[level];
    if (array->subarray == NULL) {
        if (array->type == INT) {
            ((int*)array->contents)[target_index] = *((int*)val);
        }
        else if (array->type == DOUBLE) {
            ((double*)array->contents)[target_index] = *((double*)val);
        }
        else if (array->type == STRING) {
            free(((char**)array->contents)[target_index]);
            ((char**)array->contents)[target_index] = strdup((char*)val);
        }
        else {
            emacs_value old_val = ((emacs_value*)array->contents)[target_index];
            env->free_global_ref(env, old_val);

            emacs_value protected_val = env->make_global_ref(env, val);
            ((emacs_value*)array->contents)[target_index] = protected_val;
        }
    }
    else {
        set_array(env, array->subarray[target_index], index, val, level + 1);
    }
}

void free_subarray(Array *arr, int index) {
    if (arr->subarray == NULL) {
        if (arr->type == STRING) {
            int size = arr->sizes[arr->dim - 1];
            for (int i = 0; i < size; i++) {
                free(((char**)arr->contents)[i]);
            }
        }
        free(arr->contents);
    }
    else {
        int size = arr->sizes[index];
        for (int i = 0; i < size; i++) {
            free_subarray(arr->subarray[i], index + 1);
        }
        free(arr->subarray);
    }
    free(arr->sizes);
}

void free_array_finalizer(void *arr) {
    free_subarray(arr, 0);
    free(arr);
}

void free_array(emacs_env *env, Array *arr) {
    if (arr->subarray == NULL) {
        if (arr->type == EMACS_VALUE) {
            int size = arr->sizes[0];
            for (int i = 0; i < size; i++) {
                env->free_global_ref(env, ((emacs_value*)arr->contents)[i]);
            }
        }
        free(arr->contents);
    }   
    else {
        int size = arr->sizes[0];
        for (int i = 0; i < size; i++) {
            free_array(env, arr->subarray[i]);
        }
        free(arr->subarray);
    }
    free(arr->sizes);
    free(arr);
}

// defun ref-array (arr coords)
// args[0]: arr ptr(emacs_val), args[1]: coords
static emacs_value ref_array_wrapper(emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data) {

    Array *array = (Array*)(env->get_user_ptr(env, args[0]));

    // Get index
    int dim = array->dim;
    int pos[dim];
    emacs_value elements = args[1];
    emacs_value Fcar = env->intern(env, "car");
    emacs_value Fcdr = env->intern(env, "cdr");
    for (int i = 0; i < dim; i++) {
        pos[i] = env->extract_integer(env, env->funcall(env, Fcar, 1, &elements));
        elements = env->funcall(env, Fcdr, 1, &elements);
    }

    void *val = ref_array(array, pos, 0);
    emacs_value result;
    if (array->type == INT) {
        result = env->make_integer(env, *((int*)val));
    }
    else if (array->type == DOUBLE) {
        result = env->make_float(env, *((float*)val));
    }
    else if (array->type == STRING) {
        result = env->make_string(env, val, strlen(val));
    }
    else { // EMACS_VALUE
        result = (emacs_value)val;
    }
    return result;
}

// defun set-array (arr coords val)
// args[0]: arr, args[1]: coords, args[2]: val
static emacs_value set_array_wrapper(emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data) {
    Array *array = (Array*)(env->get_user_ptr(env, args[0]));

    // Get index
    int dim = array->dim;
    int pos[dim];
    emacs_value elements = args[1];
    emacs_value Fcar = env->intern(env, "car");
    emacs_value Fcdr = env->intern(env, "cdr");
    for (int i = 0; i < dim; i++) {
        pos[i] = env->extract_integer(env, env->funcall(env, Fcar, 1, &elements));
        elements = env->funcall(env, Fcdr, 1, &elements);
    }

    emacs_value val = args[2];
    if (array->type == INT) {
        int integer_val = env->extract_integer(env, val);
        set_array(env, array, pos, &integer_val, 0);
    }
    else if (array->type == DOUBLE) {
        double double_val = env->extract_float(env, val);
        set_array(env, array, pos, &double_val, 0);
    }
    else if (array->type == STRING) {
        ptrdiff_t len;
        env->copy_string_contents(env, val, NULL, &len);
        char *string_val = (char*)malloc(len);
        env->copy_string_contents(env, val, string_val, &len);
        set_array(env, array, pos, string_val, 0);
    }
    else { // EMACS_VALUE
        set_array(env, array, pos, val, 0);
    }
    return val;
}

// defun free-array (arr)
// args[0]: arr
static emacs_value free_array_wrapper(emacs_env *env, ptrdiff_t nargs, emacs_value *args, void *data) {
    Array *array = (Array*)(env->get_user_ptr(env, args[0]));
    if (array->type != EMACS_VALUE) {
        return env->make_integer(env, -1);
    }
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
    
    // Get the type of argument val
    emacs_value type = env->funcall(env, env->intern(env, "type-of"), 1, &args[1]);
    type = env->funcall(env, env->intern(env, "symbol-name"), 1, &type);
    ptrdiff_t len;

    // Put the size of the type into len
    env->copy_string_contents(env, type, NULL, &len);
    char *val_type = (char*)malloc(len);
    env->copy_string_contents(env, type, val_type, &len);

    emacs_value initial_val = args[1];

    // Create C array
    Array *C_array;
    if (strcmp(val_type, "integer") == 0) {
        int64_t integer_val = env->extract_integer(env, initial_val);
        C_array = make_array(env, INT, dim, sizes, &integer_val);
    }
    else if (strcmp(val_type, "float") == 0) {
        double double_val = env->extract_float(env, initial_val);
        C_array = make_array(env, DOUBLE, dim, sizes, &double_val);
    }
    else if (strcmp(val_type, "string") == 0) {
        ptrdiff_t len;
        env->copy_string_contents(env, initial_val, NULL, &len);
        char *string_val = (char*)malloc(len);
        env->copy_string_contents(env, initial_val, string_val, &len);
        C_array = make_array(env, STRING, dim, sizes, string_val);
    }
    else {
        C_array = make_array(env, EMACS_VALUE, dim, sizes, initial_val);
    }

    emacs_value result = env->make_user_ptr(env, &free_array_finalizer, C_array);
    free(val_type);
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
