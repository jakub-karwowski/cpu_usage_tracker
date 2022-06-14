#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "message.h"
#include "queue.h"
#include "sized_array.h"
#include "statef.h"

#define BUFFER_INC 4096
#define NUMBER_OF_FIELDS 8
#define N_OBSERVED 3
#define RECIVED_SIGTERM 0
#define THREAD_ERROR 1

void program_terminate(int signum);
static size_t get_num_cpus(const Message* msg);
static void* thread_reader(void* arg);
static void* thread_analyzer(void* arg);
static void* thread_printer(void* arg);
static void* thread_watchdog(void* arg);

static volatile sig_atomic_t program_continue = 1;
static CP_Queue* queue_raw;
static CP_Queue* queue_processed;

void program_terminate(int signum) {
    if (signum != SIGTERM) {
        return;
    }
    program_continue = 0;
}

static void* thread_reader(void* arg) {
    Thread_statef* const flag = (Thread_statef*)arg;
    size_t buff_size = BUFFER_INC + 1;
    char* buff = malloc(sizeof(*buff) * buff_size);
    if (buff == NULL) {
        return NULL;
    }
    size_t bytes_to_read = 0;
    while (program_continue) {
        statef_set(flag, 0);
        FILE* stream;
        stream = fopen("/proc/stat", "r");
        if (stream == NULL) {
            free(buff);
            return NULL;
        }
        bytes_to_read = buff_size - 1;
        size_t n_bytes_total = 0;
        size_t n_bytes = 0;
        do {
            n_bytes = fread(buff + n_bytes_total, sizeof(*buff), bytes_to_read, stream);
            n_bytes_total += n_bytes;
            if (n_bytes == bytes_to_read) {
                if (buff_size + BUFFER_INC < buff_size) {
                    free(buff);
                    return NULL;
                }
                buff_size += BUFFER_INC;
                char* buff_temp = realloc(buff, buff_size);
                if (buff_temp == NULL) {
                    free(buff);
                    return NULL;
                }
                buff = buff_temp;
                bytes_to_read = BUFFER_INC;
            }
        } while(n_bytes == bytes_to_read);
        if (ferror(stream)) {
            free(buff);
        }
        buff[n_bytes_total] = '\0';
        cp_queue_lock(queue_raw);
        if (cp_queue_full(queue_raw)) {
            cp_queue_wait_for_consumer(queue_raw);
        }
        Message* msg = message_create(buff);
        if (msg == NULL) {
            return NULL;
        }
        if (cp_queue_enqueue(queue_raw, &msg) != 0) {
            free(msg);
            cp_queue_call_consumer(queue_raw);
            cp_queue_unlock(queue_raw);
            return NULL;
        }
        cp_queue_call_consumer(queue_raw);
        cp_queue_unlock(queue_raw);
        fclose(stream);
        sleep(1);
    }
    free(buff);
    return NULL;
}

static size_t get_num_cpus(const Message* const msg) {
    size_t num_cpus = 0;

    const char* cpu_name = "cpu";
    const size_t cpu_name_len = strlen(cpu_name);

    const char* payload_ptr = message_get_payload_const(msg);
    if (payload_ptr == NULL) {
        return 0;
    }
    char* line_end = strchr(payload_ptr, '\n');
    while (line_end != NULL && strncmp(line_end + 1, cpu_name, cpu_name_len) == 0) {
        ++num_cpus;
        line_end = strchr(line_end + 1, '\n');
    }
    return num_cpus;
}

