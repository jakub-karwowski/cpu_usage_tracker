#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct CP_Queue CP_Queue;

CP_Queue* cp_queue_create(size_t n_elem, size_t elem_size);

void cp_queue_destroy(CP_Queue* queue);

bool cp_queue_empty(const CP_Queue* queue);

bool cp_queue_full(const CP_Queue* queue);

int cp_queue_enqueue(CP_Queue* restrict queue, const void* restrict elem);

int cp_queue_dequeue(CP_Queue* restrict queue, void* restrict elem);

void cp_queue_lock(CP_Queue* queue);

void cp_queue_unlock(CP_Queue* queue);

void cp_queue_call_producer(CP_Queue* queue);

void cp_queue_call_consumer(CP_Queue* queue);

void cp_queue_wait_for_producer(CP_Queue* queue);

void cp_queue_wait_for_consumer(CP_Queue* queue);

#endif // !QUEUE_h
