#define _GNU_SOURCE
#include "scheduler.h"
#include "queue.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

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
#ifdef MULTI
    int cpu_id = (arg != NULL) ? *((int*)arg) : 0; // CPU 0 por padrão
#else
    (void)arg; // Suprime warning de parâmetro não utilizado  
#endif
    
    while (1) {
        // Verifica se deve terminar
        if (system_state.generator_done && is_queue_empty(&system_state.ready_queue)) {
            break;
        }
        
        switch (system_state.scheduler_type) {
            case FCFS:
#ifdef MULTI
                schedule_fcfs_cpu(cpu_id);
#else
                schedule_fcfs();
#endif
                break;
            case ROUND_ROBIN:
#ifdef MULTI
                schedule_round_robin_cpu(cpu_id);
#else
                schedule_round_robin();
#endif
                break;
            case PRIORITY:
#ifdef MULTI
                schedule_priority_cpu(cpu_id);  
#else
                schedule_priority();
#endif
                break;
            default:
                fprintf(stderr, "Erro: tipo de escalonador desconhecido\n");
                break;
        }
        
        // Pequena pausa
        usleep(10000); // 10ms
    }
    
    log_scheduler_end();
    return NULL;
}

void schedule_fcfs() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador FCFS iniciado\n");
    
    // Loop principal simplificado: enquanto houver processos prontos ou ainda não criados
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        
        iteration_count++;
        
        // Timeout de segurança
        if (iteration_count > 100) {
            break;
        }
        
        // FCFS: pegar primeiro da fila e executar até terminar
        if (!is_queue_empty(&system_state.ready_queue)) {
            PCB* process = dequeue_process(&system_state.ready_queue);
            if (process != NULL) {
                // add_log_message("Executando processo PID %d por %dms\n", process->pid, process->process_len);
                
                // Registrar eventos no log
                log_process_start(scheduler_name, process->pid);
                
                // Mudar estado para RUNNING e acordar threads do processo
                set_process_running(process);
                
                // Aguardar termino completo (FCFS não preempta)
                pthread_mutex_lock(&process->mutex);
                while (process->state != FINISHED) {
                    pthread_mutex_unlock(&process->mutex);
                    usleep(10000); // 10ms de pausa
                    pthread_mutex_lock(&process->mutex);
                }
                pthread_mutex_unlock(&process->mutex);
                
                // Registrar finalização no log
                log_process_finish(scheduler_name, process->pid);
                // add_log_message("Processo PID %d finalizado\n", process->pid);
            }
        }
        
        // Pequena pausa para não sobrecarregar CPU
        usleep(10000); // 10ms
    }
    
    add_log_message("Escalonador FCFS finalizado\n");
}

void schedule_round_robin() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador Round Robin iniciado (quantum: %dms)\n", system_state.quantum);
    
    // Round Robin que implementa quantum corretamente
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        iteration_count++;
        
        if (iteration_count > 100) {
            break;
        }
        
        if (!is_queue_empty(&system_state.ready_queue)) {
            PCB* process = dequeue_process(&system_state.ready_queue);
            if (process != NULL) {
                // add_log_message("RR: Executando processo PID %d\n", process->pid);
                
                // Log específico para RR
                log_process_start_rr(process->pid, system_state.quantum);
                
                // Verificar quanto tempo o processo ainda precisa
                pthread_mutex_lock(&process->mutex);
                int remaining_time = process->remaining_time;
                
                if (remaining_time <= system_state.quantum) {
                    // Processo termina neste quantum
                    process->state = RUNNING;
                    pthread_cond_broadcast(&process->cv);
                    pthread_mutex_unlock(&process->mutex);
                    
                    // Aguardar o processo terminar
                    pthread_mutex_lock(&process->mutex);
                    while (process->state != FINISHED) {
                        pthread_mutex_unlock(&process->mutex);
                        usleep(10000); // 10ms
                        pthread_mutex_lock(&process->mutex);
                    }
                    pthread_mutex_unlock(&process->mutex);
                    
                    log_process_finish(scheduler_name, process->pid);
                    // add_log_message("RR: Processo PID %d terminou\n", process->pid);
                } else {
                    // Processo usa o quantum mas não termina
                    process->remaining_time -= system_state.quantum;
                    pthread_mutex_unlock(&process->mutex);
                    
                    // Recolocar na fila para próxima execução
                    enqueue_process(&system_state.ready_queue, process);
                    // add_log_message("RR: Processo PID %d usou quantum, restam %dms - recolocado na fila\n", 
                    //                process->pid, process->remaining_time);
                }
            }
        }
        
        usleep(10000); // 10ms
    }
    
    add_log_message("Escalonador Round Robin finalizado\n");
}

