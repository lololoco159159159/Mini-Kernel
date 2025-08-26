#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "structures.h"

/**
 * Inicializa o sistema de escalonamento
 * @param scheduler_type Tipo de política de escalonamento
 * @param quantum Tempo de quantum para Round Robin (ms)
 */
void init_scheduler(SchedulerType scheduler_type, int quantum);

/**
 * Thread principal do escalonador
 * @param arg Argumentos da thread (não utilizado)
 * @return NULL
 */
void* scheduler_thread(void* arg);

/**
 * Escalonamento FCFS (First Come First Served)
 */
void schedule_fcfs();

/**
 * Escalonamento Round Robin
 */
void schedule_round_robin();

/**
 * Escalonamento por Prioridade Preemptiva
 */
void schedule_priority();

/**
 * Coloca um processo em execução
 * @param pcb Ponteiro para o processo
 */
void set_process_running(PCB* pcb);

/**
 * Para a execução de um processo (preempção ou finalização)
 * @param pcb Ponteiro para o processo
 */
void stop_process_execution(PCB* pcb);

/**
 * Verifica se há processos de maior prioridade na fila
 * @param current_priority Prioridade atual em execução
 * @return Ponteiro para processo de maior prioridade ou NULL
 */
PCB* check_higher_priority_process(int current_priority);

/**
 * Aguarda por um tempo específico (em milissegundos)
 * @param milliseconds Tempo a aguardar
 */
void sleep_ms(int milliseconds);

/**
 * Thread do escalonador para um CPU específico
 * @param arg Ponteiro para o ID do CPU
 * @return NULL
 */
void* scheduler_thread_cpu(void* arg);

/**
 * Gerencia a execução multiprocessador
 * @param policy_names Array com nomes das políticas
 * @param log_msg Buffer para mensagens de log
 */
void handle_multiprocessor_execution(const char* policy_names[], char* log_msg);

/**
 * Obtém o tempo atual em milissegundos desde o início da simulação
 * @return Tempo em milissegundos
 */
long get_current_time_ms();

/**
 * Verifica se todos os processos terminaram
 * @return 1 se todos terminaram, 0 caso contrário
 */
int all_processes_finished();

/**
 * Limpa recursos do escalonador
 */
void cleanup_scheduler();

#ifdef MULTI
/**
 * Thread do escalonador para a segunda CPU (modo multiprocessador)
 * @param arg Argumentos da thread (não utilizado)
 * @return NULL
 */
void* scheduler_thread_cpu2(void* arg);

/**
 * Função principal do escalonador multiprocessador
 * @param arg Argumentos da thread (não utilizado)
 * @return NULL
 */
void* scheduler_thread_function(void* arg);

/**
 * Coloca um processo em execução na CPU 2
 * @param pcb Ponteiro para o processo
 */
void set_process_running_cpu2(PCB* pcb);

/**
 * Para a execução de um processo na CPU 2
 * @param pcb Ponteiro para o processo
 */
void stop_process_execution_cpu2(PCB* pcb);
#endif

#endif // SCHEDULER_H
