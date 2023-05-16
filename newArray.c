#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emacs-module.h"
// #include <emacs-module.h>

int plugin_is_GPL_compatible;

int
emacs_module_init (struct emacs_runtime *runtime)
{
    emacs_env *env = runtime->get_environment (runtime);

    
    // if (env->size >= sizeof (struct emacs_env_26))
    //     emacs_version = 26;  /* Emacs 26 or later.  */
    // else if (env->size >= sizeof (struct emacs_env_25))
    //     emacs_version = 25;
    // else
    //     return 2; /* Unknown or unsupported version.  */


    // arranges for a C function make_array to be callable as make-array from Lisp
    // an Emacs function created from the C function func
    emacs_value func = env->make_function (env, 4, 4,
                                            make_array, "Make array with data type, dimension, sizes, and initial value", NULL);
    // bind the Lisp function to a symbol, so that Lisp code could call your function by name
    emacs_value symbol = env->intern (env, "make-array");
    emacs_value args[] = {symbol, func};
    env->funcall (env, env->intern (env, "defalias"), 2, args);


    return 0;
}

typedef enum {
    INT,
    DOUBLE,
    STRING
} Type;

typedef struct {
    Type type;
    void *contents;
    int dim;
    int *sizes;
} Array;

Array* make_array(Type type, int dim, int *sizes, void *initial_val) {
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

void free_array(Array *array) {
    if (array->type == STRING) {
        // 如果是字串型別，需要額外釋放每個字串的空間
        int total_size = 1;
        for (int i = 0; i < array->dim; i++) {
            total_size *= array->sizes[i];
        }
        for (int i = 0; i < total_size; i++) {
            free(((char**)array->contents)[i]);
        }
    }
    free(array->contents);
    free(array->sizes);
    free(array);
}



int main() {
    int dim = 3;
    int sizes[] = {2, 3, 4};

    int int_val = 1;
    double double_val = 1.0;
    char* string_val = "abc";

    Array *int_array = make_array(INT, dim, sizes, &int_val);
    Array *double_array = make_array(DOUBLE, dim, sizes, &double_val);
    Array *string_array = make_array(STRING, dim, sizes, string_val);

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

    free_array(int_array);
    free_array(double_array);
    free_array(string_array);

    return 0;
}
