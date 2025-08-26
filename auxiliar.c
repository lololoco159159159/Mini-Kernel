#include "scheduler.h"
#include "logger.h"
#include "process_manager.h"
#include "ready_queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

Scheduler* scheduler = NULL;

Scheduler* create_scheduler(int num_cpus, SchedulerType type) {
    Scheduler* sched = malloc(sizeof(Scheduler));
    if (!sched) return NULL;
    
    sched->ready_queue = create_ready_queue();
    if (!sched->ready_queue) {
        free(sched);
        return NULL;
    }
    
    sched->scheduler_type = type;
    sched->num_cpus = num_cpus;
    sched->current_process[0] = NULL;
    sched->current_process[1] = NULL;
    sched->generator_done = false;
    
    pthread_mutex_init(&sched->scheduler_mutex, NULL);
    pthread_cond_init(&sched->scheduler_cv, NULL);
    
    return sched;
}

void destroy_scheduler(Scheduler* sched) {
    if (!sched) return;
    
    destroy_ready_queue(sched->ready_queue);
    pthread_mutex_destroy(&sched->scheduler_mutex);
    pthread_cond_destroy(&sched->scheduler_cv);
    free(sched);
}

void initialize_scheduler(int num_cpus) {
    scheduler = malloc(sizeof(Scheduler));
    scheduler->ready_queue = create_ready_queue();
    scheduler->num_cpus = num_cpus;
    scheduler->current_process[0] = NULL;
    scheduler->current_process[1] = NULL;
    scheduler->generator_done = false;
    
    pthread_mutex_init(&scheduler->scheduler_mutex, NULL);
    pthread_cond_init(&scheduler->scheduler_cv, NULL);
}

void handle_monoprocessor_execution(PCB* process, const char* policy_names[], char* log_msg) {
    if (scheduler->scheduler_type == PRIORITY) {
        // Implementação específica para Priority com preempção
        while (true) {
            pthread_mutex_lock(&process->mutex);
            if (process->remaining_time <= 0) {
                process->state = FINISHED;
                snprintf(log_msg, 256, "[%s] Processo PID %d finalizado", 
                        policy_names[scheduler->scheduler_type], process->pid);
                add_to_log(log_msg);
                scheduler->current_process[0] = NULL;
                pthread_mutex_unlock(&process->mutex);
                break;
            }
            
            int slice = (process->remaining_time >= 50) ? 50 : process->remaining_time;
            process->remaining_time -= slice;
            pthread_mutex_unlock(&process->mutex);
            usleep(50000); // 50ms
            
            // Verificar se existe processo com prioridade mais alta pronto
            PCB* peek = ready_queue_peek_highest_priority(scheduler->ready_queue);
            int preempt = 0;
            if (peek && peek->priority < process->priority) preempt = 1;
            
            if (preempt) {
                pthread_mutex_lock(&process->mutex);
                if (process->state != FINISHED && process->remaining_time > 0) {
                    process->state = READY;
                    scheduler->current_process[0] = NULL;
                    pthread_mutex_unlock(&process->mutex);
                    // Colocar processo preemptado de volta na fila
                    enqueue_process(scheduler->ready_queue, process);
                } else {
                    pthread_mutex_unlock(&process->mutex);
                }
                break;
            }
        }
        return;
    }
    
    // Lógica original para FCFS e RR
    // Aguardar quantum ou término
    if (scheduler->scheduler_type == RR) {
        // Para processos que podem terminar no quantum, aguardar término
        long start_time = get_current_time_ms();
        while (get_current_time_ms() - start_time < QUANTUM_MS) {
            pthread_mutex_lock(&process->mutex);
            if (process->state == FINISHED) {
                pthread_mutex_unlock(&process->mutex);
                break;
            }
            pthread_mutex_unlock(&process->mutex);
            usleep(10000); // 10ms
        }
    }
    
    // Verificar se terminou
    if (scheduler->scheduler_type == RR) {
        // Para Round Robin, verificar após quantum
        pthread_mutex_lock(&process->mutex);
        if (process->state == FINISHED) {
            snprintf(log_msg, 256, "[%s] Processo PID %d finalizado", 
                    policy_names[scheduler->scheduler_type], process->pid);
            add_to_log(log_msg);
            scheduler->current_process[0] = NULL;
        } else {
            // Preempção no Round Robin - parar o processo primeiro
            process->state = READY;
            pthread_cond_broadcast(&process->cv); // Acordar threads para verificar estado
            scheduler->current_process[0] = NULL;
            pthread_mutex_unlock(&process->mutex);
            // Só recoloca na fila se ainda tem tempo restante
            if (process->remaining_time > 0) {
                enqueue_process(scheduler->ready_queue, process);
            }
            return; // Retornar sem unlock
        }
        pthread_mutex_unlock(&process->mutex);
    }
    
    // Para FCFS, aguardar término completo
    if (scheduler->scheduler_type == FCFS) {
        while (true) {
            pthread_mutex_lock(&process->mutex);
            if (process->state == FINISHED) {
                snprintf(log_msg, 256, "[%s] Processo PID %d finalizado", 
                        policy_names[scheduler->scheduler_type], process->pid);
                add_to_log(log_msg);
                scheduler->current_process[0] = NULL;
                pthread_mutex_unlock(&process->mutex);
                break;
            }
            pthread_mutex_unlock(&process->mutex);
            usleep(100000); // 100ms
        }
    }
}

