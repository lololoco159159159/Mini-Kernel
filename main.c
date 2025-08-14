#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>

#include "structures.h"
#include "scheduler.h"
#include "queue.h"
#include "log.h"

// Variável global do sistema
SystemState system_state;

// Protótipos de funções
int read_input_file(const char* filename);
void* process_thread_function(void* arg);
void* process_generator_thread(void* arg);
void init_system();
void cleanup_system();
void wait_for_all_threads();

int main(int argc, char* argv[]) {
    // Verifica argumentos da linha de comando
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }
    
    // Inicializa o sistema
    init_system();
    
    // Lê o arquivo de entrada
    if (!read_input_file(argv[1])) {
        fprintf(stderr, "Erro ao ler arquivo de entrada: %s\n", argv[1]);
        cleanup_system();
        return 1;
    }
    
    // Inicializa o escalonador
    init_scheduler(system_state.scheduler_type, THREAD_EXECUTION_TIME);
    
    // Cria a thread geradora de processos
    pthread_t generator_thread;
    if (pthread_create(&generator_thread, NULL, process_generator_thread, NULL) != 0) {
        fprintf(stderr, "Erro ao criar thread geradora de processos\n");
        cleanup_system();
        return 1;
    }
    
    // Cria a thread do escalonador
    pthread_t scheduler_thread_id;
    if (pthread_create(&scheduler_thread_id, NULL, scheduler_thread, NULL) != 0) {
        fprintf(stderr, "Erro ao criar thread do escalonador\n");
        cleanup_system();
        return 1;
    }
    
#ifdef MULTI
    // Cria a segunda thread do escalonador para multiprocessador
    pthread_t scheduler_thread_cpu2_id;
    if (pthread_create(&scheduler_thread_cpu2_id, NULL, scheduler_thread_cpu2, NULL) != 0) {
        fprintf(stderr, "Erro ao criar segunda thread do escalonador\n");
        cleanup_system();
        return 1;
    }
#endif
    
    // Aguarda as threads principais terminarem
    pthread_join(generator_thread, NULL);
    pthread_join(scheduler_thread_id, NULL);
    
#ifdef MULTI
    pthread_join(scheduler_thread_cpu2_id, NULL);
#endif
    
    // Aguarda todas as threads dos processos terminarem
    wait_for_all_threads();
    
    // Salva o log em arquivo
    save_log_to_file("log_execucao_minikernel.txt");
    
    // Limpa recursos do sistema
    cleanup_system();
    
    return 0;
}

int read_input_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        return 0;
    }
    
    // Lê número de processos
    if (fscanf(file, "%d", &system_state.process_count) != 1) {
        fclose(file);
        return 0;
    }
    
    // Aloca memória para a lista de processos
    system_state.pcb_list = malloc(system_state.process_count * sizeof(PCB));
    if (system_state.pcb_list == NULL) {
        fclose(file);
        return 0;
    }
    
    // Lê informações de cada processo
    for (int i = 0; i < system_state.process_count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        
        pcb->pid = i + 1; // PID sequencial começando em 1
        
        if (fscanf(file, "%d %d %d %d", 
                   &pcb->process_len, 
                   &pcb->priority, 
                   &pcb->num_threads, 
                   &pcb->start_time) != 4) {
            free(system_state.pcb_list);
            fclose(file);
            return 0;
        }
        
        // Inicializa campos do PCB
        pcb->remaining_time = pcb->process_len;
        pcb->state = READY;
        
        // Inicializa mutex e variável de condição
        pthread_mutex_init(&pcb->mutex, NULL);
        pthread_cond_init(&pcb->cv, NULL);
        
        // Aloca memória para os IDs das threads
        pcb->thread_ids = malloc(pcb->num_threads * sizeof(pthread_t));
        if (pcb->thread_ids == NULL) {
            // Limpa recursos já alocados
            for (int j = 0; j <= i; j++) {
                if (j < i || pcb->thread_ids == NULL) {
                    pthread_mutex_destroy(&system_state.pcb_list[j].mutex);
                    pthread_cond_destroy(&system_state.pcb_list[j].cv);
                    if (system_state.pcb_list[j].thread_ids != NULL) {
                        free(system_state.pcb_list[j].thread_ids);
                    }
                }
            }
            free(system_state.pcb_list);
            fclose(file);
            return 0;
        }
    }
    
    // Lê política de escalonamento
    int scheduler_type_int;
    if (fscanf(file, "%d", &scheduler_type_int) != 1) {
        // Limpa recursos
        for (int i = 0; i < system_state.process_count; i++) {
            pthread_mutex_destroy(&system_state.pcb_list[i].mutex);
            pthread_cond_destroy(&system_state.pcb_list[i].cv);
            free(system_state.pcb_list[i].thread_ids);
        }
        free(system_state.pcb_list);
        fclose(file);
        return 0;
    }
    
    system_state.scheduler_type = (SchedulerType)scheduler_type_int;
    
    fclose(file);
    return 1;
}

