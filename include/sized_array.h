#ifndef SIZED_ARRAY_H
#define SIZED_ARRAY_H

#include <stdlib.h>

typedef struct S_array S_array;

S_array* s_array_create(size_t n_elem, size_t s_elem);

void s_array_destroy(S_array* s_array);

int s_array_put(S_array* restrict s_array, size_t index, void* restrict elem);

int s_array_get(S_array* restrict s_array, size_t index, void* restrict elem);

size_t s_array_size(const S_array* array);

#endif // !SIZED_ARRAY_H
