#ifndef QUEUE_H
#define QUEUE_H

#include "structures.h"

/**
 * Inicializa a fila de processos prontos
 * @param queue Ponteiro para a fila a ser inicializada
 */
void init_ready_queue(ReadyQueue* queue);

/**
 * Adiciona um processo na fila de prontos
 * @param queue Ponteiro para a fila
 * @param pcb Ponteiro para o processo a ser adicionado
 */
void enqueue_process(ReadyQueue* queue, PCB* pcb);

/**
 * Remove e retorna o próximo processo da fila (FCFS)
 * @param queue Ponteiro para a fila
 * @return Ponteiro para o processo removido ou NULL se fila vazia
 */
PCB* dequeue_process(ReadyQueue* queue);

/**
 * Remove um processo específico da fila (usado na preempção por prioridade)
 * @param queue Ponteiro para a fila
 * @param pcb Ponteiro para o processo a ser removido
 * @return 1 se removeu com sucesso, 0 caso contrário
 */
int remove_process_from_queue(ReadyQueue* queue, PCB* pcb);

/**
 * Retorna o processo com maior prioridade da fila (sem remover)
 * @param queue Ponteiro para a fila
 * @return Ponteiro para o processo de maior prioridade ou NULL se fila vazia
 */
PCB* get_highest_priority_process(ReadyQueue* queue);

/**
 * Verifica se a fila está vazia
 * @param queue Ponteiro para a fila
 * @return 1 se vazia, 0 caso contrário
 */
int is_queue_empty(ReadyQueue* queue);

/**
 * Retorna o tamanho atual da fila
 * @param queue Ponteiro para a fila
 * @return Número de processos na fila
 */
int get_queue_size(ReadyQueue* queue);

/**
 * Libera a memória alocada para a fila
 * @param queue Ponteiro para a fila
 */
void destroy_ready_queue(ReadyQueue* queue);

/**
 * Exibe o conteúdo da fila para debug (thread-safe)
 * @param queue Ponteiro para a fila
 */
void print_queue_debug(ReadyQueue* queue);

/**
 * Insere um processo na fila mantendo ordem de prioridade
 * @param queue Ponteiro para a fila
 * @param pcb Ponteiro para o processo a ser adicionado
 */
void enqueue_process_by_priority(ReadyQueue* queue, PCB* pcb);

/**
 * Verifica se um processo específico está na fila
 * @param queue Ponteiro para a fila
 * @param pcb Ponteiro para o processo a ser procurado
 * @return 1 se encontrado, 0 caso contrário
 */
int is_process_in_queue(ReadyQueue* queue, PCB* pcb);

/**
 * Remove e retorna o processo de maior prioridade da fila
 * @param queue Ponteiro para a fila
 * @return Ponteiro para o processo de maior prioridade ou NULL se fila vazia
 */
PCB* dequeue_highest_priority_process(ReadyQueue* queue);

#endif // QUEUE_H
