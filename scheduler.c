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
void handle_multiprocessor_execution(const char* policy_names[], char* log_msg) {
    // Verificar processos terminados primeiro
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        PCB* process = system_state.current_process_array[cpu];
        if (process != NULL) {
            pthread_mutex_lock(&process->mutex);
            if (process->state == FINISHED) {
                // Verificar se já logamos a finalização deste processo
                bool already_logged = false;
                for (int i = 0; i < cpu; i++) {
                    if (system_state.current_process_array[i] == process) {
                        already_logged = true;
                        break;
                    }
                }
                
                if (!already_logged) {
                    snprintf(log_msg, 256, "[%s] Processo PID %d finalizado", 
                            policy_names[system_state.scheduler_type], process->pid);
                    add_essential_log_message("%s\n", log_msg);
                }
                
                // Limpar processo de todos os CPUs
                for (int i = 0; i < system_state.num_cpus; i++) {
                    if (system_state.current_process_array[i] == process) {
                        system_state.current_process_array[i] = NULL;
                    }
                }
                
                // Para Round Robin multiprocessador, fazer rebalanceamento após término
                if (system_state.scheduler_type == ROUND_ROBIN && system_state.num_cpus > 1) {
                    // Coletar todos os processos ainda em execução
                    PCB* running_processes[system_state.num_cpus];
                    int running_count = 0;
                    
                    for (int i = 0; i < system_state.num_cpus; i++) {
                        if (system_state.current_process_array[i] != NULL && system_state.current_process_array[i] != process) {
                            running_processes[running_count++] = system_state.current_process_array[i];
                            system_state.current_process_array[i] = NULL; // Limpar para re-alocar
                        }
                    }
                    
                    // Re-alocar processos em execução sequencialmente nos CPUs
                    // Só fazer rebalanceamento se há processos na fila esperando
                    if (running_count > 0 && !is_queue_empty(&system_state.ready_queue)) {
                        for (int i = 0; i < running_count; i++) {
                            system_state.current_process_array[i] = running_processes[i];
                            
                            snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                                    policy_names[system_state.scheduler_type], running_processes[i]->pid, THREAD_EXECUTION_TIME, i);
                            add_essential_log_message("%s\n", log_msg);
                        }
                    } else if (running_count > 0) {
                        // Se não há fila, apenas restaurar processos sem re-logar
                        for (int i = 0; i < running_count; i++) {
                            system_state.current_process_array[i] = running_processes[i];
                        }
                    }
                } else {
                    // Para outras políticas, re-logar processos que continuam executando
                    // quando há CPUs que ficaram livres
                    bool has_free_cpu = false;
                    for (int i = 0; i < system_state.num_cpus; i++) {
                        if (system_state.current_process_array[i] == NULL) {
                            has_free_cpu = true;
                            break;
                        }
                    }
                    
                    if (has_free_cpu) {
                        for (int i = 0; i < system_state.num_cpus; i++) {
                            PCB* running_proc = system_state.current_process_array[i];
                            if (running_proc != NULL && running_proc != process) {
                                snprintf(log_msg, 256, "[%s] Executando processo PID %d // processador %d", 
                                        policy_names[system_state.scheduler_type], running_proc->pid, i);
                                add_essential_log_message("%s\n", log_msg);
                            }
                        }
                    }
                }
                
                // Sinalizar escalonador para verificar possível expansão
                pthread_mutex_lock(&system_state.scheduler_mutex);
                pthread_cond_signal(&system_state.scheduler_cv);
                pthread_mutex_unlock(&system_state.scheduler_mutex);
            }
            pthread_mutex_unlock(&process->mutex);
        }
    }
    
    // Verificar se há processos em execução que podem se expandir para CPUs livres
    // Apenas para Round Robin quando não há processos na fila
    if (is_queue_empty(&system_state.ready_queue) && system_state.scheduler_type == ROUND_ROBIN) {
        for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
            PCB* running_process = system_state.current_process_array[cpu];
            if (running_process != NULL && running_process->state == RUNNING) {
                // Verificar se processo pode usar mais CPUs
                int current_cpus = 0;
                for (int i = 0; i < system_state.num_cpus; i++) {
                    if (system_state.current_process_array[i] == running_process) {
                        current_cpus++;
                    }
                }
                
                // Expandir para CPUs livres
                if (current_cpus < system_state.num_cpus) {
                    bool expanded = false;
                    
                    // Alocar CPUs livres
                    for (int free_cpu = 0; free_cpu < system_state.num_cpus; free_cpu++) {
                        if (system_state.current_process_array[free_cpu] == NULL) {
                            system_state.current_process_array[free_cpu] = running_process;
                            expanded = true;
                        }
                    }
                    
                    // Se houve expansão, logar todos os CPUs onde o processo agora está executando
                    if (expanded) {
                        for (int all_cpu = 0; all_cpu < system_state.num_cpus; all_cpu++) {
                            if (system_state.current_process_array[all_cpu] == running_process) {
                                snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                                        policy_names[system_state.scheduler_type], running_process->pid, THREAD_EXECUTION_TIME, all_cpu);
                                add_essential_log_message("%s\n", log_msg);
                            }
                        }
                    }
                }
                break; // Só processar um processo de cada vez
            }
        }
    }
    
    // Alocar novos processos para CPUs livres
    for (int cpu = 0; cpu < system_state.num_cpus; cpu++) {
        PCB* current_cpu_process = system_state.current_process_array[cpu];
        if (current_cpu_process == NULL) {
            PCB* process = NULL;
            
            // Selecionar processo baseado na política
            switch (system_state.scheduler_type) {
                case FCFS:
                    process = dequeue_process(&system_state.ready_queue);
                    break;
                case PRIORITY:
                    process = get_highest_priority_process(&system_state.ready_queue);
                    if (process != NULL) {
                        remove_process_from_queue(&system_state.ready_queue, process);
                    }
                    break;
                case ROUND_ROBIN:
                    process = dequeue_process(&system_state.ready_queue);
                    break;
            }

            if (process != NULL) {
                pthread_mutex_lock(&process->mutex);
                process->state = RUNNING;
                system_state.current_process_array[cpu] = process;
                
                if (system_state.scheduler_type == ROUND_ROBIN) {
                    snprintf(log_msg, 256, "[%s] Executando processo PID %d com quantum %dms // processador %d", 
                            policy_names[system_state.scheduler_type], process->pid, THREAD_EXECUTION_TIME, cpu);
                } else {
                    snprintf(log_msg, 256, "[%s] Executando processo PID %d // processador %d", 
                            policy_names[system_state.scheduler_type], process->pid, cpu);
                }
                add_essential_log_message("%s\n", log_msg);
                
                pthread_cond_broadcast(&process->cv);
                pthread_mutex_unlock(&process->mutex);
                
                // Se processo tem múltiplas threads, tentar usar próximo CPU livre também
                // EXCETO para Round Robin, que deve usar apenas um CPU por processo
                if (process->num_threads > 1 && system_state.scheduler_type != ROUND_ROBIN) {
                    for (int next_cpu = cpu + 1; next_cpu < system_state.num_cpus; next_cpu++) {
                        if (system_state.current_process_array[next_cpu] == NULL) {
                            system_state.current_process_array[next_cpu] = process;
                            
                            snprintf(log_msg, 256, "[%s] Executando processo PID %d // processador %d", 
                                    policy_names[system_state.scheduler_type], process->pid, next_cpu);
                            add_essential_log_message("%s\n", log_msg);
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
void* scheduler_thread_function(void* arg) {
    (void)arg; // Suprimir warning
    char log_msg[256];
    const char* policy_names[] = {"", "FCFS", "RR", "PRIORITY"};
    
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
        handle_multiprocessor_execution(policy_names, log_msg);
        
        usleep(50); // 0.05ms de intervalo - muito rápido para máxima responsividade
    }

    add_essential_log_message("Escalonador terminou execução de todos processos\n");
    return NULL;
}

#endif