void schedule_priority() {
    add_log_message("Escalonador por Prioridade iniciado\n");
    
    // Executa até que não há mais processos para gerar ou executar
    do {
        // Processa apenas se há processos aguardando execução
        if (is_queue_empty(&system_state.ready_queue)) {
            usleep(10000);
            continue;
        }
        
        // Obtém processo com prioridade mais alta da fila
        PCB* selected_process = get_highest_priority_process(&system_state.ready_queue);
        if (selected_process == NULL) {
            usleep(10000);
            continue;
        }
        
        // Retira processo da fila de prontos
        remove_process_from_queue(&system_state.ready_queue, selected_process);
        
        // Inicia execução do processo selecionado
        log_process_start_priority(selected_process->pid, selected_process->priority);
        set_process_running(selected_process);
        
        // Controla execução com possibilidade de preempção
        int keep_running = 1;
        do {
            // Verifica estado atual do processo
            pthread_mutex_lock(&selected_process->mutex);
            int time_left = selected_process->remaining_time;
            ProcessState current_state = selected_process->state;
            
            // Finaliza se processo completou execução
            if (time_left <= 0 || current_state == FINISHED) {
                selected_process->state = FINISHED;
                pthread_mutex_unlock(&selected_process->mutex);
                log_process_finish_priority(selected_process->pid);
                keep_running = 0;
                break;
            }
            
            // Define quantum de execução (máximo 50ms)
            int time_quantum;
            if (time_left > 50) {
                time_quantum = 50;
            } else {
                time_quantum = time_left;
            }
            selected_process->remaining_time -= time_quantum;
            pthread_mutex_unlock(&selected_process->mutex);
            
            // Executa por um quantum
            usleep(50000); // 50ms
            
            // Avalia necessidade de preempção
            PCB* next_priority_process = get_highest_priority_process(&system_state.ready_queue);
            if (next_priority_process != NULL) {
                // Compara prioridades (menor valor = maior prioridade)
                if (next_priority_process->priority < selected_process->priority) {
                    // Preempção necessária
                    pthread_mutex_lock(&selected_process->mutex);
                    if (selected_process->state != FINISHED && selected_process->remaining_time > 0) {
                        selected_process->state = READY;
                        pthread_mutex_unlock(&selected_process->mutex);
                        
                        // Reinsere processo na fila
                        enqueue_process(&system_state.ready_queue, selected_process);
                        add_log_message("Processo PID %d preemptado por processo de maior prioridade\n", selected_process->pid);
                        keep_running = 0;
                    } else {
                        pthread_mutex_unlock(&selected_process->mutex);
                    }
                }
            }
        } while (keep_running);
        
        // Breve pausa antes da próxima iteração
        usleep(10000);
        
    } while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue));
    
    add_log_message("Escalonador por Prioridade finalizado\n");
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
// Thread do escalonador para CPU específico (multiprocessador)
void* scheduler_thread_cpu(void* arg) {
    int cpu_id = *(int*)arg;
    
    add_log_message("Escalonador iniciado (CPU %d)\n", cpu_id);
    
    while (true) {
        // Executa algoritmo baseado no tipo
        switch (system_state.scheduler_type) {
            case FCFS:
                schedule_fcfs_cpu(cpu_id);
                break;
            case ROUND_ROBIN:
                schedule_round_robin_cpu(cpu_id);
                break;
            case PRIORITY:
                schedule_priority_cpu(cpu_id);
                break;
            default:
                fprintf(stderr, "Erro: tipo de escalonador desconhecido\n");
                break;
        }
        
        // Verifica condição de parada
        if (system_state.generator_done && is_queue_empty(&system_state.ready_queue)) {
            break;
        }
        
        // Pequena pausa
        usleep(10000); // 10ms
    }
    
    // Apenas o primeiro CPU que terminar chama log_scheduler_end
    static int scheduler_ended = 0;
    if (__sync_bool_compare_and_swap(&scheduler_ended, 0, 1)) {
        log_scheduler_end();
    }
    
    add_log_message("Escalonador terminado (CPU %d)\n", cpu_id);
    return NULL;
}