static void* thread_analyzer(void* arg) {
    Thread_statef* const flag = (Thread_statef*)arg;
    
    statef_set(flag, 0);
    cp_queue_lock(queue_raw);
    if (cp_queue_empty(queue_raw)) {
        cp_queue_wait_for_producer(queue_raw);
    }
    Message* first_msg;
    if (cp_queue_dequeue(queue_raw, &first_msg) != 0) {
        cp_queue_call_producer(queue_raw);
        cp_queue_unlock(queue_raw);
        return NULL;
    }
    cp_queue_call_producer(queue_raw);
    cp_queue_unlock(queue_raw);

    const size_t num_cpus = get_num_cpus(first_msg);
    if (num_cpus == 0) {
        message_destroy(first_msg);
        return NULL;
    }

    unsigned long long prev[num_cpus * NUMBER_OF_FIELDS];
    memset(prev, 0, sizeof(prev));
    
    {
        char* line_end = message_get_payload(first_msg);
        line_end = strchr(line_end, '\n');
        size_t i = 0;
        while (line_end != NULL && i < num_cpus) {
            char* next_line_end = strchr(line_end + 1, '\n');
            if (next_line_end != NULL) {
                *next_line_end = '\0';
            }
            sscanf(line_end + 1, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
            &prev[i * NUMBER_OF_FIELDS + 0], &prev[i * NUMBER_OF_FIELDS + 1],
            &prev[i * NUMBER_OF_FIELDS + 2], &prev[i * NUMBER_OF_FIELDS + 3],
            &prev[i * NUMBER_OF_FIELDS + 4], &prev[i * NUMBER_OF_FIELDS + 5],
            &prev[i * NUMBER_OF_FIELDS + 6], &prev[i * NUMBER_OF_FIELDS + 7]);
            line_end = next_line_end;
            ++i;
        }
    }

    message_destroy(first_msg);

    while (program_continue) {
        statef_set(flag, 0);
        cp_queue_lock(queue_raw);
        if (cp_queue_empty(queue_raw)) {
            cp_queue_wait_for_producer(queue_raw);
        }
        Message* msg;
        if (cp_queue_dequeue(queue_raw, &msg) != 0) {
            cp_queue_call_producer(queue_raw);
            cp_queue_unlock(queue_raw);
            return NULL;
        }
        cp_queue_call_producer(queue_raw);
        cp_queue_unlock(queue_raw);
        S_array* array = s_array_create(num_cpus, sizeof(double));
        if (array == NULL) {
            message_destroy(msg);
            return NULL;
        }
        char* line_end = message_get_payload(msg);
        line_end = strchr(line_end, '\n');
        size_t i = 0;
        while (line_end != NULL && i < num_cpus) {
            char* next_line_end = strchr(line_end + 1, '\n');
            if (next_line_end != NULL) {
                *next_line_end = '\0';
            }
            
            unsigned long long user = 0,
             nice = 0, system = 0, idle = 0,
              iowait = 0, irq = 0, softirq = 0,
               steal = 0;

            sscanf(line_end + 1, "%*s %llu %llu %llu %llu %llu %llu %llu %llu", 
            &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
            unsigned long long cpu_non_idle_d = 0;
            unsigned long long cpu_idle_d = 0;
            cpu_non_idle_d += (user - prev[i * NUMBER_OF_FIELDS + 0]);
            cpu_non_idle_d += (nice - prev[i * NUMBER_OF_FIELDS + 1]);
            cpu_non_idle_d += (system - prev[i * NUMBER_OF_FIELDS + 2]);
            cpu_idle_d += (idle - prev[i * NUMBER_OF_FIELDS + 3]);
            cpu_idle_d += (iowait - prev[i * NUMBER_OF_FIELDS + 4]);
            cpu_non_idle_d += (irq - prev[i * NUMBER_OF_FIELDS + 5]);
            cpu_non_idle_d += (softirq - prev[i * NUMBER_OF_FIELDS + 6]);
            cpu_non_idle_d += (steal - prev[i * NUMBER_OF_FIELDS + 7]);
            prev[i * NUMBER_OF_FIELDS + 0] = user;
            prev[i * NUMBER_OF_FIELDS + 1] = nice;
            prev[i * NUMBER_OF_FIELDS + 2] = system;
            prev[i * NUMBER_OF_FIELDS + 3] = idle;
            prev[i * NUMBER_OF_FIELDS + 4] = iowait;
            prev[i * NUMBER_OF_FIELDS + 5] = irq;
            prev[i * NUMBER_OF_FIELDS + 6] = softirq;
            prev[i * NUMBER_OF_FIELDS + 7] = steal;
            line_end = next_line_end;
            double cpu_percent = (double)(cpu_non_idle_d) / (double)(cpu_non_idle_d + cpu_idle_d) * 100;
            s_array_put(array, i, &cpu_percent);
            ++i;
        }

        message_destroy(msg);
        cp_queue_lock(queue_processed);
        if (cp_queue_full(queue_processed)) {
            cp_queue_wait_for_consumer(queue_processed);
        }
        if (cp_queue_enqueue(queue_processed, &array) != 0) {
            free(array);
            cp_queue_call_consumer(queue_processed);
            cp_queue_unlock(queue_processed);
            return NULL;
        }
        cp_queue_call_consumer(queue_processed);
        cp_queue_unlock(queue_processed);
    }
    return NULL;
}

static void* thread_printer(void* arg) {
    Thread_statef* const flag = (Thread_statef*)arg;
    while (program_continue) {
        statef_set(flag, 0);
        cp_queue_lock(queue_processed);
        if (cp_queue_empty(queue_processed)) {
            cp_queue_wait_for_producer(queue_processed);
        }
        S_array* array;
        if (cp_queue_dequeue(queue_processed, &array) != 0) {
            cp_queue_call_producer(queue_processed);
            cp_queue_unlock(queue_processed);
            return NULL;
        }
        cp_queue_call_producer(queue_processed);
        cp_queue_unlock(queue_processed);
        const size_t array_len = s_array_size(array);
        for (size_t i = 0; i < array_len; ++i) {
            double a;
            if (s_array_get(array, i, &a) == 0) {
                printf("cpu%zu: %.02f\n", i, a);
            }
        }
        printf("\n");
        s_array_destroy(array);
    }
    return NULL;
}

static void* thread_watchdog(void* arg) {
    Thread_statef* const flags = (Thread_statef*)arg;
    int* exit_status = malloc(sizeof(int));
    *exit_status = RECIVED_SIGTERM;
    while (program_continue) {
        sleep(2);
        for (size_t i = 0; program_continue && i < N_OBSERVED; ++i) {
            if (program_continue && statef_check_and_reset(&flags[i], 1, 1) != 0) {
                *exit_status = THREAD_ERROR;
                program_continue = 0;
            }
        }
    }
    cp_queue_lock(queue_raw);
    cp_queue_call_consumer(queue_raw);
    cp_queue_unlock(queue_raw);
    cp_queue_lock(queue_processed);
    cp_queue_call_consumer(queue_processed);
    cp_queue_unlock(queue_processed);
    return (void*)exit_status;
}

int main(void) {
    struct sigaction action = {.sa_handler = program_terminate};
    sigaction(SIGTERM, &action, NULL);

    queue_raw = cp_queue_create(3, sizeof(Message*));
    if (queue_raw == NULL) {
        return 1;
    }
    queue_processed = cp_queue_create(3, sizeof(S_array*));
    if (queue_processed == NULL) {
        cp_queue_destroy(queue_raw);
        return 1;
    }
    pthread_t reader, printer, analyzer, watchdog;
    Thread_statef flags[N_OBSERVED];
    for (size_t i = 0; i < N_OBSERVED; ++i) {
        statef_create(&flags[i]);
    }
    
    pthread_create(&reader, NULL, thread_reader, (void*)&flags[0]);
    pthread_create(&analyzer, NULL, thread_analyzer, (void*){&flags[1]});
    pthread_create(&printer, NULL, thread_printer, (void*){&flags[2]});
    pthread_create(&watchdog, NULL, thread_watchdog, (void*)flags);

    int* watchdog_exit;
    pthread_join(reader, NULL);
    pthread_join(analyzer, NULL);
    pthread_join(printer, NULL);
    pthread_join(watchdog, (void**)&watchdog_exit);
    
    if (*watchdog_exit == THREAD_ERROR) {
        fprintf(stderr, "watek nie odpowiada\n");
    }

    free(watchdog_exit);
    for (size_t i = 0; i < N_OBSERVED; ++i) {
        statef_destroy(&flags[i]);
    }
    while (!cp_queue_empty(queue_raw)) {
        Message* msg;
        if (cp_queue_dequeue(queue_raw, &msg) == 0) {
            message_destroy(msg);
        }
    }
    cp_queue_destroy(queue_raw);
    while (!cp_queue_empty(queue_processed)) {
        S_array* array;
        if (cp_queue_dequeue(queue_processed, &array) == 0) {
            s_array_destroy(array);
        }
    }
    cp_queue_destroy(queue_processed);
    return 0;
}
