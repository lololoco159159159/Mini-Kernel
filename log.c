#include "log.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// Variável global para o mutex do log
pthread_mutex_t log_mutex;

// Buffer separado para mensagens essenciais (apenas para arquivo final)
static char* essential_log_buffer = NULL;
static int essential_log_size = 0;

// Função para verificar se uma mensagem é essencial para o arquivo final
int is_essential_message(const char* message) {
    // Mensagens que devem aparecer no arquivo final
    return (strstr(message, "[FCFS] Executando processo PID") != NULL ||
            strstr(message, "[FCFS] Processo PID") != NULL ||
            strstr(message, "[RR] Executando processo PID") != NULL ||
            strstr(message, "[RR] Processo PID") != NULL ||
            strstr(message, "[PRIORITY] Executando processo PID") != NULL ||
            strstr(message, "[PRIORITY] Processo PID") != NULL ||
            strstr(message, "Escalonador terminou execução de todos processos") != NULL);
}

// Função para adicionar mensagem ao buffer essencial
void add_essential_log_message(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    va_list args;
    va_start(args, format);
    
    // Calcula o tamanho necessário para a nova mensagem
    int needed_size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    // Aloca buffer essencial se necessário
    if (essential_log_buffer == NULL) {
        essential_log_buffer = malloc(MAX_LOG_SIZE);
        if (essential_log_buffer == NULL) {
            pthread_mutex_unlock(&log_mutex);
            return;
        }
        essential_log_buffer[0] = '\0';
        essential_log_size = 0;
    }
    
    // Verifica se há espaço suficiente no buffer essencial
    if (essential_log_size + needed_size + 1 >= MAX_LOG_SIZE) {
        int new_size = MAX_LOG_SIZE * 2;
        char* new_buffer = realloc(essential_log_buffer, new_size);
        if (new_buffer == NULL) {
            pthread_mutex_unlock(&log_mutex);
            return;
        }
        essential_log_buffer = new_buffer;
    }
    
    // Adiciona a mensagem ao buffer essencial
    va_start(args, format);
    int written = vsnprintf(essential_log_buffer + essential_log_size, 
                           MAX_LOG_SIZE - essential_log_size, format, args);
    va_end(args);
    
    if (written > 0) {
        essential_log_size += written;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

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
    add_essential_log_message("[%s] Executando processo PID %d\n", scheduler_name, pid);
}

void log_process_start_rr(int pid, int quantum) {
    add_essential_log_message("[RR] Executando processo PID %d com quantum %dms\n", pid, quantum);
}

void log_process_start_priority(int pid, int priority) {
    add_essential_log_message("[PRIORITY] Executando processo PID %d prioridade %d \n", pid, priority);
}

// Funções para multiprocessador
void log_process_start_cpu(const char* scheduler_name, int pid, int cpu_id) {
    add_essential_log_message("[%s] Executando processo PID %d // processador %d\n", scheduler_name, pid, cpu_id);
}

void log_process_start_rr_cpu(int pid, int quantum, int cpu_id) {
    add_essential_log_message("[RR] Executando processo PID %d com quantum %dms // processador %d\n", pid, quantum, cpu_id);
}

void log_process_start_priority_cpu(int pid, int priority, int cpu_id) {
    add_essential_log_message("[PRIORITY] Executando processo PID %d prioridade %d // processador %d\n", pid, priority, cpu_id);
}

void log_process_finish_priority(int pid) {
    add_essential_log_message("[RRIORITY] Processo PID %d finalizado\n", pid);
}

void log_process_finish(const char* scheduler_name, int pid) {
    add_essential_log_message("[%s] Processo PID %d finalizado\n", scheduler_name, pid);
}

void log_process_preempted(const char* scheduler_name, int pid) {
    add_log_message("[%s] Processo PID %d preemptado\n", scheduler_name, pid);
}

void log_quantum_expired(const char* scheduler_name, int pid) {
    add_log_message("[%s] Quantum do processo PID %d expirado\n", scheduler_name, pid);
}

void log_scheduler_end() {
    add_essential_log_message("Escalonador terminou execução de todos processos\n");
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
    
    // Salva o log essencial no arquivo (em vez do log completo)
    if (essential_log_buffer != NULL && essential_log_buffer[0] != '\0') {
        size_t essential_size = strlen(essential_log_buffer);
        size_t written = fwrite(essential_log_buffer, 1, essential_size, file);
        if (written != essential_size) {
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
