/**
 * CFS (Completely Fair Scheduler) Header
 * Interface pública do algoritmo CFS
 * Completamente isolado e thread-safe
 */

#ifndef CFS_H
#define CFS_H

#include "structures.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Inicializa o sistema CFS
 */
void cfs_init(void);

/**
 * Adiciona processo à fila do CFS
 * @param process Processo a ser adicionado
 */
void cfs_enqueue_process(PCB* process);

/**
 * Seleciona próximo processo para execução
 * @return Processo com menor vruntime ou NULL se não há processos
 */
PCB* cfs_pick_next(void);

/**
 * Reinsere processo após execução
 * @param process Processo que acabou de executar
 * @param runtime_ns Tempo que o processo executou (em nanossegundos)
 */
void cfs_put_prev_process(PCB* process, uint64_t runtime_ns);

/**
 * Calcula timeslice para um processo
 * @param process Processo para calcular timeslice
 * @return Timeslice em microssegundos
 */
int cfs_get_timeslice(PCB* process);

/**
 * Verifica se há processos na fila CFS
 * @return true se há processos, false caso contrário
 */
bool cfs_has_processes(void);

/**
 * Limpa e finaliza o sistema CFS
 */
void cfs_cleanup(void);

#endif // CFS_H
