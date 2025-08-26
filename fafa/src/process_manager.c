#include "process_manager.h"
#include "scheduler.h"
#include "logger.h"
#include "tcb.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

PCB* pcb_list = NULL;
int num_processes = 0;

void read_input(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Erro ao abrir arquivo %s\n", filename);
        exit(1);
    }
    
    fscanf(file, "%d", &num_processes);
    pcb_list = malloc(num_processes * sizeof(PCB));
    
    for (int i = 0; i < num_processes; i++) {
        PCB* pcb = &pcb_list[i];
        int process_len, priority, num_threads, start_time;
        
        fscanf(file, "%d %d %d %d", 
               &process_len, &priority, 
               &num_threads, &start_time);
        
        initialize_pcb(pcb, i + 1, process_len, priority, num_threads, start_time);
    }
    
    int scheduler_type;
    fscanf(file, "%d", &scheduler_type);
    scheduler->scheduler_type = (SchedulerType)scheduler_type;
    
    fclose(file);
}

void* process_thread_function(void* arg) {
    TCB* tcb = (TCB*)arg;
    PCB* pcb = tcb->pcb;
    
    while (true) {
        pthread_mutex_lock(&pcb->mutex);
        
        // Aguardar até o processo estar executando
        while (pcb->state != RUNNING && pcb->state != FINISHED) {
            pthread_cond_wait(&pcb->cv, &pcb->mutex);
        }
        
        if (pcb->state == FINISHED) {
            pthread_mutex_unlock(&pcb->mutex);
            break;
        }
        
        pthread_mutex_unlock(&pcb->mutex);
        
        // Simular execução por pequenos incrementos
        usleep(50000); // 50ms
        
        pthread_mutex_lock(&pcb->mutex);
        
        // Só decrementar se ainda estiver RUNNING
        if (pcb->state == RUNNING) {
            pcb->remaining_time -= 50; // Decrementar 50ms
            
            if (pcb->remaining_time <= 0) {
                pcb->remaining_time = 0;
                pcb->state = FINISHED;
                pthread_cond_broadcast(&pcb->cv);
            }
        }
        
        pthread_mutex_unlock(&pcb->mutex);
    }
    
    destroy_tcb(tcb);
    return NULL;
}

void* process_generator_thread_function(void* arg) {
    (void)arg; // Suprimir warning
    
    // Criar array de índices ordenado por tempo de chegada
    int* order = malloc(num_processes * sizeof(int));
    for (int i = 0; i < num_processes; i++) {
        order[i] = i;
    }
    
    // Ordenar por tempo de chegada
    for (int i = 0; i < num_processes - 1; i++) {
        for (int j = i + 1; j < num_processes; j++) {
            if (pcb_list[order[i]].start_time > pcb_list[order[j]].start_time) {
                int temp = order[i];
                order[i] = order[j];
                order[j] = temp;
            }
        }
    }
    
    for (int i = 0; i < num_processes; i++) {
        int index = order[i];
        PCB* pcb = &pcb_list[index];
        
        // Aguardar tempo de chegada
        while (get_current_time_ms() < pcb->start_time) {
            usleep(10000);
        }
        
        // Criar threads do processo
        for (int j = 0; j < pcb->num_threads; j++) {
            TCB* tcb = create_tcb(pcb, j);
            pthread_create(&pcb->thread_ids[j], NULL, process_thread_function, tcb);
        }
        
        // Adicionar à fila de prontos
        enqueue_process(scheduler->ready_queue, pcb);
        
        // Sinalizar escalonador com alta prioridade para verificação imediata
        pthread_mutex_lock(&scheduler->scheduler_mutex);
        pthread_cond_broadcast(&scheduler->scheduler_cv); // broadcast para acordar imediatamente
        pthread_mutex_unlock(&scheduler->scheduler_mutex);
    }
    
    free(order);
    
    pthread_mutex_lock(&scheduler->scheduler_mutex);
    scheduler->generator_done = true;
    pthread_cond_signal(&scheduler->scheduler_cv);
    pthread_mutex_unlock(&scheduler->scheduler_mutex);
    
    return NULL;
}

void cleanup_resources() {
    if (pcb_list) {
        for (int i = 0; i < num_processes; i++) {
            PCB* pcb = &pcb_list[i];
            
            // Aguardar threads terminarem
            for (int t = 0; t < pcb->num_threads; t++) {
                pthread_join(pcb->thread_ids[t], NULL);
            }
            
            cleanup_pcb(pcb);
        }
        free(pcb_list);
        pcb_list = NULL;
    }
    
    if (scheduler) {
        destroy_scheduler(scheduler);
        scheduler = NULL;
    }
}
