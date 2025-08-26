#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX_LOG_SIZE 10000

struct timeval start_time_global;
static char log_buffer[MAX_LOG_SIZE];
static int log_index = 0;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

long get_current_time_ms() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    return (current_time.tv_sec - start_time_global.tv_sec) * 1000 + 
           (current_time.tv_usec - start_time_global.tv_usec) / 1000;
}

void add_to_log(const char* message) {
    pthread_mutex_lock(&log_mutex);
    int len = strlen(message);
    if (log_index + len + 1 < MAX_LOG_SIZE) {
        strcpy(log_buffer + log_index, message);
        log_index += len;
        log_buffer[log_index++] = '\n';
        log_buffer[log_index] = '\0';
    }
    pthread_mutex_unlock(&log_mutex);
}

void save_log_to_file() {
    FILE* file = fopen("log_execucao_minikernel.txt", "w");
    if (file) {
        fwrite(log_buffer, 1, log_index, file);
        fclose(file);
    }
}
