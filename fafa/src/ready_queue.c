#include "ready_queue.h"
#include <stdlib.h>

ReadyQueue* create_ready_queue() {
    ReadyQueue* queue = malloc(sizeof(ReadyQueue));
    if (!queue) return NULL;
    
    queue->front = 0;
    queue->rear = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    return queue;
}

void destroy_ready_queue(ReadyQueue* queue) {
    if (!queue) return;
    
    pthread_mutex_destroy(&queue->mutex);
    free(queue);
}

void enqueue_process(ReadyQueue* queue, PCB* process) {
    if (!queue || !process) return;
    
    pthread_mutex_lock(&queue->mutex);
    if (queue->count < MAX_PROCESSES) {
        queue->processes[queue->rear] = process;
        queue->rear = (queue->rear + 1) % MAX_PROCESSES;
        queue->count++;
    }
    pthread_mutex_unlock(&queue->mutex);
}

PCB* dequeue_process(ReadyQueue* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    PCB* process = NULL;
    if (queue->count > 0) {
        process = queue->processes[queue->front];
        queue->front = (queue->front + 1) % MAX_PROCESSES;
        queue->count--;
    }
    pthread_mutex_unlock(&queue->mutex);
    return process;
}

bool is_queue_empty(ReadyQueue* queue) {
    if (!queue) return true;
    
    pthread_mutex_lock(&queue->mutex);
    bool empty = (queue->count == 0);
    pthread_mutex_unlock(&queue->mutex);
    return empty;
}

void remove_process_from_queue(ReadyQueue* queue, PCB* process) {
    if (!queue || !process) return;
    
    pthread_mutex_lock(&queue->mutex);
    for (int i = 0; i < queue->count; i++) {
        int index = (queue->front + i) % MAX_PROCESSES;
        if (queue->processes[index] == process) {
            // Remover processo movendo todos os posteriores
            for (int j = i; j < queue->count - 1; j++) {
                int curr_index = (queue->front + j) % MAX_PROCESSES;
                int next_index = (queue->front + j + 1) % MAX_PROCESSES;
                queue->processes[curr_index] = queue->processes[next_index];
            }
            queue->rear = (queue->rear - 1 + MAX_PROCESSES) % MAX_PROCESSES;
            queue->count--;
            break;
        }
    }
    pthread_mutex_unlock(&queue->mutex);
}

PCB* ready_queue_peek_highest_priority(ReadyQueue* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    PCB* highest = NULL;
    
    for (int i = 0; i < queue->count; i++) {
        int index = (queue->front + i) % MAX_PROCESSES;
        PCB* current = queue->processes[index];
        if (highest == NULL || current->priority < highest->priority) {
            highest = current;
        }
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return highest;
}

PCB* find_highest_priority_process_without_removing(ReadyQueue* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    PCB* highest = NULL;
    
    for (int i = 0; i < queue->count; i++) {
        int index = (queue->front + i) % MAX_PROCESSES;
        PCB* current = queue->processes[index];
        if (highest == NULL || current->priority < highest->priority) {
            highest = current;
        }
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return highest;
}

PCB* find_highest_priority_process(ReadyQueue* queue) {
    if (!queue) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    PCB* highest = NULL;
    int highest_index = -1;
    
    for (int i = 0; i < queue->count; i++) {
        int index = (queue->front + i) % MAX_PROCESSES;
        PCB* current = queue->processes[index];
        if (highest == NULL || current->priority < highest->priority) {
            highest = current;
            highest_index = index;
        }
    }
    
    if (highest != NULL) {
        // Remover processo da fila
        for (int i = highest_index; i != queue->rear; i = (i + 1) % MAX_PROCESSES) {
            int next = (i + 1) % MAX_PROCESSES;
            queue->processes[i] = queue->processes[next];
        }
        queue->rear = (queue->rear - 1 + MAX_PROCESSES) % MAX_PROCESSES;
        queue->count--;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return highest;
}