void handle_multiprocessor_execution(const char* policy_names[], char* log_msg) {
    // Verificar processos terminados primeiro
    for (int cpu = 0; cpu < scheduler->num_cpus; cpu++) {
        PCB* process = scheduler->current_process[cpu];
        if (process != NULL) {
            pthread_mutex_lock(&process->mutex);
            if (process->state == FINISHED) {
                // Verificar se já logamos a finalização deste processo
                bool already_logged = false;
                for (int i = 0; i < cpu; i++) {
                    if (scheduler->current_process[i] == process) {
                        already_logged = true;
                        break;
                    }
                }
                
                if (!already_logged) {
                    snprintf(log_msg, 256, "[%s] Processo PID %d finalizado", 
                            policy_names[scheduler->scheduler_type], process->pid);
                    add_to_log(log_msg);
                }
                
                // Limpar processo de todos os CPUs
                for (int i = 0; i < scheduler->num_cpus; i++) {
                    if (scheduler->current_process[i] == process) {
                        scheduler->current_process[i] = NULL;
                    }
                }
                
                // Para Round Robin multiprocessador, fazer rebalanceamento após término
                if (scheduler->scheduler_type == RR && scheduler->num_cpus > 1) {
                    // Coletar todos os processos ainda em execução
                    PCB* running_processes[scheduler->num_cpus];
                    int running_count = 0;
                    
                    for (int i = 0; i < scheduler->num_cpus; i++) {
                        if (scheduler->current_process[i] != NULL && scheduler->current_process[i] != process) {
                            running_processes[running_count++] = scheduler->current_process[i];
                            scheduler->current_process[i] = NULL; // Limpar para re-alocar
                        }
                    }
                    
                    // Re-alocar processos em execução sequencialmente nos CPUs
                    // Só fazer rebalanceamento se há processos na fila esperando
                    if (running_count > 0 && !is_queue_empty(scheduler->ready_queue)) {
                        for (int i = 0; i < running_count; i++) {
                            scheduler->current_process[i] = running_processes[i];
                            
                            snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                                    policy_names[scheduler->scheduler_type], running_processes[i]->pid, QUANTUM_MS, i);
                            add_to_log(log_msg);
                        }
                    } else if (running_count > 0) {
                        // Se não há fila, apenas restaurar processos sem re-logar
                        for (int i = 0; i < running_count; i++) {
                            scheduler->current_process[i] = running_processes[i];
                        }
                    }
                }
                
                // Sinalizar escalonador para verificar possível expansão
                pthread_mutex_lock(&scheduler->scheduler_mutex);
                pthread_cond_signal(&scheduler->scheduler_cv);
                pthread_mutex_unlock(&scheduler->scheduler_mutex);
            }
            pthread_mutex_unlock(&process->mutex);
        }
    }
    
    // Verificar se há processos em execução que podem se expandir para CPUs livres
    for (int cpu = 0; cpu < scheduler->num_cpus; cpu++) {
        PCB* running_process = scheduler->current_process[cpu];
        if (running_process != NULL && running_process->state == RUNNING) {
            // Verificar se processo pode usar mais CPUs
            int current_cpus = 0;
            for (int i = 0; i < scheduler->num_cpus; i++) {
                if (scheduler->current_process[i] == running_process) {
                    current_cpus++;
                }
            }
            
            // Em multiprocessador, processo pode usar quantos CPUs estiverem livres
            // Para Round Robin, só expandir se não há processos na fila
            bool can_expand = (scheduler->scheduler_type != RR) || is_queue_empty(scheduler->ready_queue);
            if (current_cpus < scheduler->num_cpus && can_expand) {
                bool expanded = false;
                
                // Alocar CPUs livres (sem logar ainda)
                for (int free_cpu = 0; free_cpu < scheduler->num_cpus; free_cpu++) {
                    if (scheduler->current_process[free_cpu] == NULL) {
                        scheduler->current_process[free_cpu] = running_process;
                        expanded = true;
                    }
                }
                
                // Se houve expansão, logar todos os CPUs onde o processo agora está executando
                if (expanded) {
                    for (int all_cpu = 0; all_cpu < scheduler->num_cpus; all_cpu++) {
                        if (scheduler->current_process[all_cpu] == running_process) {
                            if (scheduler->scheduler_type == RR) {
                                snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                                        policy_names[scheduler->scheduler_type], running_process->pid, QUANTUM_MS, all_cpu);
                            } else {
                                snprintf(log_msg, 256, "[%s] Executando processo PID %d // processador %d", 
                                        policy_names[scheduler->scheduler_type], running_process->pid, all_cpu);
                            }
                            add_to_log(log_msg);
                        }
                    }
                }
            }
        }
    }
    
    // Alocar novos processos para CPUs livres
    for (int cpu = 0; cpu < scheduler->num_cpus; cpu++) {
        if (scheduler->current_process[cpu] == NULL) {
            PCB* process = NULL;
            
            // Selecionar processo baseado na política
            switch (scheduler->scheduler_type) {
                case FCFS:
                    process = dequeue_process(scheduler->ready_queue);
                    break;
                case PRIORITY:
                    process = find_highest_priority_process(scheduler->ready_queue);
                    break;
                case RR:
                    process = dequeue_process(scheduler->ready_queue);
                    break;
            }
            
            if (process != NULL) {
                pthread_mutex_lock(&process->mutex);
                process->state = RUNNING;
                scheduler->current_process[cpu] = process;
                
                if (scheduler->scheduler_type == RR) {
                    snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                            policy_names[scheduler->scheduler_type], process->pid, QUANTUM_MS, cpu);
                } else {
                    snprintf(log_msg, 256, "[%s] Executando processo PID %d // processador %d", 
                            policy_names[scheduler->scheduler_type], process->pid, cpu);
                }
                add_to_log(log_msg);
                
                pthread_cond_broadcast(&process->cv);
                pthread_mutex_unlock(&process->mutex);
                
                // Se processo tem múltiplas threads, tentar usar próximo CPU livre também
                // EXCETO para Round Robin, que deve usar apenas um CPU por processo
                if (process->num_threads > 1 && scheduler->scheduler_type != RR) {
                    for (int next_cpu = cpu + 1; next_cpu < scheduler->num_cpus; next_cpu++) {
                        if (scheduler->current_process[next_cpu] == NULL) {
                            scheduler->current_process[next_cpu] = process;
                            
                            snprintf(log_msg, 256, "[%s] Executando processo PID %d // processador %d", 
                                    policy_names[scheduler->scheduler_type], process->pid, next_cpu);
                            add_to_log(log_msg);
                            break; // Só alocar um CPU adicional por vez
                        }
                    }
                }
            }
        }
    }
}

