#define _GNU_SOURCE
#include "scheduler.h"
#include "queue.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

void init_scheduler(SchedulerType scheduler_type, int quantum) {
    system_state.scheduler_type = scheduler_type;
    system_state.quantum = quantum;
    system_state.current_process = NULL;
    system_state.generator_done = 0;
    
    // Inicializa o tempo de início
    struct timeval tv;
    gettimeofday(&tv, NULL);
    system_state.start_time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
#ifdef MULTI
    system_state.current_process_cpu2 = NULL;
    pthread_mutex_init(&system_state.cpu2_mutex, NULL);
#endif
}

void* scheduler_thread(void* arg) {
    (void)arg; // Suprime warning de parâmetro não utilizado
    
    switch (system_state.scheduler_type) {
        case FCFS:
            schedule_fcfs();
            break;
        case ROUND_ROBIN:
            schedule_round_robin();
            break;
        case PRIORITY:
            schedule_priority();
            break;
        default:
            fprintf(stderr, "Erro: tipo de escalonador desconhecido\n");
            break;
    }
    
    log_scheduler_end();
    return NULL;
}

void schedule_fcfs() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue) || 
           system_state.current_process != NULL) {
        
        // Aguarda processo na fila se não há processo em execução
        if (system_state.current_process == NULL) {
            pthread_mutex_lock(&system_state.ready_queue.mutex);
            
            while (is_queue_empty(&system_state.ready_queue) && !system_state.generator_done) {
                pthread_cond_wait(&system_state.ready_queue.cv, &system_state.ready_queue.mutex);
            }
            
            if (!is_queue_empty(&system_state.ready_queue)) {
                PCB* next_process = dequeue_process(&system_state.ready_queue);
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
                
                if (next_process != NULL) {
                    set_process_running(next_process);
                    log_process_start(scheduler_name, next_process->pid);
                }
            } else {
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
            }
        }
        
        // Verifica se o processo atual terminou
        if (system_state.current_process != NULL) {
            pthread_mutex_lock(&system_state.current_process->mutex);
            
            if (system_state.current_process->state == FINISHED) {
                PCB* finished_process = system_state.current_process;
                log_process_finish(scheduler_name, finished_process->pid);
                system_state.current_process = NULL;
                pthread_mutex_unlock(&finished_process->mutex);
            } else {
                pthread_mutex_unlock(&system_state.current_process->mutex);
            }
        }
        
        sleep_ms(100); // Pequena pausa para evitar busy waiting
    }
}

void schedule_round_robin() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue) || 
           system_state.current_process != NULL) {
        
        // Aguarda processo na fila se não há processo em execução
        if (system_state.current_process == NULL) {
            pthread_mutex_lock(&system_state.ready_queue.mutex);
            
            while (is_queue_empty(&system_state.ready_queue) && !system_state.generator_done) {
                pthread_cond_wait(&system_state.ready_queue.cv, &system_state.ready_queue.mutex);
            }
            
            if (!is_queue_empty(&system_state.ready_queue)) {
                PCB* next_process = dequeue_process(&system_state.ready_queue);
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
                
                if (next_process != NULL) {
                    set_process_running(next_process);
                    log_process_start(scheduler_name, next_process->pid);
                }
            } else {
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
            }
        }
        
        // Executa por um quantum
        if (system_state.current_process != NULL) {
            sleep_ms(system_state.quantum);
            
            pthread_mutex_lock(&system_state.current_process->mutex);
            
            if (system_state.current_process->state == FINISHED) {
                PCB* finished_process = system_state.current_process;
                log_process_finish(scheduler_name, finished_process->pid);
                system_state.current_process = NULL;
                pthread_mutex_unlock(&finished_process->mutex);
            } else if (system_state.current_process->remaining_time > 0) {
                // Preempção - volta para a fila
                PCB* preempted_process = system_state.current_process;
                stop_process_execution(preempted_process);
                log_process_preempted(scheduler_name, preempted_process->pid);
                enqueue_process(&system_state.ready_queue, preempted_process);
                system_state.current_process = NULL;
                pthread_mutex_unlock(&preempted_process->mutex);
            } else {
                pthread_mutex_unlock(&system_state.current_process->mutex);
            }
        }
    }
}

