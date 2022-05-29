#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "message.h"
#include "queue.h"
#include "sized_array.h"

#define BUFFER_INC 1024
#define NUMBER_OF_FIELDS 8

CP_Queue* queue_raw;
CP_Queue* queue_processed;

static void* thread_reader(void* unused) {
    size_t buff_size = BUFFER_INC + 1;
    char* buff = malloc(sizeof(*buff) * buff_size);
    if (buff == NULL) {
        return NULL;
    }
    size_t bytes_to_read = 0;
    while (1) {
        FILE* stream;
        stream = fopen("/proc/stat", "r");
        if (stream == NULL) {
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
                buff = realloc(buff, buff_size);
                if (buff == NULL) {
                    return NULL;
                }
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
        if (msg != NULL) {
            cp_queue_enqueue(queue_raw, &msg);
            cp_queue_call_consumer(queue_raw);
        }
        cp_queue_unlock(queue_raw);
        fclose(stream);
        sleep(1);
    }
    free(buff);
    return NULL;
}

static long get_num_cpus(Message* msg) {
    long num_cpus = 0;

    static const char* cpu_name = "cpu";
    const size_t cpu_name_len = strlen(cpu_name);

    char* line_end = message_get_payload(msg);
    if (line_end == NULL) {
        return 0;
    }
    line_end = strchr(line_end, '\n');
    while (line_end != NULL && strncmp(line_end + 1, cpu_name, cpu_name_len) == 0) {
        ++num_cpus;
        line_end = strchr(line_end + 1, '\n');
    }
    return num_cpus;
}

static void* thread_analyzer(void* unused) {
    //long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    
    cp_queue_lock(queue_raw);
    if (cp_queue_empty(queue_raw)) {
         cp_queue_wait_for_producer(queue_raw);
    }
    Message* first_msg;
    cp_queue_dequeue(queue_raw, &first_msg);
    if (first_msg == NULL) {
        return NULL;
    }
    cp_queue_call_producer(queue_raw);
    cp_queue_unlock(queue_raw);

    const long num_cpus = get_num_cpus(first_msg);
    if (num_cpus == 0) {
        return NULL;
    }

    unsigned long long prev[num_cpus * NUMBER_OF_FIELDS];
    memset(prev, 0, sizeof(prev));
    /* fill prev */
    {
        char* line_end = message_get_payload(first_msg);
        line_end = strchr(line_end, '\n');
        long i = 0;
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

    while (1) {
        cp_queue_lock(queue_raw);
        if (cp_queue_empty(queue_raw)) {
            cp_queue_wait_for_producer(queue_raw);
        }
        Message* msg;
        cp_queue_dequeue(queue_raw, &msg);
        if (msg == NULL) {
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
        long i = 0;
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
            double cpu_percent = (double)(cpu_non_idle_d) / (cpu_non_idle_d + cpu_idle_d) * 100;
            s_array_put(array, i, &cpu_percent);
            ++i;
        }

        message_destroy(msg);
        cp_queue_lock(queue_processed);
        if (cp_queue_full(queue_processed)) {
            cp_queue_wait_for_consumer(queue_processed);
        }
        cp_queue_enqueue(queue_processed, &array);
        cp_queue_call_consumer(queue_processed);
        cp_queue_unlock(queue_processed);
    }
    return NULL;
}

static void* thread_printer(void* unused) {
    while (1) {
        cp_queue_lock(queue_processed);
        if (cp_queue_empty(queue_processed)) {
            cp_queue_wait_for_producer(queue_processed);
        }
        S_array* array;
        cp_queue_dequeue(queue_processed, &array);
        if (array == NULL) {
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

int main(void) {
    queue_raw = cp_queue_create(3, sizeof(Message*));
    if (queue_raw == NULL) {
        return 1;
    }
    queue_processed = cp_queue_create(3, sizeof(S_array*));
    if (queue_processed == NULL) {
        cp_queue_destroy(queue_raw);
        return 1;
    }
    pthread_t reader, printer, analyzer;
    pthread_create(&reader, NULL, thread_reader, NULL);
    pthread_create(&analyzer, NULL, thread_analyzer, NULL);
    pthread_create(&printer, NULL, thread_printer, NULL);

    pthread_join(reader, NULL);
    pthread_join(analyzer, NULL);
    pthread_join(printer, NULL);

    cp_queue_destroy(queue_raw);
    cp_queue_destroy(queue_processed);
    return 0;
}
