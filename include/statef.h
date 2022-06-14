#ifndef STATEF_H
#define STATEF_H

#include <stdlib.h>
#include <pthread.h>

typedef struct Thread_statef {
    int flag;
    char pad[4];
    pthread_mutex_t mutex;
} Thread_statef;

int statef_create(Thread_statef* statef);

void statef_destroy(Thread_statef* statef);

int statef_check_and_reset(Thread_statef* statef, int tocmp, int toset);

void statef_set(Thread_statef* statef, int value);
#endif // !THREAD_STATEF_H
