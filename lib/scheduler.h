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
void schedule_priority();

/**
 * Implementação do algoritmo CFS (Completely Fair Scheduler)
 * Utiliza Red-Black Tree para escalonamento justo baseado em vruntime
 */
void schedule_cfs();

/**
 * Configura processo para estado RUNNING
 * @param pcb PCB do processo
 */
void configure_process_state(PCB* pcb);

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
/**
 * Executa escalonamento para sistemas multiprocessadores
 * @param scheduler_names Array com nomes das políticas de escalonamento
 * @param message_buffer Buffer para mensagens de log
 */
void execute_multicore_scheduling(const char* scheduler_names[], char* message_buffer);

/**
 * Obtém o tempo atual em milissegundos desde o início da simulação
 * @return Tempo em milissegundos
 */
long calculate_elapsed_time();

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
void* multicore_scheduler_main(void* arg);

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

/**
 * ========== FUNÇÕES CFS (Completely Fair Scheduler) ==========
 * Implementação do Desafio Tópico 8 com Red-Black Tree
 */

/**
 * Implementação do algoritmo CFS
 */
void schedule_cfs();

/**
 * Funções da Red-Black Tree para CFS
 */
void cfs_rb_insert(PCB* process);
void cfs_rb_remove(PCB* process);
PCB* cfs_rb_leftmost(PCB* node);
PCB* rb_rotate_left(PCB* node);
PCB* rb_rotate_right(PCB* node);
void rb_insert_fixup(PCB* node);

/**
 * Funções de gerenciamento CFS
 */
PCB* cfs_select_next(void);
int cfs_calculate_timeslice(PCB* process);
void cfs_update_vruntime(PCB* process, int executed_time_ms);
void cfs_init_process(PCB* process);
int cfs_is_empty();

#endif // SCHEDULER_H
