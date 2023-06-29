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

typedef struct MultiDimArray Array;
struct MultiDimArray {
    Type type;
    void *contents;
    int dim;
    int *sizes;
    Array **subarray;
};

Array* make_array(Type type, int dim, int level, int *sizes, void *initial_val) {
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
    }
    else {
        array->subarray = malloc(sizeof(Array*) * size);
        for (int i = 0; i < size; i++) {
            array->subarray[i] = make_array(type, dim, level + 1, sizes, initial_val);
        }
    }
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
    }
    else {
        return ref_array(array->subarray[target_index], index, level + 1);
    }
    return NULL;
}

void set_array(Array *array, int *index, void *val, int level) {
    int target_index = index[level];
    if (array->subarray == NULL) {
        if (array->type == INT) {
            ((int*)array->contents)[target_index] = *((int*)val);
        }
        else if (array->type == DOUBLE) {
            ((double*)array->contents)[target_index] = *((double*)val);
        }
        else if (array->type == STRING) {
            if (((char**)array->contents)[target_index] != NULL) {
                free(((char**)array->contents)[target_index]);
            }
            ((char**)array->contents)[target_index] = strdup((char*)val);
        }
    }
    else {
        set_array(array->subarray[target_index], index, val, level + 1);
    }
}

void free_array(Array *arr, int index) {
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
            free_array(arr->subarray[i], index + 1);
        }
        free(arr->subarray);
    }
    free(arr->sizes);
    free(arr);
}


int main() {
    int dim = 7;
    int sizes[] = {10,10,10,10,10,10,10};
    int int_val = 1;

    // test make_array
     struct timespec start, end;
     clock_gettime(CLOCK_MONOTONIC, &start);
    Array *int_array = make_array(INT, dim, 0, sizes, &int_val);
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
                                set_array(int_array, (int[]){i,j,k,l,m,n,o}, &int_val, 0);
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
                                int *p = (int *)ref_array(int_array, (int[]){i,j,k,l,m,n,o}, 0);
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

    free_array(int_array, 0);

     printf("make_array_time: %f seconds\n", make_array_time);
     printf("set_array_time: %f seconds\n", set_array_time);
     printf("ref_array_time: %f seconds\n", ref_array_time);

    return 0;
}
