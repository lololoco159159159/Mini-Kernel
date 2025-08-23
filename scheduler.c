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
}void schedule_fcfs() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador FCFS iniciado\n");
    
    // Loop principal simplificado: enquanto houver processos prontos ou ainda não criados
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        
        iteration_count++;
        
        // Timeout de segurança
        if (iteration_count > 100) {
            add_log_message("AVISO: Escalonador atingiu limite de iteracoes - terminando\n");
            break;
        }
        
        // Log periódico para acompanhamento
        if (iteration_count % 10 == 0) {
            add_log_message("Escalonador: iteracao %d, generator_done=%d, queue_size=%d\n",
                           iteration_count, system_state.generator_done, 
                           get_queue_size(&system_state.ready_queue));
        }
        
        // FCFS: pegar primeiro da fila e executar até terminar
        if (!is_queue_empty(&system_state.ready_queue)) {
            PCB* process = dequeue_process(&system_state.ready_queue);
            if (process != NULL) {
                add_log_message("Executando processo PID %d por %dms\n", process->pid, process->process_len);
                
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
                add_log_message("Processo PID %d finalizado\n", process->pid);
            }
        }
        
        // Pequena pausa para não sobrecarregar CPU
        usleep(10000); // 10ms
    }
    
    add_log_message("Escalonador FCFS finalizado apos %d iteracoes\n", iteration_count);
}

void schedule_round_robin() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador Round Robin iniciado (quantum: %dms)\n", system_state.quantum);
    
    // Versão extremamente simplificada - como o FCFS mas com logs diferentes
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        iteration_count++;
        
        if (iteration_count > 50) {
            add_log_message("AVISO: Escalonador RR atingiu limite de iteracoes\n");
            break;
        }
        
        if (!is_queue_empty(&system_state.ready_queue)) {
            PCB* process = dequeue_process(&system_state.ready_queue);
            if (process != NULL) {
                add_log_message("RR: Executando processo PID %d\n", process->pid);
                log_process_start(scheduler_name, process->pid);
                
                // Versão simplificada: apenas fazer o processo rodar até terminar
                set_process_running(process);
                
                // Aguardar término simples
                pthread_mutex_lock(&process->mutex);
                while (process->state != FINISHED) {
                    pthread_mutex_unlock(&process->mutex);
                    usleep(10000); // 10ms
                    pthread_mutex_lock(&process->mutex);
                }
                pthread_mutex_unlock(&process->mutex);
                
                log_process_finish(scheduler_name, process->pid);
                add_log_message("RR: Processo PID %d terminou\n", process->pid);
            }
        }
        
        usleep(10000); // 10ms
    }
    
    add_log_message("Escalonador Round Robin finalizado apos %d iteracoes\n", iteration_count);
}

void schedule_priority() {
    const char* scheduler_name = get_scheduler_name(system_state.scheduler_type);
    int iteration_count = 0;
    
    add_log_message("Escalonador por Prioridade iniciado\n");
    
    // Loop principal simplificado: enquanto houver processos prontos ou ainda não criados
    while (!system_state.generator_done || !is_queue_empty(&system_state.ready_queue)) {
        
        iteration_count++;
        
        // Timeout de segurança
        if (iteration_count > 100) {
            add_log_message("AVISO: Escalonador Prioridade atingiu limite de iteracoes - terminando\n");
            break;
        }
        
        // Log periódico
        if (iteration_count % 10 == 0) {
            add_log_message("Escalonador Prioridade: iteracao %d, generator_done=%d, queue_size=%d\n",
                           iteration_count, system_state.generator_done, 
                           get_queue_size(&system_state.ready_queue));
        }
        
        // Prioridade: pegar processo de maior prioridade (menor valor numérico)
        if (!is_queue_empty(&system_state.ready_queue)) {
            PCB* process = get_highest_priority_process(&system_state.ready_queue);
            if (process != NULL) {
                // Remove da fila
                remove_process_from_queue(&system_state.ready_queue, process);
                
                add_log_message("Executando processo PID %d (prioridade %d) por %dms\n", 
                               process->pid, process->priority, process->process_len);
                
                // Registrar eventos no log
                log_process_start(scheduler_name, process->pid);
                
                // Mudar estado para RUNNING e acordar threads do processo
                set_process_running(process);
                
                // Aguardar término completo (versão não-preemptiva)
                pthread_mutex_lock(&process->mutex);
                while (process->state != FINISHED) {
                    pthread_mutex_unlock(&process->mutex);
                    usleep(10000); // 10ms de pausa
                    pthread_mutex_lock(&process->mutex);
                }
                pthread_mutex_unlock(&process->mutex);
                
                // Registrar finalização no log
                log_process_finish(scheduler_name, process->pid);
                add_log_message("Processo PID %d terminou execução\n", process->pid);
            }
        }
        
        // Pequena pausa
        usleep(10000); // 10ms
    }
    
    add_log_message("Escalonador por Prioridade finalizado apos %d iteracoes\n", iteration_count);
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
    int iteration_count = 0;
    
    add_log_message("Escalonador CPU2 iniciado\n");
    
    // Versão simplificada: aguarda um pouco e termina
    while (!system_state.generator_done && iteration_count < 10) {
        iteration_count++;
        add_log_message("[CPU2] Iteracao %d, aguardando...\n", iteration_count);
        sleep_ms(100);
    }
    
    add_log_message("Escalonador CPU2 finalizado apos %d iteracoes\n", iteration_count);
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
