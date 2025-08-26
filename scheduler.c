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
/* Função auxiliar: Verifica se o processo já foi logado como finalizado */
static bool is_process_already_logged_as_finished(PCB* process, int current_cpu) {
    for (int i = 0; i < current_cpu; i++) {
        if (system_state.current_process_array[i] == process) {
            return true;
        }
    }
    return false;
}

/* Função auxiliar: Remove processo de todos os CPUs */
static void remove_process_from_all_cpus(PCB* process) {
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] == process) {
            system_state.current_process_array[cpu] = NULL;
        }
    }
}

/* Função auxiliar: Coleta processos ativos exceto o terminado */
static int collect_active_processes(PCB* finished_process, PCB* active_processes[]) {
    int active_count = 0;
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        PCB* current = system_state.current_process_array[cpu];
        if (current != NULL && current != finished_process) {
            active_processes[active_count++] = current;
            system_state.current_process_array[cpu] = NULL; // Limpar para re-alocar
        }
    }
    return active_count;
}

/* Função auxiliar: Rebalanceia processos Round Robin após término */
static void rebalance_round_robin_processes(PCB* active_processes[], int active_count, 
                                          const char* scheduler_names[], char* message_buffer) {
    if (active_count > 0 && !is_queue_empty(&system_state.ready_queue)) {
        // Re-alocar com log se há fila esperando
        for (int i = 0; i < active_count; i++) {
            system_state.current_process_array[i] = active_processes[i];
            
            snprintf(message_buffer, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                    scheduler_names[system_state.scheduler_type], active_processes[i]->pid, 
                    THREAD_EXECUTION_TIME, i);
            add_essential_log_message("%s\n", message_buffer);
        }
    } else if (active_count > 0) {
        // Apenas restaurar sem re-logar se não há fila
        for (int i = 0; i < active_count; i++) {
            system_state.current_process_array[i] = active_processes[i];
        }
    }
}

/* Função auxiliar: Re-loga processos que continuam em outras políticas */
static void relog_continuing_processes(PCB* finished_process, 
                                     const char* scheduler_names[], char* message_buffer) {
    bool has_available_cpu = false;
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] == NULL) {
            has_available_cpu = true;
            break;
        }
    }
    
    if (has_available_cpu) {
        for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
            PCB* continuing_proc = system_state.current_process_array[cpu];
            if (continuing_proc != NULL && continuing_proc != finished_process) {
                snprintf(message_buffer, 256, "[%s] Executando processo PID %d // processador %d", 
                        scheduler_names[system_state.scheduler_type], continuing_proc->pid, cpu);
                add_essential_log_message("%s\n", message_buffer);
            }
        }
    }
}

/* Função modular: Processa terminação de processos */
static void handle_finished_processes(const char* scheduler_names[], char* message_buffer) {
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        PCB* current_proc = system_state.current_process_array[cpu];
        if (current_proc == NULL) continue;
        
        pthread_mutex_lock(&current_proc->mutex);
        if (current_proc->state == FINISHED) {
            // Verificar se já foi logado
            if (!is_process_already_logged_as_finished(current_proc, cpu)) {
                snprintf(message_buffer, 256, "[%s] Processo PID %d finalizado", 
                        scheduler_names[system_state.scheduler_type], current_proc->pid);
                add_essential_log_message("%s\n", message_buffer);
            }
            
            // Remover de todos os CPUs
            remove_process_from_all_cpus(current_proc);
            
            // Rebalanceamento específico por política
            if (system_state.scheduler_type == ROUND_ROBIN && system_state.num_cpus > 1) {
                PCB* active_processes[system_state.num_cpus];
                int active_count = collect_active_processes(current_proc, active_processes);
                rebalance_round_robin_processes(active_processes, active_count, scheduler_names, message_buffer);
            } else {
                relog_continuing_processes(current_proc, scheduler_names, message_buffer);
            }
            
            // Sinalizar escalonador
            pthread_mutex_lock(&system_state.scheduler_mutex);
            pthread_cond_signal(&system_state.scheduler_cv);
            pthread_mutex_unlock(&system_state.scheduler_mutex);
        }
        pthread_mutex_unlock(&current_proc->mutex);
    }
}

/* Função auxiliar: Conta CPUs usados por um processo */
static int count_cpus_used_by_process(PCB* process) {
    int count = 0;
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] == process) {
            count++;
        }
    }
    return count;
}

/* Função auxiliar: Expande processo para CPUs livres */
static bool expand_process_to_free_cpus(PCB* process) {
    bool expansion_occurred = false;
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] == NULL) {
            system_state.current_process_array[cpu] = process;
            expansion_occurred = true;
        }
    }
    return expansion_occurred;
}

