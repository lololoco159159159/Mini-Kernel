#ifndef PCB_H
#define PCB_H

#include <pthread.h>
#include <stdbool.h>

// Estados do processo
typedef enum {
    READY,
    RUNNING,
    FINISHED
} ProcessState;

// Estrutura BCP (Bloco de Controle de Processo)
typedef struct {
    int pid;
    int process_len;
    int remaining_time;
    int priority;
    int num_threads;
    int start_time;
    ProcessState state;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    pthread_t *thread_ids;
} PCB;

// Funções para gerenciar PCB
PCB* create_pcb(int pid, int process_len, int priority, int num_threads, int start_time);
void destroy_pcb(PCB* pcb);
void initialize_pcb(PCB* pcb, int pid, int process_len, int priority, int num_threads, int start_time);
void cleanup_pcb(PCB* pcb);

#endif