void* process_thread_function(void* arg) {
    TCB* tcb = (TCB*)arg;
    PCB* pcb = tcb->pcb;
    // int thread_index = tcb->thread_index; // Variável não utilizada no momento
    
    while (1) {
        pthread_mutex_lock(&pcb->mutex);
        
        // Aguarda até o processo estar em execução
        while (pcb->state != RUNNING && pcb->state != FINISHED) {
            pthread_cond_wait(&pcb->cv, &pcb->mutex);
        }
        
        // Verifica se o processo terminou
        if (pcb->state == FINISHED) {
            pthread_mutex_unlock(&pcb->mutex);
            break;
        }
        
        // Simula execução por um pequeno período
        pthread_mutex_unlock(&pcb->mutex);
        usleep(THREAD_EXECUTION_TIME * 1000); // 500ms
        
        // Atualiza tempo restante de forma segura
        pthread_mutex_lock(&pcb->mutex);
        
        if (pcb->remaining_time > 0) {
            pcb->remaining_time -= THREAD_EXECUTION_TIME;
            
            if (pcb->remaining_time <= 0) {
                pcb->remaining_time = 0;
                pcb->state = FINISHED;
                pthread_cond_broadcast(&pcb->cv); // Sinaliza outras threads
            }
        }
        
        pthread_mutex_unlock(&pcb->mutex);
    }
    
    free(tcb); // Libera a estrutura TCB
    return NULL;
}

void* process_generator_thread(void* arg) {
    (void)arg; // Suprime warning
    
    // long start_time = get_current_time_ms(); // Não utilizado no momento
    
    for (int i = 0; i < system_state.process_count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        
        // Aguarda o tempo de chegada do processo
        while (get_current_time_ms() < pcb->start_time) {
            usleep(10000); // 10ms
        }
        
        // Cria as threads do processo
        for (int j = 0; j < pcb->num_threads; j++) {
            TCB* tcb = malloc(sizeof(TCB));
            if (tcb == NULL) {
                fprintf(stderr, "Erro ao alocar TCB para thread %d do processo %d\n", j, pcb->pid);
                continue;
            }
            
            tcb->pcb = pcb;
            tcb->thread_index = j;
            
            if (pthread_create(&pcb->thread_ids[j], NULL, process_thread_function, tcb) != 0) {
                fprintf(stderr, "Erro ao criar thread %d do processo %d\n", j, pcb->pid);
                free(tcb);
            }
        }
        
        // Adiciona o processo à fila de prontos
        enqueue_process(&system_state.ready_queue, pcb);
    }
    
    // Sinaliza que todos os processos foram criados
    system_state.generator_done = 1;
    
    return NULL;
}

void init_system() {
    // Inicializa estruturas globais
    memset(&system_state, 0, sizeof(SystemState));
    
    // Inicializa fila de prontos
    init_ready_queue(&system_state.ready_queue);
    
    // Inicializa sistema de log
    init_log_system();
}

void cleanup_system() {
    // Aguarda e limpa threads dos processos
    if (system_state.pcb_list != NULL) {
        for (int i = 0; i < system_state.process_count; i++) {
            PCB* pcb = &system_state.pcb_list[i];
            
            // Libera recursos do PCB
            if (pcb->thread_ids != NULL) {
                free(pcb->thread_ids);
            }
            
            pthread_mutex_destroy(&pcb->mutex);
            pthread_cond_destroy(&pcb->cv);
        }
        
        free(system_state.pcb_list);
    }
    
    // Limpa fila de prontos
    destroy_ready_queue(&system_state.ready_queue);
    
    // Limpa sistema de log
    cleanup_log_system();
    
    // Limpa escalonador
    cleanup_scheduler();
}

void wait_for_all_threads() {
    if (system_state.pcb_list == NULL) return;
    
    for (int i = 0; i < system_state.process_count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        
        for (int j = 0; j < pcb->num_threads; j++) {
            pthread_join(pcb->thread_ids[j], NULL);
        }
    }
}