void schedule_priority() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue) || 
           system_state.current_process != NULL) {
        
        // Verifica se há processo de maior prioridade
        if (system_state.current_process != NULL) {
            PCB* higher_priority = check_higher_priority_process(system_state.current_process->priority);
            
            if (higher_priority != NULL) {
                // Preempção por prioridade
                pthread_mutex_lock(&system_state.current_process->mutex);
                stop_process_execution(system_state.current_process);
                log_process_preempted(scheduler_name, system_state.current_process->pid);
                enqueue_process(&system_state.ready_queue, system_state.current_process);
                pthread_mutex_unlock(&system_state.current_process->mutex);
                
                remove_process_from_queue(&system_state.ready_queue, higher_priority);
                set_process_running(higher_priority);
                log_process_start(scheduler_name, higher_priority->pid);
                system_state.current_process = higher_priority;
            }
        }
        
        // Aguarda processo na fila se não há processo em execução
        if (system_state.current_process == NULL) {
            pthread_mutex_lock(&system_state.ready_queue.mutex);
            
            while (is_queue_empty(&system_state.ready_queue) && !system_state.generator_done) {
                pthread_cond_wait(&system_state.ready_queue.cv, &system_state.ready_queue.mutex);
            }
            
            if (!is_queue_empty(&system_state.ready_queue)) {
                PCB* next_process = get_highest_priority_process(&system_state.ready_queue);
                if (next_process != NULL) {
                    remove_process_from_queue(&system_state.ready_queue, next_process);
                    pthread_mutex_unlock(&system_state.ready_queue.mutex);
                    
                    set_process_running(next_process);
                    log_process_start(scheduler_name, next_process->pid);
                    system_state.current_process = next_process;
                } else {
                    pthread_mutex_unlock(&system_state.ready_queue.mutex);
                }
            } else {
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
            }
        }
        
        // Verifica se o processo atual terminou
        if (system_state.current_process != NULL) {
            pthread_mutex_lock(&system_state.current_process->mutex);
            
            if (system_state.current_process->state == FINISHED) {
                PCB* finished_process = system_state.current_process;
                log_process_finish(scheduler_name, finished_process->pid);
                system_state.current_process = NULL;
                pthread_mutex_unlock(&finished_process->mutex);
            } else {
                pthread_mutex_unlock(&system_state.current_process->mutex);
            }
        }
        
        sleep_ms(100); // Pequena pausa para verificação de preempção
    }
}

void set_process_running(PCB* pcb) {
    if (pcb == NULL) return;
    
    pthread_mutex_lock(&pcb->mutex);
    pcb->state = RUNNING;
    pthread_cond_broadcast(&pcb->cv); // Acorda todas as threads do processo
    pthread_mutex_unlock(&pcb->mutex);
}

void stop_process_execution(PCB* pcb) {
    if (pcb == NULL) return;
    
    pthread_mutex_lock(&pcb->mutex);
    if (pcb->state == RUNNING) {
        pcb->state = READY;
    }
    pthread_mutex_unlock(&pcb->mutex);
}

PCB* check_higher_priority_process(int current_priority) {
    PCB* highest_priority = get_highest_priority_process(&system_state.ready_queue);
    
    if (highest_priority != NULL && highest_priority->priority < current_priority) {
        return highest_priority;
    }
    
    return NULL;
}

void sleep_ms(int milliseconds) {
    usleep(milliseconds * 1000);
}

long get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long current_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return current_time - system_state.start_time_ms;
}

int all_processes_finished() {
    for (int i = 0; i < system_state.process_count; i++) {
        pthread_mutex_lock(&system_state.pcb_list[i].mutex);
        if (system_state.pcb_list[i].state != FINISHED) {
            pthread_mutex_unlock(&system_state.pcb_list[i].mutex);
            return 0;
        }
        pthread_mutex_unlock(&system_state.pcb_list[i].mutex);
    }
    return 1;
}

void cleanup_scheduler() {
#ifdef MULTI
    pthread_mutex_destroy(&system_state.cpu2_mutex);
#endif
}

#ifdef MULTI
void* scheduler_thread_cpu2(void* arg) {
    (void)arg;
    
    // Implementação simplificada para segunda CPU
    // Similar ao scheduler principal, mas operando independentemente
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue) || 
           system_state.current_process_cpu2 != NULL) {
        
        // Lógica similar ao escalonador principal
        // mas usando current_process_cpu2 e cpu2_mutex
        
        sleep_ms(100);
    }
    
    return NULL;
}

void set_process_running_cpu2(PCB* pcb) {
    if (pcb == NULL) return;
    
    pthread_mutex_lock(&pcb->mutex);
    pcb->state = RUNNING;
    pthread_cond_broadcast(&pcb->cv);
    pthread_mutex_unlock(&pcb->mutex);
}

void stop_process_execution_cpu2(PCB* pcb) {
    if (pcb == NULL) return;
    
    pthread_mutex_lock(&pcb->mutex);
    if (pcb->state == RUNNING) {
        pcb->state = READY;
    }
    pthread_mutex_unlock(&pcb->mutex);
}
#endif
