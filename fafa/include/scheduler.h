#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "pcb.h"
#include "ready_queue.h"
#include <pthread.h>
#include <stdbool.h>

#define MAX_LOG_SIZE 10000
#define QUANTUM_MS 500

// Políticas de escalonamento
typedef enum {
    FCFS = 1,
    RR = 2,
    PRIORITY = 3
} SchedulerType;

// Estrutura para o escalonador
typedef struct {
    ReadyQueue* ready_queue;
    SchedulerType scheduler_type;
    int num_cpus;
    PCB* current_process[2]; // Para até 2 CPUs
    bool generator_done;
    pthread_cond_t scheduler_cv;
    pthread_mutex_t scheduler_mutex;
} Scheduler;

// Funções do escalonador
Scheduler* create_scheduler(int num_cpus, SchedulerType type);
void destroy_scheduler(Scheduler* scheduler);
void initialize_scheduler(int num_cpus);
void* scheduler_thread_function(void* arg);
void handle_monoprocessor_execution(PCB* process, const char* policy_names[], char* log_msg);
void handle_multiprocessor_execution(const char* policy_names[], char* log_msg);

// Variáveis globais do escalonador
extern Scheduler* scheduler;

#endif
