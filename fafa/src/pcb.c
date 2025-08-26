#include "pcb.h"
#include <stdlib.h>
#include <string.h>

PCB* create_pcb(int pid, int process_len, int priority, int num_threads, int start_time) {
    PCB* pcb = malloc(sizeof(PCB));
    if (!pcb) return NULL;
    
    initialize_pcb(pcb, pid, process_len, priority, num_threads, start_time);
    return pcb;
}

void initialize_pcb(PCB* pcb, int pid, int process_len, int priority, int num_threads, int start_time) {
    pcb->pid = pid;
    pcb->process_len = process_len;
    pcb->remaining_time = process_len;
    pcb->priority = priority;
    pcb->num_threads = num_threads;
    pcb->start_time = start_time;
    pcb->state = READY;
    
    pthread_mutex_init(&pcb->mutex, NULL);
    pthread_cond_init(&pcb->cv, NULL);
    pcb->thread_ids = malloc(num_threads * sizeof(pthread_t));
}

void cleanup_pcb(PCB* pcb) {
    if (!pcb) return;
    
    pthread_mutex_destroy(&pcb->mutex);
    pthread_cond_destroy(&pcb->cv);
    
    if (pcb->thread_ids) {
        free(pcb->thread_ids);
        pcb->thread_ids = NULL;
    }
}

void destroy_pcb(PCB* pcb) {
    if (!pcb) return;
    
    cleanup_pcb(pcb);
    free(pcb);
}
