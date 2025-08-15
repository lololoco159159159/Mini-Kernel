#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

/*
 * =============================================================================
 * IMPLEMENTAÇÃO DA FILA DE PROCESSOS PRONTOS (READY QUEUE)
 * =============================================================================
 * 
 * Esta implementação usa uma lista encadeada simples com proteção thread-safe
 * através de mutexes. A fila suporta:
 * 
 * - Inserção no final (FIFO para FCFS)
 * - Remoção no início (FIFO para FCFS)  
 * - Remoção de elemento específico (para preempção)
 * - Busca por maior prioridade (para escalonamento por prioridade)
 * - Inserção ordenada por prioridade
 * - Operações thread-safe com mutex
 * 
 * Estrutura: front -> [PCB1] -> [PCB2] -> [PCB3] -> NULL <- rear
 * =============================================================================
 */

void init_ready_queue(ReadyQueue* queue) {
    if (queue == NULL) return;
    
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;
    
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cv, NULL);
}

void enqueue_process(ReadyQueue* queue, PCB* pcb) {
    if (queue == NULL || pcb == NULL) return;
    
    QueueNode* new_node = malloc(sizeof(QueueNode));
    if (new_node == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memória para nó da fila\n");
        return;
    }
    
    new_node->pcb = pcb;
    new_node->next = NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->rear == NULL) {
        // Fila vazia
        queue->front = queue->rear = new_node;
    } else {
        // Adiciona no final
        queue->rear->next = new_node;
        queue->rear = new_node;
    }
    
    queue->size++;
    
    // Sinaliza que há processos na fila
    pthread_cond_signal(&queue->cv);
    
    pthread_mutex_unlock(&queue->mutex);
}

PCB* dequeue_process(ReadyQueue* queue) {
    if (queue == NULL) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->front == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    QueueNode* node_to_remove = queue->front;
    PCB* pcb = node_to_remove->pcb;
    
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    
    queue->size--;
    free(node_to_remove);
    
    pthread_mutex_unlock(&queue->mutex);
    
    return pcb;
}

int remove_process_from_queue(ReadyQueue* queue, PCB* pcb) {
    if (queue == NULL || pcb == NULL) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    
    QueueNode* current = queue->front;
    QueueNode* prev = NULL;
    
    // Procura o processo na fila
    while (current != NULL && current->pcb != pcb) {
        prev = current;
        current = current->next;
    }
    
    if (current == NULL) {
        // Processo não encontrado
        pthread_mutex_unlock(&queue->mutex);
        return 0;
    }
    
    // Remove o nó
    if (prev == NULL) {
        // É o primeiro nó
        queue->front = current->next;
        if (queue->front == NULL) {
            queue->rear = NULL;
        }
    } else {
        prev->next = current->next;
        if (current == queue->rear) {
            queue->rear = prev;
        }
    }
    
    queue->size--;
    free(current);
    
    pthread_mutex_unlock(&queue->mutex);
    
    return 1;
}

PCB* get_highest_priority_process(ReadyQueue* queue) {
    if (queue == NULL) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->front == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    QueueNode* current = queue->front;
    PCB* highest_priority = current->pcb;
    
    // Procura o processo com maior prioridade (menor número)
    while (current != NULL) {
        if (current->pcb->priority < highest_priority->priority) {
            highest_priority = current->pcb;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    return highest_priority;
}

int is_queue_empty(ReadyQueue* queue) {
    if (queue == NULL) return 1;
    
    pthread_mutex_lock(&queue->mutex);
    int empty = (queue->size == 0);
    pthread_mutex_unlock(&queue->mutex);
    
    return empty;
}

int get_queue_size(ReadyQueue* queue) {
    if (queue == NULL) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    int size = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}

void destroy_ready_queue(ReadyQueue* queue) {
    if (queue == NULL) return;
    
    pthread_mutex_lock(&queue->mutex);
    
    QueueNode* current = queue->front;
    while (current != NULL) {
        QueueNode* next = current->next;
        free(current);
        current = next;
    }
    
    queue->front = queue->rear = NULL;
    queue->size = 0;
    
    pthread_mutex_unlock(&queue->mutex);
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cv);
}

void print_queue_debug(ReadyQueue* queue) {
    if (queue == NULL) {
        printf("DEBUG: Queue é NULL\n");
        return;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    printf("DEBUG: Fila de prontos (tamanho=%d): ", queue->size);
    
    QueueNode* current = queue->front;
    while (current != NULL) {
        printf("PID%d(P%d) ", current->pcb->pid, current->pcb->priority);
        current = current->next;
    }
    printf("\n");
    
    pthread_mutex_unlock(&queue->mutex);
}

void enqueue_process_by_priority(ReadyQueue* queue, PCB* pcb) {
    if (queue == NULL || pcb == NULL) return;
    
    QueueNode* new_node = malloc(sizeof(QueueNode));
    if (new_node == NULL) {
        fprintf(stderr, "Erro: falha ao alocar memória para nó da fila\n");
        return;
    }
    
    new_node->pcb = pcb;
    new_node->next = NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    // Se fila vazia ou novo processo tem maior prioridade que o primeiro
    if (queue->front == NULL || pcb->priority < queue->front->pcb->priority) {
        new_node->next = queue->front;
        queue->front = new_node;
        
        if (queue->rear == NULL) {
            queue->rear = new_node;
        }
    } else {
        // Procura posição correta para inserir mantendo ordem de prioridade
        QueueNode* current = queue->front;
        QueueNode* prev = NULL;
        
        while (current != NULL && current->pcb->priority <= pcb->priority) {
            prev = current;
            current = current->next;
        }
        
        // Insere entre prev e current
        new_node->next = current;
        prev->next = new_node;
        
        if (current == NULL) {
            queue->rear = new_node;
        }
    }
    
    queue->size++;
    
    // Sinaliza que há processos na fila
    pthread_cond_signal(&queue->cv);
    
    pthread_mutex_unlock(&queue->mutex);
}

int is_process_in_queue(ReadyQueue* queue, PCB* pcb) {
    if (queue == NULL || pcb == NULL) return 0;
    
    pthread_mutex_lock(&queue->mutex);
    
    QueueNode* current = queue->front;
    while (current != NULL) {
        if (current->pcb == pcb) {
            pthread_mutex_unlock(&queue->mutex);
            return 1;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

PCB* dequeue_highest_priority_process(ReadyQueue* queue) {
    if (queue == NULL) return NULL;
    
    pthread_mutex_lock(&queue->mutex);
    
    if (queue->front == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    // Procura processo de maior prioridade
    QueueNode* current = queue->front;
    QueueNode* prev = NULL;
    QueueNode* highest_priority_node = current;
    QueueNode* highest_priority_prev = NULL;
    
    while (current != NULL) {
        if (current->pcb->priority < highest_priority_node->pcb->priority) {
            highest_priority_node = current;
            highest_priority_prev = prev;
        }
        prev = current;
        current = current->next;
    }
    
    // Remove o nó de maior prioridade
    PCB* result = highest_priority_node->pcb;
    
    if (highest_priority_prev == NULL) {
        // É o primeiro nó
        queue->front = highest_priority_node->next;
        if (queue->front == NULL) {
            queue->rear = NULL;
        }
    } else {
        highest_priority_prev->next = highest_priority_node->next;
        if (highest_priority_node == queue->rear) {
            queue->rear = highest_priority_prev;
        }
    }
    
    queue->size--;
    free(highest_priority_node);
    
    pthread_mutex_unlock(&queue->mutex);
    
    return result;
}
