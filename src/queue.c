#include "queue.h"
#include <string.h>
#include <pthread.h>

struct CP_Queue {
    size_t max_n_elem;
    size_t size;
    size_t elem_size;
    size_t head;
    size_t tail;
    pthread_mutex_t mutex;
    pthread_cond_t can_produce;
    pthread_cond_t can_consume;
    u_int8_t buffer[];
};

CP_Queue* cp_queue_create(const size_t n_elem, const size_t elem_size) {
    if (n_elem == 0) {
        return NULL;
    }
    if (elem_size == 0) {
        return NULL;
    }
    CP_Queue* queue;
    queue = calloc(1, sizeof(*queue) + n_elem * elem_size);
    if (queue == NULL) {
        return NULL;
    }
    *queue = (CP_Queue){.max_n_elem = n_elem,
                        .elem_size = elem_size,
                        .mutex = PTHREAD_MUTEX_INITIALIZER,
                        .can_consume = PTHREAD_COND_INITIALIZER,
                        .can_produce = PTHREAD_COND_INITIALIZER
                        };
    return queue;
}

void cp_queue_destroy(CP_Queue* const queue) {
    if (queue == NULL) {
        return;
    }
    pthread_cond_destroy(&queue->can_consume);
    pthread_cond_destroy(&queue->can_produce);
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

bool cp_queue_empty(const CP_Queue* queue) {
    return queue->size == 0;
}

bool cp_queue_full(const CP_Queue* queue) {
    return queue->size == queue->max_n_elem;
}

int cp_queue_enqueue(CP_Queue* const restrict queue, const void* const restrict elem) {
    if (queue == NULL) {
        return -1;
    }
    if (elem == NULL) {
        return -1;
    }
    if (cp_queue_full(queue)) {
        return 1;
    }
    u_int8_t* const ptr = &queue->buffer[queue->tail * queue->elem_size];
    memcpy(ptr, elem, queue->elem_size);
    ++queue->tail;
    if (queue->tail == queue->max_n_elem) {
        queue->tail = 0;
    }
    ++queue->size;
    return 0;
}

int cp_queue_dequeue(CP_Queue* const restrict queue, void* restrict elem) {
    if (queue == NULL) {
        return -1;
    }
    if (elem == NULL) {
        return -1;
    }
    if (cp_queue_empty(queue)) {
        return 1;
    }
    u_int8_t* const ptr = &queue->buffer[queue->head * queue->elem_size];
    memcpy(elem, ptr, queue->elem_size);
    ++queue->head;
    if (queue->head == queue->max_n_elem) {
        queue->head = 0;
    }
    --queue->size;
    return 0;
}

void cp_queue_lock(CP_Queue* queue) {
    pthread_mutex_lock(&queue->mutex);
}

void cp_queue_unlock(CP_Queue* queue) {
    pthread_mutex_unlock(&queue->mutex);
}

void cp_queue_call_producer(CP_Queue* queue) {
    pthread_cond_signal(&queue->can_produce);
}

void cp_queue_call_consumer(CP_Queue* queue) {
    pthread_cond_signal(&queue->can_consume);
}

void cp_queue_wait_for_producer(CP_Queue* queue) {
    pthread_cond_wait(&queue->can_consume, &queue->mutex);
}

void cp_queue_wait_for_consumer(CP_Queue* queue) {
    pthread_cond_wait(&queue->can_produce, &queue->mutex);
}


