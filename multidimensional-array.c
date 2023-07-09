#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


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
        int val = *((int*)initial_val);
        if (val == 0) {
            memset(array->contents, 0, sizeof(int) * total_size);
        }
        else {
            for (int i = 0; i < total_size; i++) {
                ((int*)array->contents)[i] = val;
            }
        }
    }
    else if (type == DOUBLE) {
        array->contents = malloc(sizeof(double) * total_size);
        double val = *((double*)initial_val);
        if (val == 0) {
            memset(array->contents, 0, sizeof(double) * total_size);
        }
        else {
            for (int i = 0; i < total_size; i++) {
                ((double*)array->contents)[i] = val;
            }
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
    int target_index = convert_to_1d_index(array, index);
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
    int dim = 7;
    int sizes[] = {10,10,10,10,10,10,10};
    int int_val = 0;

    // test make_array
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    Array *int_array = make_array(INT, dim, sizes, &int_val);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double make_array_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    

    // test set_array
    int_val = 2;
     clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            for (int k = 0; k < 9; k++) {
                for (int l = 0; l < 9; l++) {
                    for (int m = 0; m < 9; m++) {
                        for (int n = 0; n < 9; n++) {
                            for (int o = 0; o < 9; o++) {
                                set_array(int_array, (int[]){i,j,k,l,m,n,o}, &int_val);
                            }
                        }
                    }
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double set_array_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;


    // test ref_array
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            for (int k = 0; k < 9; k++) {
                for (int l = 0; l < 9; l++) {
                    for (int m = 0; m < 9; m++) {
                        for (int n = 0; n < 9; n++) {
                            for (int o = 0; o < 9; o++) {
                                int *p = (int *)ref_array(int_array, (int[]){i,j,k,l,m,n,o});
                                int ans1 = *p;
                            }
                        }
                    }
                }
            }
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double ref_array_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    free_array(int_array);

    printf("make_array_time: %f seconds\n", make_array_time);
    printf("ref_array_time: %f seconds\n", ref_array_time);
    printf("set_array_time: %f seconds\n", set_array_time);

    return 0;
}