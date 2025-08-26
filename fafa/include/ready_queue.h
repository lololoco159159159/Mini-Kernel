#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "pcb.h"
#include <stdbool.h>
#include <pthread.h>

#define MAX_PROCESSES 100

// Estrutura da fila de prontos
typedef struct {
    PCB* processes[MAX_PROCESSES];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
} ReadyQueue;

// Funções da fila de prontos
ReadyQueue* create_ready_queue();
void destroy_ready_queue(ReadyQueue* queue);
void enqueue_process(ReadyQueue* queue, PCB* process);
PCB* dequeue_process(ReadyQueue* queue);
bool is_queue_empty(ReadyQueue* queue);
void remove_process_from_queue(ReadyQueue* queue, PCB* process);
PCB* find_highest_priority_process(ReadyQueue* queue);
PCB* find_highest_priority_process_without_removing(ReadyQueue* queue);
PCB* ready_queue_peek_highest_priority(ReadyQueue* queue);

#endif
