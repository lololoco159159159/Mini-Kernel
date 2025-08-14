#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <pthread.h>

// Estados dos processos
typedef enum {
    READY,
    RUNNING,
    FINISHED
} ProcessState;

// Políticas de escalonamento
typedef enum {
    FCFS = 1,
    ROUND_ROBIN = 2,
    PRIORITY = 3
} SchedulerType;

// Estrutura BCP - Bloco de Controle de Processo
typedef struct {
    int pid;                    // Identificador único do processo
    int process_len;            // Duração total de execução (ms)
    int remaining_time;         // Tempo restante de execução
    int priority;               // Prioridade (1 = maior, 5 = menor)
    int num_threads;            // Quantidade de threads do processo
    int start_time;             // Tempo de chegada (ms)
    
    ProcessState state;         // Estado atual do processo
    pthread_mutex_t mutex;      // Mutex para controle de acesso
    pthread_cond_t cv;          // Variável de condição
    pthread_t *thread_ids;      // Identificadores das threads
} PCB;

// Estrutura TCB - Bloco de Controle de Thread
typedef struct {
    PCB* pcb;                   // Ponteiro para o processo pai
    int thread_index;           // Índice da thread no processo
} TCB;

// Estrutura da fila de prontos
typedef struct QueueNode {
    PCB* pcb;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
} ReadyQueue;

// Estrutura global do sistema
typedef struct {
    PCB* pcb_list;              // Lista de todos os processos
    int process_count;          // Número total de processos
    ReadyQueue ready_queue;     // Fila de processos prontos
    PCB* current_process;       // Processo atualmente em execução
    SchedulerType scheduler_type; // Política de escalonamento
    int quantum;                // Quantum para Round Robin (ms)
    int generator_done;         // Flag para sinalizar fim da criação
    char* log_buffer;           // Buffer para logs
    int log_size;               // Tamanho atual do log
    long start_time_ms;         // Tempo de início da simulação
    
#ifdef MULTI
    PCB* current_process_cpu2;  // Processo em execução na CPU 2
    pthread_mutex_t cpu2_mutex; // Mutex para controle da CPU 2
#endif
} SystemState;

// Variáveis globais
extern SystemState system_state;
extern pthread_mutex_t log_mutex;

// Constantes
#define MAX_LOG_SIZE 10000
#define THREAD_EXECUTION_TIME 500  // 500ms por quantum de thread
#define MAX_PROCESSES 100

#endif // STRUCTURES_H
