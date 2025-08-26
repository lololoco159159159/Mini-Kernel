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
    system_state.generator_done = 0;
    
    // Inicializa mutexes e condições
    pthread_mutex_init(&system_state.scheduler_mutex, NULL);
    pthread_cond_init(&system_state.scheduler_cv, NULL);
    
    // Inicializa o tempo de início
    struct timeval tv;
    gettimeofday(&tv, NULL);
    system_state.start_time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
#ifdef MULTI
    system_state.num_cpus = 2;
    system_state.current_process_array[0] = NULL;
    system_state.current_process_array[1] = NULL;
    pthread_mutex_init(&system_state.cpu2_mutex, NULL);
#endif
}

void* scheduler_thread(void* arg) {
    (void)arg; // Suprime warning de parâmetro não utilizado  
    
    while (1) {
        // Verifica se deve terminar
        if (system_state.generator_done && is_queue_empty(&system_state.ready_queue)) {
            break;
        }
        
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
                configure_process_state(process);
                
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
        configure_process_state(selected_process);
        
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

void configure_process_state(PCB* pcb) {
    if (pcb == NULL) return;
    
    pthread_mutex_lock(&pcb->mutex);
    pcb->state = RUNNING;
    pthread_cond_broadcast(&pcb->cv); // Acorda todas as threads do processo
    pthread_mutex_unlock(&pcb->mutex);
}

void halt_process_execution(PCB* pcb) {
    if (pcb == NULL) return;
    
    pthread_mutex_lock(&pcb->mutex);
    if (pcb->state == RUNNING) {
        pcb->state = READY;
    }
    pthread_mutex_unlock(&pcb->mutex);
}

PCB* find_higher_priority_process(int current_priority) {
    PCB* highest_priority = get_highest_priority_process(&system_state.ready_queue);
    
    if (highest_priority != NULL && highest_priority->priority < current_priority) {
        return highest_priority;
    }
    
    return NULL;
}

void pause_execution(int milliseconds) {
    usleep(milliseconds * 1000);
}

long calculate_elapsed_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long current_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return current_time - system_state.start_time_ms;
}

int verify_all_processes_completed() {
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
void execute_multicore_scheduling(const char* scheduler_names[], char* message_buffer) {
    // Verificar processos terminados primeiro
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        PCB* current_proc = system_state.current_process_array[cpu];
        if (current_proc != NULL) {
            pthread_mutex_lock(&current_proc->mutex);
            if (current_proc->state == FINISHED) {
                // Verificar se já logamos a finalização deste processo
                bool log_already_written = false;
                for (int j = 0; j < cpu; j++) {
                    if (system_state.current_process_array[j] == current_proc) {
                        log_already_written = true;
                        break;
                    }
                }
                
                if (!log_already_written) {
                    snprintf(message_buffer, 256, "[%s] Processo PID %d finalizado", 
                            scheduler_names[system_state.scheduler_type], current_proc->pid);
                    add_essential_log_message("%s\n", message_buffer);
                }
                
                // Limpar processo de todos os CPUs
                for (int k = 0; k < system_state.num_cpus; k++) {
                    if (system_state.current_process_array[k] == current_proc) {
                        system_state.current_process_array[k] = NULL;
                    }
                }
                
                // Para Round Robin multiprocessador, fazer rebalanceamento após término
                if (system_state.scheduler_type == ROUND_ROBIN && system_state.num_cpus > 1) {
                    // Coletar todos os processos ainda em execução
                    PCB* active_processes[system_state.num_cpus];
                    int active_count = 0;
                    
                    for (int m = 0; m < system_state.num_cpus; m++) {
                        if (system_state.current_process_array[m] != NULL && system_state.current_process_array[m] != current_proc) {
                            active_processes[active_count++] = system_state.current_process_array[m];
                            system_state.current_process_array[m] = NULL; // Limpar para re-alocar
                        }
                    }
                    
                    // Re-alocar processos em execução sequencialmente nos CPUs
                    // Só fazer rebalanceamento se há processos na fila esperando
                    if (active_count > 0 && !is_queue_empty(&system_state.ready_queue)) {
                        for (int n = 0; n < active_count; n++) {
                            system_state.current_process_array[n] = active_processes[n];
                            
                            snprintf(message_buffer, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                                    scheduler_names[system_state.scheduler_type], active_processes[n]->pid, THREAD_EXECUTION_TIME, n);
                            add_essential_log_message("%s\n", message_buffer);
                        }
                    } else if (active_count > 0) {
                        // Se não há fila, apenas restaurar processos sem re-logar
                        for (int p = 0; p < active_count; p++) {
                            system_state.current_process_array[p] = active_processes[p];
                        }
                    }
                } else {
                    // Para outras políticas, re-logar processos que continuam executando
                    // quando há CPUs que ficaram livres
                    bool has_available_cpu = false;
                    for (int q = 0; q < system_state.num_cpus; q++) {
                        if (system_state.current_process_array[q] == NULL) {
                            has_available_cpu = true;
                            break;
                        }
                    }
                    
                    if (has_available_cpu) {
                        for (int r = 0; r < system_state.num_cpus; r++) {
                            PCB* continuing_proc = system_state.current_process_array[r];
                            if (continuing_proc != NULL && continuing_proc != current_proc) {
                                snprintf(message_buffer, 256, "[%s] Executando processo PID %d // processador %d", 
                                        scheduler_names[system_state.scheduler_type], continuing_proc->pid, r);
                                add_essential_log_message("%s\n", message_buffer);
                            }
                        }
                    }
                }
                
                // Sinalizar escalonador para verificar possível expansão
                pthread_mutex_lock(&system_state.scheduler_mutex);
                pthread_cond_signal(&system_state.scheduler_cv);
                pthread_mutex_unlock(&system_state.scheduler_mutex);
            }
            pthread_mutex_unlock(&current_proc->mutex);
        }
    }
    
    // Verificar se há processos em execução que podem se expandir para CPUs livres
    // Apenas para Round Robin quando não há processos na fila
    if (is_queue_empty(&system_state.ready_queue) && system_state.scheduler_type == ROUND_ROBIN) {
        for (int cpu_idx = 0; cpu_idx < system_state.num_cpus; cpu_idx++) {
            PCB* expanding_process = system_state.current_process_array[cpu_idx];
            if (expanding_process != NULL && expanding_process->state == RUNNING) {
                // Verificar se processo pode usar mais CPUs
                int current_cpu_count = 0;
                for (int s = 0; s < system_state.num_cpus; s++) {
                    if (system_state.current_process_array[s] == expanding_process) {
                        current_cpu_count++;
                    }
                }
                
                // Expandir para CPUs livres
                if (current_cpu_count < system_state.num_cpus) {
                    bool expansion_occurred = false;
                    
                    // Alocar CPUs livres
                    for (int available_cpu = 0; available_cpu < system_state.num_cpus; available_cpu++) {
                        if (system_state.current_process_array[available_cpu] == NULL) {
                            system_state.current_process_array[available_cpu] = expanding_process;
                            expansion_occurred = true;
                        }
                    }
                    
                    // Se houve expansão, logar todos os CPUs onde o processo agora está executando
                    if (expansion_occurred) {
                        for (int target_cpu = 0; target_cpu < system_state.num_cpus; target_cpu++) {
                            if (system_state.current_process_array[target_cpu] == expanding_process) {
                                snprintf(message_buffer, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                                        scheduler_names[system_state.scheduler_type], expanding_process->pid, THREAD_EXECUTION_TIME, target_cpu);
                                add_essential_log_message("%s\n", message_buffer);
                            }
                        }
                    }
                }
                break; // Só processar um processo de cada vez
            }
        }
    }
    
    // Alocar novos processos para CPUs livres
    for (int cpu_slot = 0; cpu_slot < system_state.num_cpus; cpu_slot++) {
        PCB* assigned_process = system_state.current_process_array[cpu_slot];
        if (assigned_process == NULL) {
            PCB* new_process = NULL;
            
            // Selecionar processo baseado na política
            switch (system_state.scheduler_type) {
                case FCFS:
                    new_process = dequeue_process(&system_state.ready_queue);
                    break;
                case PRIORITY:
                    new_process = get_highest_priority_process(&system_state.ready_queue);
                    if (new_process != NULL) {
                        remove_process_from_queue(&system_state.ready_queue, new_process);
                    }
                    break;
                case ROUND_ROBIN:
                    new_process = dequeue_process(&system_state.ready_queue);
                    break;
            }

            if (new_process != NULL) {
                pthread_mutex_lock(&new_process->mutex);
                new_process->state = RUNNING;
                system_state.current_process_array[cpu_slot] = new_process;
                
                if (system_state.scheduler_type == ROUND_ROBIN) {
                    snprintf(message_buffer, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                            scheduler_names[system_state.scheduler_type], new_process->pid, THREAD_EXECUTION_TIME, cpu_slot);
                } else {
                    snprintf(message_buffer, 256, "[%s] Executando processo PID %d // processador %d", 
                            scheduler_names[system_state.scheduler_type], new_process->pid, cpu_slot);
                }
                add_essential_log_message("%s\n", message_buffer);
                
                pthread_cond_broadcast(&new_process->cv);
                pthread_mutex_unlock(&new_process->mutex);
                
                // Se processo tem múltiplas threads, tentar usar próximo CPU livre também
                // EXCETO para Round Robin, que deve usar apenas um CPU por processo
                if (new_process->num_threads > 1 && system_state.scheduler_type != ROUND_ROBIN) {
                    for (int additional_cpu = cpu_slot + 1; additional_cpu < system_state.num_cpus; additional_cpu++) {
                        if (system_state.current_process_array[additional_cpu] == NULL) {
                            system_state.current_process_array[additional_cpu] = new_process;
                            
                            snprintf(message_buffer, 256, "[%s] Executando processo PID %d // processador %d", 
                                    scheduler_names[system_state.scheduler_type], new_process->pid, additional_cpu);
                            add_essential_log_message("%s\n", message_buffer);
                            break; // Só alocar um CPU adicional por vez
                        }
                    }
                }
            }
        }
    }
}
#endif

#ifdef MULTI
void* multicore_scheduler_main(void* arg) {
    (void)arg; // Suprimir warning
    char message_buffer[256];
    const char* scheduler_names[] = {"", "FCFS", "RR", "PRIORITY"};
    
    add_log_message("[DEBUG] Iniciando scheduler_thread_function\n");
    
    while (true) {
        pthread_mutex_lock(&system_state.scheduler_mutex);
        
        // Verificar se ainda há processos em execução
        bool has_running_processes = false;
        for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
            if (system_state.current_process_array[cpu] != NULL) {
                has_running_processes = true;
                break;
            }
        }
        
        // printf("[DEBUG] has_running_processes: %d, generator_done: %d, queue_empty: %d\n", 
        //        has_running_processes, system_state.generator_done, is_queue_empty(&system_state.ready_queue));
        
        // Debug: mostrar quais processos estão em execução
        /*
        for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
            PCB* proc = system_state.current_process_array[cpu];
            if (proc != NULL) {
                printf("[DEBUG] CPU %d: PID %d, state %d, remaining_time %d\n", 
                       cpu, proc->pid, proc->state, proc->remaining_time);
            }
        }
        fflush(stdout);
        */
        
        // Debug: mostrar estado atual
        // add_log_message("[DEBUG] Queue empty: %d, generator_done: %d, has_running: %d\n", 
        //                is_queue_empty(&system_state.ready_queue), system_state.generator_done, has_running_processes);
        
        // Aguardar se não há processos prontos e não há processos em execução
        while (is_queue_empty(&system_state.ready_queue) && !system_state.generator_done && !has_running_processes) {
            pthread_cond_wait(&system_state.scheduler_cv, &system_state.scheduler_mutex);
            
            // Recalcular após acordar
            has_running_processes = false;
            for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
                if (system_state.current_process_array[cpu] != NULL) {
                    has_running_processes = true;
                    break;
                }
            }
        }
        
        if (is_queue_empty(&system_state.ready_queue) && system_state.generator_done && !has_running_processes) {
            pthread_mutex_unlock(&system_state.scheduler_mutex);
            break;
        }
        
        pthread_mutex_unlock(&system_state.scheduler_mutex);
        
        // Executar algoritmo de escalonamento multiprocessador
        execute_multicore_scheduling(scheduler_names, message_buffer);
        
        usleep(50); // 0.05ms de intervalo - muito rápido para máxima responsividade
    }

    add_essential_log_message("Escalonador terminou execução de todos processos\n");
    return NULL;
}

#endif
