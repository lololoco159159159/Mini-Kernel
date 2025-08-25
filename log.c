#include "log.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// Variável global para o mutex do log
pthread_mutex_t log_mutex;

void init_log_system() {
    pthread_mutex_init(&log_mutex, NULL);
    
    // Aloca buffer inicial com tamanho generoso
    system_state.log_buffer = malloc(MAX_LOG_SIZE);
    if (system_state.log_buffer == NULL) {
        fprintf(stderr, "ERRO FATAL: Falha ao alocar memória para buffer de log\n");
        exit(1);
    }
    
    system_state.log_buffer[0] = '\0';
    system_state.log_size = 0;
    
    // Log inicial do sistema
    add_log_message("=== INICIO DA SIMULACAO DO MINI-KERNEL ===\n");
    add_log_message("Sistema de log inicializado - Buffer: %d bytes\n", MAX_LOG_SIZE);
}

void add_log_message(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    va_list args;
    va_start(args, format);
    
    // Calcula o tamanho necessário para a nova mensagem
    int needed_size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    // Verifica se há espaço suficiente no buffer
    if (system_state.log_size + needed_size + 1 >= MAX_LOG_SIZE) {
        // Duplica o tamanho do buffer se necessário
        int new_size = MAX_LOG_SIZE * 2;
        char* new_buffer = realloc(system_state.log_buffer, new_size);
        if (new_buffer == NULL) {
            // Se falhar, tenta salvar o que tem e continua
            fprintf(stderr, "AVISO: Buffer de log cheio - algumas mensagens podem ser perdidas\n");
            pthread_mutex_unlock(&log_mutex);
            return;
        }
        system_state.log_buffer = new_buffer;
    }
    
    // Adiciona a mensagem ao buffer global
    va_start(args, format);
    int written = vsnprintf(system_state.log_buffer + system_state.log_size, 
                           MAX_LOG_SIZE - system_state.log_size, format, args);
    va_end(args);
    
    if (written > 0) {
        system_state.log_size += written;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void log_system_start() {
    add_log_message("=== SISTEMA INICIADO ===\n");
    add_log_message("Escalonador: %s\n", get_scheduler_name(system_state.scheduler_type));
    add_log_message("Numero de processos: %d\n", system_state.process_count);
    add_log_message("Quantum (para RR): %d ms\n", system_state.quantum);
}

void log_process_created(int pid, int num_threads) {
    add_log_message("Processo PID %d criado com %d threads\n", pid, num_threads);
}

void log_process_start(const char* scheduler_name, int pid) {
    add_log_message("[%s] Executando processo PID %d\n", scheduler_name, pid);
}

void log_process_start_rr(int pid, int quantum) {
    add_log_message("[RR] Executando processo PID %d com quantum %dms\n", pid, quantum);
}

void log_process_finish(const char* scheduler_name, int pid) {
    add_log_message("[%s] Processo PID %d finalizado\n", scheduler_name, pid);
}

void log_process_preempted(const char* scheduler_name, int pid) {
    add_log_message("[%s] Processo PID %d preemptado\n", scheduler_name, pid);
}

void log_quantum_expired(const char* scheduler_name, int pid) {
    add_log_message("[%s] Quantum do processo PID %d expirado\n", scheduler_name, pid);
}

void log_scheduler_end() {
    add_log_message("Escalonador terminou execução de todos processos\n");
}

void add_log_with_timestamp(const char* message) {
    // Verifica se o sistema está inicializado
    if (system_state.start_time_ms == 0) {
        add_log_message("[0 ms] %s", message);
    } else {
        long current_time = get_current_time_ms();
        add_log_message("[%ld ms] %s", current_time, message);
    }
}

int save_log_to_file(const char* filename) {
    // Faz o lock para escrever no arquivo
    pthread_mutex_lock(&log_mutex);
    
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        fprintf(stderr, "ERRO: Nao foi possivel criar arquivo de log: %s\n", filename);
        pthread_mutex_unlock(&log_mutex);
        return 0;
    }
    
    if (system_state.log_buffer != NULL && system_state.log_size > 0) {
        size_t written = fwrite(system_state.log_buffer, 1, system_state.log_size, file);
        if (written != (size_t)system_state.log_size) {
            fprintf(stderr, "AVISO: Nem todo o log foi escrito no arquivo\n");
        }
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