// Função auxiliar para obter processo para CPU específico (multiprocessador)
PCB* get_process_for_cpu(int cpu_id) {
    pthread_mutex_lock(&system_state.ready_queue.mutex);
    
    if (system_state.ready_queue.front == NULL) {
        pthread_mutex_unlock(&system_state.ready_queue.mutex);
        return NULL;
    }
    
    QueueNode* current = system_state.ready_queue.front;
    QueueNode* prev = NULL;
    
    while (current != NULL) {
        PCB* candidate = current->pcb;
        pthread_mutex_lock(&candidate->mutex);
        
        // Para processos com múltiplas threads, permite execução em ambos os CPUs
        if (candidate->state == READY) {
            if (candidate->num_threads > 1) {
                // Para processos multithread, não remove da fila ainda
                pthread_mutex_unlock(&candidate->mutex);
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
                return candidate;
            } else {
                // Para processos com uma thread, remove da fila
                if (prev == NULL) {
                    system_state.ready_queue.front = current->next;
                } else {
                    prev->next = current->next;
                }
                if (current == system_state.ready_queue.rear) {
                    system_state.ready_queue.rear = prev;
                }
                system_state.ready_queue.size--;
                free(current);
                
                pthread_mutex_unlock(&candidate->mutex);
                pthread_mutex_unlock(&system_state.ready_queue.mutex);
                return candidate;
            }
        } else if (candidate->state == RUNNING && candidate->num_threads > 1) {
            // Permite que outros CPUs também executem este processo multithread
            pthread_mutex_unlock(&candidate->mutex);
            pthread_mutex_unlock(&system_state.ready_queue.mutex);
            return candidate;
        }
        
        pthread_mutex_unlock(&candidate->mutex);
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&system_state.ready_queue.mutex);
    return NULL;
}

// Versões multiprocessador dos escalonadores
void schedule_fcfs_cpu(int cpu_id) {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador FCFS CPU%d iniciado\n", cpu_id);
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        iteration_count++;
        
        if (iteration_count > 100) {
            break;
        }
        
        PCB* process = get_process_for_cpu(cpu_id);
        if (process != NULL) {
            // Log com identificação do processador
            log_process_start_cpu(scheduler_name, process->pid, cpu_id);
            
            set_process_running(process);
            
            pthread_mutex_lock(&process->mutex);
            while (process->state != FINISHED) {
                pthread_mutex_unlock(&process->mutex);
                usleep(10000);
                pthread_mutex_lock(&process->mutex);
            }
            
            // Para processos multithread, apenas o primeiro CPU a detectar o fim imprime
            int should_log_finish = 0;
            if (process->num_threads > 1) {
                // Usa uma variável estática para controlar quem imprime
                static volatile int finished_processes[MAX_PROCESSES] = {0};
                if (__sync_bool_compare_and_swap(&finished_processes[process->pid - 1], 0, 1)) {
                    should_log_finish = 1;
                }
                
                pthread_mutex_unlock(&process->mutex);
                
                // Remove da fila apenas uma vez
                if (should_log_finish) {
                    pthread_mutex_lock(&system_state.ready_queue.mutex);
                    QueueNode* current = system_state.ready_queue.front;
                    QueueNode* prev = NULL;
                    
                    while (current != NULL) {
                        if (current->pcb == process) {
                            if (prev == NULL) {
                                system_state.ready_queue.front = current->next;
                            } else {
                                prev->next = current->next;
                            }
                            if (current == system_state.ready_queue.rear) {
                                system_state.ready_queue.rear = prev;
                            }
                            system_state.ready_queue.size--;
                            free(current);
                            break;
                        }
                        prev = current;
                        current = current->next;
                    }
                    pthread_mutex_unlock(&system_state.ready_queue.mutex);
                }
            } else {
                should_log_finish = 1;
                pthread_mutex_unlock(&process->mutex);
            }
            
            if (should_log_finish) {
                log_process_finish(scheduler_name, process->pid);
            }
        }
        
        usleep(10000);
    }
    
    add_log_message("Escalonador FCFS CPU%d finalizado\n", cpu_id);
}