/* Função auxiliar: Loga expansão de processo */
static void log_process_expansion(PCB* process, const char* scheduler_names[], char* message_buffer) {
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] == process) {
            snprintf(message_buffer, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                    scheduler_names[system_state.scheduler_type], process->pid, THREAD_EXECUTION_TIME, cpu);
            add_essential_log_message("%s\n", message_buffer);
        }
    }
}

/* Função modular: Gerencia expansão de processos Round Robin */
static void handle_process_expansion(const char* scheduler_names[], char* message_buffer) {
    if (!is_queue_empty(&system_state.ready_queue) || system_state.scheduler_type != ROUND_ROBIN) {
        return; // Só expande Round Robin quando fila vazia
    }
    
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        PCB* expanding_process = system_state.current_process_array[cpu];
        if (expanding_process == NULL || expanding_process->state != RUNNING) {
            continue;
        }
        
        int current_cpu_count = count_cpus_used_by_process(expanding_process);
        if (current_cpu_count < system_state.num_cpus) {
            bool expansion_occurred = expand_process_to_free_cpus(expanding_process);
            if (expansion_occurred) {
                log_process_expansion(expanding_process, scheduler_names, message_buffer);
            }
        }
        break; // Só processar um processo por vez
    }
}

/* Função auxiliar: Seleciona processo baseado na política */
static PCB* select_process_by_policy(void) {
    PCB* selected = NULL;
    
    switch (system_state.scheduler_type) {
        case FCFS:
            selected = dequeue_process(&system_state.ready_queue);
            break;
        case PRIORITY:
            selected = get_highest_priority_process(&system_state.ready_queue);
            if (selected != NULL) {
                remove_process_from_queue(&system_state.ready_queue, selected);
            }
            break;
        case ROUND_ROBIN:
            selected = dequeue_process(&system_state.ready_queue);
            break;
    }
    
    return selected;
}

/* Função auxiliar: Configura e loga novo processo em CPU */
static void assign_process_to_cpu(PCB* process, int cpu_slot, 
                                const char* scheduler_names[], char* message_buffer) {
    pthread_mutex_lock(&process->mutex);
    process->state = RUNNING;
    system_state.current_process_array[cpu_slot] = process;
    
    // Log baseado na política
    if (system_state.scheduler_type == ROUND_ROBIN) {
        snprintf(message_buffer, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                scheduler_names[system_state.scheduler_type], process->pid, THREAD_EXECUTION_TIME, cpu_slot);
    } else {
        snprintf(message_buffer, 256, "[%s] Executando processo PID %d // processador %d", 
                scheduler_names[system_state.scheduler_type], process->pid, cpu_slot);
    }
    add_essential_log_message("%s\n", message_buffer);
    
    pthread_cond_broadcast(&process->cv);
    pthread_mutex_unlock(&process->mutex);
}

/* Função auxiliar: Tenta expandir processo multi-thread para CPU adicional */
static void try_multithread_expansion(PCB* process, int starting_cpu, 
                                    const char* scheduler_names[], char* message_buffer) {
    if (process->num_threads <= 1 || system_state.scheduler_type == ROUND_ROBIN) {
        return; // Não expande single-thread ou Round Robin
    }
    
    for (int cpu = starting_cpu + 1; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] == NULL) {
            system_state.current_process_array[cpu] = process;
            
            snprintf(message_buffer, 256, "[%s] Executando processo PID %d // processador %d", 
                    scheduler_names[system_state.scheduler_type], process->pid, cpu);
            add_essential_log_message("%s\n", message_buffer);
            break; // Só um CPU adicional por vez
        }
    }
}

/* Função modular: Aloca novos processos para CPUs livres */
static void allocate_new_processes_to_cpus(const char* scheduler_names[], char* message_buffer) {
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        if (system_state.current_process_array[cpu] != NULL) {
            continue; // CPU ocupado
        }
        
        PCB* new_process = select_process_by_policy();
        if (new_process == NULL) {
            continue; // Nenhum processo disponível
        }
        
        assign_process_to_cpu(new_process, cpu, scheduler_names, message_buffer);
        try_multithread_expansion(new_process, cpu, scheduler_names, message_buffer);
    }
}

/* Função principal modularizada */
void execute_multicore_scheduling(const char* scheduler_names[], char* message_buffer) {
    handle_finished_processes(scheduler_names, message_buffer);
    handle_process_expansion(scheduler_names, message_buffer);  
    allocate_new_processes_to_cpus(scheduler_names, message_buffer);
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
