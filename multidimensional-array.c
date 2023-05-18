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

typedef struct {
    Type type;
    void *contents;
    int dim;
    int *sizes;
} Array;

Array* make_array(emacs_env *env, Type type, int dim, int *sizes, void *initial_val) {
    Array *array = malloc(sizeof(Array));
    int total_size = 1;

    array->type = type;
    array->dim = dim;
    array->sizes = malloc(dim * sizeof(int));

    for (int i = 0; i < dim; i++) {
        total_size *= sizes[i];
        array->sizes[i] = sizes[i];
    }
    
    // put initial value
    if (type == INT) {
        array->contents = malloc(sizeof(int) * total_size);
        for (int i = 0; i < total_size; i++) {
            ((int*)array->contents)[i] = *((int*)initial_val);
        }
    }
    else if (type == DOUBLE) {
        array->contents = malloc(sizeof(double) * total_size);
        for (int i = 0; i < total_size; i++) {
            ((double*)array->contents)[i] = *((double*)initial_val);
        }
    }
    else if (type == STRING) {
        array->contents = malloc(sizeof(char*) * total_size);
        for (int i = 0; i < total_size; i++) {
            ((char**)array->contents)[i] = strdup((char*)initial_val);
        }
    }
    else {
        array->contents = malloc(sizeof(emacs_value) * total_size);
        for (int i = 0; i < total_size; i++) {
            emacs_value protected_val = env->make_global_ref(env, *(emacs_value*)initial_val);
            ((emacs_value*)array->contents)[i] = protected_val;
        }
    }
    return array;
}

void* ref_array(Array *array, int *index){
    for (int i = 0; i < array->dim; i++) {
        if (index[i] < 0 || index[i] >= array->sizes[i]) {
            return NULL;
        }
    }
    int target_index = 0, tmp = 1;
    for (int i = array->dim - 1; i >= 0; i--) {
        target_index += index[i] * tmp;
        tmp *= array->sizes[i];
    }
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
    return NULL;
}

void set_array(Array *array, int *index, void *val) {
    for (int i = 0; i < array->dim; i++) {
        if (index[i] < 0 || index[i] >= array->sizes[i]) {
            return;
        }
    }
    int target_index = 0, tmp = 1;
    for (int i = array->dim - 1; i >= 0; i--) {
        target_index += index[i] * tmp;
        tmp *= array->sizes[i];
    }
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
}

void free_array(void *arr) {
    Array *array = (Array*)arr;
    if (array->type == STRING) {
        int total_size = 1;
        for (int i = 0; i < array->dim; i++) {
            total_size *= array->sizes[i];
        }
        for (int i = 0; i < total_size; i++) {
            free(((char**)array->contents)[i]);
        }
    }
    // else if (array->type == EMACS_VALUE) {
    //     int total_size = 1;
    //     for (int i = 0; i < total_size; i++) {
    //         emacs_value val = ((emacs_value*)array->contents)[i];
            // env->free_global_ref(env, val);
        // }
    // }
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

    int *pos = (int*)malloc(dim * sizeof(int));
    
    for (int i = 0; i < dim; i++) {
        emacs_value index = env->make_integer(env, i);
        emacs_value nth_args[] = {index, args[1]};
        emacs_value nth = env->funcall (env, env->intern (env, "nth"), 2, nth_args);
        pos[i] = env->extract_integer(env, nth);
    }

    void *val = ref_array(array, pos);
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
        C_array = make_array(env, EMACS_VALUE, dim, sizes, &initial_val);
    }

    emacs_value result = env->make_user_ptr(env, &free_array, C_array);
    free(val_type);
    free(sizes);

    return result;
}

int emacs_module_init (struct emacs_runtime *runtime)
{
    emacs_env *env = runtime->get_environment (runtime);
    
    int_val = 2;
    set_array(int_array, (int[]){1, 1, 3}, &int_val);
    int *p = (int *)ref_array(int_array, (int[]){1, 1, 3});
    int ans1 = *p;
    
    double_val = 2.0;
    set_array(double_array, (int[]){1, 1, 3}, &double_val);
    double *q = (double *)ref_array(double_array, (int[]){1, 1, 3});
    double ans2 = *q;

    string_val = "def";
    set_array(string_array, (int[]){1, 1, 3}, string_val);
    char *r = (char *)ref_array(string_array, (int[]){1, 1, 3});
    char* ans3 = r;

    printf("%d\n", ans1);
    printf("%f\n", ans2);
    printf("%s\n", ans3);
    
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

    // Indicates that the module can be used in other Emacs Lisp code
    emacs_value module_symbol = env->intern(env, "multidimensional-array");
    emacs_value provide_args[] = { module_symbol };
    env->funcall(env, env->intern(env, "provide"), 1, provide_args);
    return 0;
}
