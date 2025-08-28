#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <pthread.h>

// Estados dos processos - conforme especificação 2.8
typedef enum {
    READY,                      // Processo pronto para execução
    RUNNING,                    // Processo em execução
    FINISHED                    // Processo finalizado
} ProcessState;

// Políticas de escalonamento - conforme especificação seção 4.1
typedef enum {
    FCFS = 1,                   // First Come First Served
    ROUND_ROBIN = 2,            // Round Robin com quantum fixo
    PRIORITY = 3,               // Prioridade Preemptiva
    CFS = 4                     // Completely Fair Scheduler (Desafio Tópico 8)
} SchedulerType;

// Constantes para Red-Black Tree (CFS)
#define RB_RED   0
#define RB_BLACK 1

// Estrutura BCP - Bloco de Controle de Processo (conforme seção 2.8)
typedef struct PCB {
    // Campos estáticos (definidos na criação)
    int pid;                    // Identificador único do processo (não confundir com getpid())
    int process_len;            // Duração total de execução em milissegundos
    int priority;               // Prioridade (1 = maior, 5 = menor prioridade)
    int num_threads;            // Quantidade de threads que o processo irá utilizar
    int start_time;             // Tempo de chegada em milissegundos (relativo ao início)
    
    // Campos dinâmicos (modificados durante execução)
    int remaining_time;         // Tempo restante de execução (decrementado pelas threads)
    ProcessState state;         // Estado atual: READY, RUNNING ou FINISHED
    volatile int should_preempt; // Flag para indicar preempção
    
    // Campos específicos para CFS (Completely Fair Scheduler) - Desafio Tópico 8
    long long vruntime;         // Virtual runtime - tempo virtual acumulado (fairness)
    int weight;                 // Peso baseado na prioridade nice para cálculo de fairness
    long long start_vruntime;   // vruntime inicial para cálculo de diferenças
    struct PCB* rb_left;        // Ponteiro para filho esquerdo na Red-Black Tree
    struct PCB* rb_right;       // Ponteiro para filho direito na Red-Black Tree
    struct PCB* rb_parent;      // Ponteiro para pai na Red-Black Tree
    int rb_color;               // Cor do nó na RB Tree (0=preto, 1=vermelho)
    
    // Mecanismos de sincronização
    pthread_mutex_t mutex;      // Mutex exclusivo para controlar acesso concorrente
    pthread_cond_t cv;          // Variável de condição para sinalizar threads
    pthread_t *thread_ids;      // Vetor com identificadores das threads do processo
} PCB;

// Estrutura TCB - Bloco de Controle de Thread (conforme seção 2.9)
typedef struct {
    PCB* pcb;                   // Ponteiro para o processo (BCP) ao qual a thread pertence
    int thread_index;           // Índice/posição da thread dentro do processo
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
    PCB* current_process;       // Processo atualmente em execução (CPU 0)
    SchedulerType scheduler_type; // Política de escalonamento
    int quantum;                // Quantum para Round Robin (ms)
    int generator_done;         // Flag para sinalizar fim da criação
    char* log_buffer;           // Buffer para logs
    int log_size;               // Tamanho atual do log
    long start_time_ms;         // Tempo de início da simulação
    
    // Mutexes e condições para sincronização do escalonador
    pthread_mutex_t scheduler_mutex; // Mutex para controle do escalonador
    pthread_cond_t scheduler_cv;     // Condição para sinalização do escalonador
    
#ifdef MULTI
    PCB* current_process_array[2]; // Array para processos nos 2 CPUs (compatível com fafa)
    int num_cpus;                  // Número de CPUs (2 para multiprocessador)
    pthread_mutex_t cpu2_mutex;   // Mutex para controle da CPU 2
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
