#include "statef.h"
#include <stdlib.h>
#include <pthread.h>

int statef_create(Thread_statef* statef) {
    if (statef == NULL) {
        return -1;
    }
    *statef = (Thread_statef) {.mutex = PTHREAD_MUTEX_INITIALIZER,
                                .flag = 0};
    return 0;
}

void statef_destroy(Thread_statef* statef) {
    if (statef == NULL) {
        return;
    }
    pthread_mutex_destroy(&statef->mutex);
}

int statef_check_and_reset(Thread_statef* statef, int tocmp, int toset) {
    if (statef == NULL) {
        return -1;
    }
    int result = 0;
    pthread_mutex_lock(&statef->mutex);
    if (statef->flag == tocmp) {
        result = 1;
    }
    statef->flag = toset;
    pthread_mutex_unlock(&statef->mutex);
    return result;
}

void statef_set(Thread_statef* statef, int value) {
    if (statef == NULL) {
        return;
    }
    pthread_mutex_lock(&statef->mutex);
    statef->flag = value;
    pthread_mutex_unlock(&statef->mutex);
}
