#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Variável global para o mutex do log
pthread_mutex_t log_mutex;

void init_log_system() {
    pthread_mutex_init(&log_mutex, NULL);
    
    system_state.log_buffer = malloc(MAX_LOG_SIZE);
    if (system_state.log_buffer == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memória para buffer de log\n");
        exit(1);
    }
    
    system_state.log_buffer[0] = '\0';
    system_state.log_size = 0;
}

void add_log_message(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    va_list args;
    va_start(args, format);
    
    // Calcula o tamanho necessário para a mensagem
    int needed_size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    // Verifica se há espaço suficiente no buffer
    if (system_state.log_size + needed_size + 1 >= MAX_LOG_SIZE) {
        // Realoca o buffer se necessário
        int new_size = MAX_LOG_SIZE * 2;
        char* new_buffer = realloc(system_state.log_buffer, new_size);
        if (new_buffer == NULL) {
            fprintf(stderr, "Erro: falha ao realocar buffer de log\n");
            pthread_mutex_unlock(&log_mutex);
            return;
        }
        system_state.log_buffer = new_buffer;
    }
    
    // Adiciona a mensagem ao buffer
    va_start(args, format);
    int written = vsnprintf(system_state.log_buffer + system_state.log_size, 
                           MAX_LOG_SIZE - system_state.log_size, format, args);
    va_end(args);
    
    if (written > 0) {
        system_state.log_size += written;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void log_process_start(const char* scheduler_name, int pid) {
    add_log_message("[%s] Executando processo PID %d\n", scheduler_name, pid);
}

void log_process_finish(const char* scheduler_name, int pid) {
    add_log_message("[%s] Processo PID %d finalizado\n", scheduler_name, pid);
}

void log_process_preempted(const char* scheduler_name, int pid) {
    add_log_message("[%s] Processo PID %d preemptado\n", scheduler_name, pid);
}

void log_scheduler_end() {
    add_log_message("Escalonador terminou execução de todos processos\n");
}

int save_log_to_file(const char* filename) {
    pthread_mutex_lock(&log_mutex);
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "Erro: não foi possível abrir arquivo %s para escrita\n", filename);
        pthread_mutex_unlock(&log_mutex);
        return 0;
    }
    
    if (system_state.log_buffer != NULL && system_state.log_size > 0) {
        fwrite(system_state.log_buffer, 1, system_state.log_size, file);
    }
    
    fclose(file);
    pthread_mutex_unlock(&log_mutex);
    
    return 1;
}

void cleanup_log_system() {
    pthread_mutex_lock(&log_mutex);
    
    if (system_state.log_buffer != NULL) {
        free(system_state.log_buffer);
        system_state.log_buffer = NULL;
    }
    
    system_state.log_size = 0;
    
    pthread_mutex_unlock(&log_mutex);
    pthread_mutex_destroy(&log_mutex);
}

const char* get_scheduler_name(SchedulerType type) {
    switch (type) {
        case FCFS:
            return "FCFS";
        case ROUND_ROBIN:
            return "RR";
        case PRIORITY:
            return "PRIORIDADE";
        default:
            return "DESCONHECIDO";
    }
}
