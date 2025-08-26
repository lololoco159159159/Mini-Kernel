#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include "pcb.h"
#include "tcb.h"

#define THREAD_EXEC_TIME_MS 500

// Funções de gerenciamento de processos
void read_input(const char* filename);
void* process_thread_function(void* arg);
void* process_generator_thread_function(void* arg);
void cleanup_resources();

// Variáveis globais dos processos
extern PCB* pcb_list;
extern int num_processes;

#endif
