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
void init_system();
void cleanup_system();

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
    
    // Log inicial do sistema
    log_system_start();
    
    // Inicializa o escalonador
    init_scheduler(system_state.scheduler_type, THREAD_EXECUTION_TIME);
    
    // DEMONSTRAÇÃO DO SISTEMA DE LOG (Passo 4)
    add_log_message("=== DEMONSTRACAO DO SISTEMA DE LOG ===\n");
    
    // Simula criação de processos
    for (int i = 0; i < system_state.process_count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        log_process_created(pcb->pid, pcb->num_threads);
        
        // Simula início de execução
        log_process_start(get_scheduler_name(system_state.scheduler_type), pcb->pid);
        
        // Simula algumas operações baseadas no tipo de escalonador
        switch (system_state.scheduler_type) {
            case ROUND_ROBIN:
                log_quantum_expired(get_scheduler_name(system_state.scheduler_type), pcb->pid);
                break;
            case PRIORITY:
                if (i < system_state.process_count - 1) {
                    log_process_preempted(get_scheduler_name(system_state.scheduler_type), pcb->pid);
                }
                break;
            default:
                break;
        }
        
        // Simula finalização
        log_process_finish(get_scheduler_name(system_state.scheduler_type), pcb->pid);
    }
    
    // Log de finalização do escalonador
    log_scheduler_end();
    
    // Adiciona algumas mensagens de teste com timestamp
    add_log_with_timestamp("Sistema funcionando corretamente\n");
    add_log_with_timestamp("Buffer de log preenchido com sucesso\n");
    
    add_log_message("=== TESTE DO BUFFER GLOBAL CONCLUIDO ===\n");
    add_log_message("Total de mensagens processadas: %d\n", system_state.process_count * 3);
    
    // Salva o log completo em arquivo (OBRIGATÓRIO - nenhum printf no terminal)
    if (save_log_to_file("log_execucao_minikernel.txt")) {
        // Sucesso silencioso - sem printf no terminal
    } else {
        fprintf(stderr, "Erro ao salvar log\n");
    }
    
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
            // Libera recursos do PCB
            if (system_state.pcb_list[i].thread_ids != NULL) {
                free(system_state.pcb_list[i].thread_ids);
            }
            
            pthread_mutex_destroy(&system_state.pcb_list[i].mutex);
            pthread_cond_destroy(&system_state.pcb_list[i].cv);
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
