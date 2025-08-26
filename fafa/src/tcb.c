#include "tcb.h"
#include <stdlib.h>

TCB* create_tcb(PCB* pcb, int thread_index) {
    TCB* tcb = malloc(sizeof(TCB));
    if (!tcb) return NULL;
    
    tcb->pcb = pcb;
    tcb->thread_index = thread_index;
    
    return tcb;
}

void destroy_tcb(TCB* tcb) {
    if (!tcb) return;
    free(tcb);
}