void* scheduler_thread_function(void* arg) {
    (void)arg; // Suprimir warning
    char log_msg[256];
    const char* policy_names[] = {"", "FCFS", "RR", "PRIORITY"};
    
    while (true) {
        pthread_mutex_lock(&scheduler->scheduler_mutex);
        
        // Verificar se ainda há processos em execução
        bool has_running_processes = false;
        for (int cpu = 0; cpu < scheduler->num_cpus; cpu++) {
            if (scheduler->current_process[cpu] != NULL) {
                has_running_processes = true;
                break;
            }
        }
        
        // Aguardar se não há processos prontos e não há processos em execução
        while (is_queue_empty(scheduler->ready_queue) && !scheduler->generator_done && !has_running_processes) {
            pthread_cond_wait(&scheduler->scheduler_cv, &scheduler->scheduler_mutex);
            
            // Recalcular após acordar
            has_running_processes = false;
            for (int cpu = 0; cpu < scheduler->num_cpus; cpu++) {
                if (scheduler->current_process[cpu] != NULL) {
                    has_running_processes = true;
                    break;
                }
            }
        }
        
        if (is_queue_empty(scheduler->ready_queue) && scheduler->generator_done && !has_running_processes) {
            pthread_mutex_unlock(&scheduler->scheduler_mutex);
            break;
        }
        
        pthread_mutex_unlock(&scheduler->scheduler_mutex);
        
        // Executar algoritmo de escalonamento
        if (scheduler->num_cpus == 1) {
            // Monoprocessador
            PCB* process = NULL;
            
            // Selecionar processo baseado na política (apenas se CPU livre)
            if (scheduler->current_process[0] == NULL) {
                switch (scheduler->scheduler_type) {
                    case FCFS:
                        process = dequeue_process(scheduler->ready_queue);
                        break;
                    case PRIORITY:
                        process = find_highest_priority_process(scheduler->ready_queue);
                        break;
                    case RR:
                        process = dequeue_process(scheduler->ready_queue);
                        break;
                }
                
                if (process != NULL) {
                    pthread_mutex_lock(&process->mutex);
                    process->state = RUNNING;
                    scheduler->current_process[0] = process;
                    
                    if (scheduler->scheduler_type == RR) {
                        snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms", 
                                policy_names[scheduler->scheduler_type], process->pid, QUANTUM_MS);
                    } else if (scheduler->scheduler_type == PRIORITY) {
                        snprintf(log_msg, 256, "[%s] Executando processo PID %d prioridade %d", 
                                policy_names[scheduler->scheduler_type], process->pid, process->priority);
                    } else {
                        snprintf(log_msg, 256, "[%s] Executando processo PID %d", 
                                policy_names[scheduler->scheduler_type], process->pid);
                    }
                    add_to_log(log_msg);
                    
                    pthread_cond_broadcast(&process->cv);
                    pthread_mutex_unlock(&process->mutex);
                    
                    handle_monoprocessor_execution(process, policy_names, log_msg);
                }
            }
        } else {
            // Multiprocessador
            handle_multiprocessor_execution(policy_names, log_msg);
        }
        
        usleep(50); // 0.05ms de intervalo - muito rápido para máxima responsividade
    }
    
    add_to_log("Escalonador terminou execução de todos processos");
    return NULL;
}
