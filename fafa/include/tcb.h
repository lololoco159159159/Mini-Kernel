#ifndef TCB_H
#define TCB_H

#include "pcb.h"

// Estrutura TCB (Task Control Block)
typedef struct {
    PCB* pcb;
    int thread_index;
} TCB;

// Funções para gerenciar TCB
TCB* create_tcb(PCB* pcb, int thread_index);
void destroy_tcb(TCB* tcb);

#endif
