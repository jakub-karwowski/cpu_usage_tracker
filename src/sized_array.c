#include <stdlib.h>
#include <string.h>
#include "sized_array.h"

struct S_array {
    size_t n_elem;
    size_t elem_size;
    u_int8_t data[];
};

S_array* s_array_create(const size_t n_elem, const size_t elem_size) {
    if (n_elem == 0 || elem_size == 0) {
        return NULL;
    }
    S_array* array = calloc(1, sizeof(*array) + n_elem * elem_size);
    if (array == NULL) {
        return NULL;
    }
    *array = (S_array) {.n_elem = n_elem,
                        .elem_size = elem_size};
    return array;
}

void s_array_destroy(S_array* s_array) {
    if (s_array == NULL) {
        return;
    }
    free(s_array);
}

int s_array_get(S_array* restrict s_array, size_t index, void* restrict elem) {
    if (s_array == NULL) {
        return -1;
    }
    if (index >= s_array->n_elem) {
        return 1;
    }
    u_int8_t* ptr = &s_array->data[index * s_array->elem_size];
    memcpy(elem, ptr, sizeof(s_array->elem_size));
    return 0;
}

int s_array_put(S_array* restrict s_array, size_t index, void* restrict elem) {
    if (s_array == NULL) {
        return -1;
    }
    if (index >= s_array->n_elem) {
        return 1;
    }
    u_int8_t* ptr = &s_array->data[index * s_array->elem_size];
    memcpy(ptr, elem, sizeof(s_array->elem_size));
    return 0;
}

size_t s_array_size(const S_array* array) {
    if (array == NULL) {
        return 0;
    }
    return array->n_elem;
}