void schedule_round_robin_cpu(int cpu_id) {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador Round Robin CPU%d iniciado\n", cpu_id);
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        iteration_count++;
        
        if (iteration_count > 100) {
            break;
        }
        
        PCB* process = get_process_for_cpu(cpu_id);
        if (process != NULL) {
            // Log com identificação do processador
            log_process_start_rr_cpu(process->pid, system_state.quantum, cpu_id);
            
            set_process_running(process);
            
            pthread_mutex_lock(&process->mutex);
            while (process->state != FINISHED) {
                pthread_mutex_unlock(&process->mutex);
                usleep(10000);
                pthread_mutex_lock(&process->mutex);
            }
            
            // Para processos multithread, apenas o primeiro CPU a detectar o fim imprime
            int should_log_finish = 0;
            if (process->num_threads > 1) {
                // Usa uma variável estática para controlar quem imprime
                static volatile int finished_processes_rr[MAX_PROCESSES] = {0};
                if (__sync_bool_compare_and_swap(&finished_processes_rr[process->pid - 1], 0, 1)) {
                    should_log_finish = 1;
                }
                
                pthread_mutex_unlock(&process->mutex);
                
                // Remove da fila apenas uma vez
                if (should_log_finish) {
                    pthread_mutex_lock(&system_state.ready_queue.mutex);
                    QueueNode* current = system_state.ready_queue.front;
                    QueueNode* prev = NULL;
                    
                    while (current != NULL) {
                        if (current->pcb == process) {
                            if (prev == NULL) {
                                system_state.ready_queue.front = current->next;
                            } else {
                                prev->next = current->next;
                            }
                            if (current == system_state.ready_queue.rear) {
                                system_state.ready_queue.rear = prev;
                            }
                            system_state.ready_queue.size--;
                            free(current);
                            break;
                        }
                        prev = current;
                        current = current->next;
                    }
                    pthread_mutex_unlock(&system_state.ready_queue.mutex);
                }
            } else {
                should_log_finish = 1;
                pthread_mutex_unlock(&process->mutex);
            }
            
            if (should_log_finish) {
                log_process_finish(scheduler_name, process->pid);
            }
        }
        
        usleep(10000);
    }
    
    add_log_message("Escalonador Round Robin CPU%d finalizado\n", cpu_id);
}

void schedule_priority_cpu(int cpu_id) {
    add_log_message("Escalonador por Prioridade CPU%d iniciado\n", cpu_id);
    
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        PCB* process = get_process_for_cpu(cpu_id);
        if (process != NULL) {
            // Log com identificação do processador
            log_process_start_priority_cpu(process->pid, process->priority, cpu_id);
            
            set_process_running(process);
            
            pthread_mutex_lock(&process->mutex);
            while (process->state != FINISHED) {
                pthread_mutex_unlock(&process->mutex);
                usleep(10000);
                pthread_mutex_lock(&process->mutex);
            }
            
            // Para processos multithread, apenas o primeiro CPU a detectar o fim imprime
            int should_log_finish = 0;
            if (process->num_threads > 1) {
                // Usa uma variável estática para controlar quem imprime
                static volatile int finished_processes_prio[MAX_PROCESSES] = {0};
                if (__sync_bool_compare_and_swap(&finished_processes_prio[process->pid - 1], 0, 1)) {
                    should_log_finish = 1;
                }
                
                pthread_mutex_unlock(&process->mutex);
                
                // Remove da fila apenas uma vez
                if (should_log_finish) {
                    pthread_mutex_lock(&system_state.ready_queue.mutex);
                    QueueNode* current = system_state.ready_queue.front;
                    QueueNode* prev = NULL;
                    
                    while (current != NULL) {
                        if (current->pcb == process) {
                            if (prev == NULL) {
                                system_state.ready_queue.front = current->next;
                            } else {
                                prev->next = current->next;
                            }
                            if (current == system_state.ready_queue.rear) {
                                system_state.ready_queue.rear = prev;
                            }
                            system_state.ready_queue.size--;
                            free(current);
                            break;
                        }
                        prev = current;
                        current = current->next;
                    }
                    pthread_mutex_unlock(&system_state.ready_queue.mutex);
                }
            } else {
                should_log_finish = 1;
                pthread_mutex_unlock(&process->mutex);
            }
            
            if (should_log_finish) {
                const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
                log_process_finish(scheduler_name, process->pid);
            }
        }
        
        usleep(10000);
    }
    
    add_log_message("Escalonador por Prioridade CPU%d finalizado\n", cpu_id);
}
#endif

#ifdef MULTI
void* scheduler_thread_cpu2(void* arg) {
    int cpu_id = 1; // CPU 1
    (void)arg;
    
    while (1) {
        // Verifica se deve terminar
        if (system_state.generator_done && is_queue_empty(&system_state.ready_queue)) {
            break;
        }
        
        switch (system_state.scheduler_type) {
            case FCFS:
                schedule_fcfs_cpu(cpu_id);
                break;
            case ROUND_ROBIN:
                schedule_round_robin_cpu(cpu_id);
                break;
            case PRIORITY:
                schedule_priority_cpu(cpu_id);
                break;
            default:
                fprintf(stderr, "Erro: tipo de escalonador desconhecido CPU2\n");
                break;
        }
        
        usleep(10000);
    }
    
    log_scheduler_end();
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
