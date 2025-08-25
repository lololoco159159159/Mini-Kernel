#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <time.h>

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
int create_process_threads(PCB* pcb);
void init_system();
void cleanup_system();
void wait_for_all_threads();
void cleanup_pcb_list(int count);

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
    int quantum = THREAD_EXECUTION_TIME;
    if (system_state.scheduler_type == ROUND_ROBIN) {
        quantum = 500; // Quantum para Round Robin (500ms)
        add_log_message("CONFIGURANDO QUANTUM RR: %dms\n", quantum);
    }
    init_scheduler(system_state.scheduler_type, quantum);
    
    // Cria a thread geradora de processos
    pthread_t generator_thread;
    if (pthread_create(&generator_thread, NULL, process_generator_thread, NULL) != 0) {
        fprintf(stderr, "Erro ao criar thread geradora de processos\n");
        cleanup_system();
        return 1;
    }
    
    // Cria a thread do escalonador
    pthread_t scheduler_thread_id;
#ifdef MONO
    if (pthread_create(&scheduler_thread_id, NULL, scheduler_thread, NULL) != 0) {
        fprintf(stderr, "Erro ao criar thread do escalonador\n");
        cleanup_system();
        return 1;
    }
#else
    // No modo multiprocessador, cria threads para cada CPU
    static int cpu1_id = 0, cpu2_id = 1;
    pthread_t scheduler_thread_cpu2_id;
    
    if (pthread_create(&scheduler_thread_id, NULL, scheduler_thread_cpu, &cpu1_id) != 0) {
        fprintf(stderr, "Erro ao criar thread do escalonador CPU 1\n");
        cleanup_system();
        return 1;
    }
    
    if (pthread_create(&scheduler_thread_cpu2_id, NULL, scheduler_thread_cpu, &cpu2_id) != 0) {
        fprintf(stderr, "Erro ao criar thread do escalonador CPU 2\n");
        cleanup_system();
        return 1;
    }
#endif
    
    // Aguarda as threads principais terminarem (com timeout)
    add_log_message("Aguardando threads terminarem...\n");
    
    // Aguarda as threads principais terminarem
    add_log_message("Aguardando threads terminarem...\n");
    
    pthread_join(generator_thread, NULL);
    add_log_message("Thread geradora terminou\n");
    
    pthread_join(scheduler_thread_id, NULL);
    add_log_message("Thread escalonador terminou\n");
    
#ifdef MULTI
    pthread_join(scheduler_thread_cpu2_id, NULL);
    add_log_message("Segunda thread escalonador terminou\n");
#endif
    
    add_log_message("Todas as threads principais terminaram\n");
    
    // Passo 9: Garantir término de todas as threads dos processos (pthread_join)
    wait_for_all_threads();
    
    // Estatísticas finais
    add_log_message("\n=== ESTATISTICAS FINAIS ===\n");
    add_log_message("Total de processos: %d\n", system_state.process_count);
    add_log_message("Politica de escalonamento: %s\n", get_scheduler_name(system_state.scheduler_type));
    add_log_message("=== FIM DA SIMULACAO ===\n");
    
    // Passo 9: Salvar log no arquivo
    save_log_to_file("log_execucao_minikernel.txt");
    
    // Passo 9: Liberar memória alocada e destruir mutexes/variáveis de condição
    cleanup_system();
    
    return 0;
}

int read_input_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        add_log_message("ERRO: Nao foi possivel abrir arquivo de entrada: %s\n", filename);
        return 0;
    }
    
    add_log_message("Iniciando leitura do arquivo: %s\n", filename);
    
    // Lê número de processos
    if (fscanf(file, "%d", &system_state.process_count) != 1 || system_state.process_count <= 0) {
        add_log_message("ERRO: Formato invalido - numero de processos\n");
        fclose(file);
        return 0;
    }
    
    add_log_message("Numero de processos a serem criados: %d\n", system_state.process_count);
    
    // Valida limite máximo de processos
    if (system_state.process_count > MAX_PROCESSES) {
        add_log_message("ERRO: Numero de processos excede limite maximo (%d)\n", MAX_PROCESSES);
        fclose(file);
        return 0;
    }
    
    // Aloca memória para a lista de processos
    system_state.pcb_list = malloc(system_state.process_count * sizeof(PCB));
    if (system_state.pcb_list == NULL) {
        add_log_message("ERRO: Falha ao alocar memoria para lista de PCBs\n");
        fclose(file);
        return 0;
    }
    
    // Lê informações de cada processo
    for (int i = 0; i < system_state.process_count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        
        // Inicializa PID sequencial
        pcb->pid = i + 1;
        
        // Lê duração do processo
        if (fscanf(file, "%d", &pcb->process_len) != 1 || pcb->process_len <= 0) {
            add_log_message("ERRO: Formato invalido - duracao do processo %d\n", pcb->pid);
            cleanup_pcb_list(i);
            fclose(file);
            return 0;
        }
        
        // Lê prioridade (1 = maior, 5 = menor)
        if (fscanf(file, "%d", &pcb->priority) != 1 || pcb->priority < 1 || pcb->priority > 5) {
            add_log_message("ERRO: Formato invalido - prioridade do processo %d (deve ser 1-5)\n", pcb->pid);
            cleanup_pcb_list(i);
            fclose(file);
            return 0;
        }
        
        // Lê número de threads
        if (fscanf(file, "%d", &pcb->num_threads) != 1 || pcb->num_threads <= 0) {
            add_log_message("ERRO: Formato invalido - numero de threads do processo %d\n", pcb->pid);
            cleanup_pcb_list(i);
            fclose(file);
            return 0;
        }
        
        // Lê tempo de chegada
        if (fscanf(file, "%d", &pcb->start_time) != 1 || pcb->start_time < 0) {
            add_log_message("ERRO: Formato invalido - tempo de chegada do processo %d\n", pcb->pid);
            cleanup_pcb_list(i);
            fclose(file);
            return 0;
        }
        
        // Inicializa campos dinâmicos do PCB
        pcb->remaining_time = pcb->process_len;
        pcb->state = READY;
        
        // Inicializa mecanismos de sincronização
        if (pthread_mutex_init(&pcb->mutex, NULL) != 0) {
            add_log_message("ERRO: Falha ao inicializar mutex do processo %d\n", pcb->pid);
            cleanup_pcb_list(i); // Limpa PCBs de 0 até i-1
            fclose(file);
            return 0;
        }
        
        if (pthread_cond_init(&pcb->cv, NULL) != 0) {
            add_log_message("ERRO: Falha ao inicializar variavel de condicao do processo %d\n", pcb->pid);
            pthread_mutex_destroy(&pcb->mutex); // Destroy o mutex que acabou de ser criado
            cleanup_pcb_list(i); // Limpa PCBs de 0 até i-1
            fclose(file);
            return 0;
        }
        
        // Inicializar thread_ids como NULL - será alocado em create_process_threads
        pcb->thread_ids = NULL;
        
        // Log da criação bem-sucedida do PCB
        add_log_message("PCB criado - PID: %d, Duracao: %dms, Prioridade: %d, Threads: %d, Chegada: %dms\n",
                       pcb->pid, pcb->process_len, pcb->priority, pcb->num_threads, pcb->start_time);
    }
    
    // Lê política de escalonamento
    int scheduler_type_int;
    if (fscanf(file, "%d", &scheduler_type_int) != 1) {
        add_log_message("ERRO: Formato invalido - politica de escalonamento\n");
        cleanup_pcb_list(system_state.process_count);
        fclose(file);
        return 0;
    }
    
    // Valida política de escalonamento
    if (scheduler_type_int < 1 || scheduler_type_int > 3) {
        add_log_message("ERRO: Politica de escalonamento invalida: %d (deve ser 1=FCFS, 2=RR, 3=PRIORIDADE)\n", 
                       scheduler_type_int);
        cleanup_pcb_list(system_state.process_count);
        fclose(file);
        return 0;
    }
    
    system_state.scheduler_type = (SchedulerType)scheduler_type_int;
    
    add_log_message("Politica de escalonamento: %s (%d)\n", 
                   get_scheduler_name(system_state.scheduler_type), scheduler_type_int);
    
    fclose(file);
    
    add_log_message("Leitura do arquivo concluida com sucesso\n");
    add_log_message("Total de PCBs inicializados: %d\n", system_state.process_count);
    
    return 1;
}

void* process_thread_function(void* arg) {
    TCB* tcb = (TCB*)arg;
    PCB* pcb = tcb->pcb;
    
    // add_log_message("Thread %d do processo PID %d iniciada\n", thread_index, pcb->pid);
    
    while (1) {
        pthread_mutex_lock(&pcb->mutex);
        
        // Aguarda sinal do escalonador enquanto estado != RUNNING e != FINISHED
        while (pcb->state != RUNNING && pcb->state != FINISHED) {
            pthread_cond_wait(&pcb->cv, &pcb->mutex);
        }
        
        // Verifica se o processo foi finalizado
        if (pcb->state == FINISHED) {
            pthread_mutex_unlock(&pcb->mutex);
            // add_log_message("Thread %d do processo PID %d terminou (estado FINISHED)\n", 
            //                thread_index, pcb->pid);
            break;
        }
        
        // Verifica se ainda há tempo restante para execução
        if (pcb->remaining_time <= 0) {
            // Processo já terminou, sinaliza outras threads
            pcb->state = FINISHED;
            pthread_cond_broadcast(&pcb->cv);
            pthread_mutex_unlock(&pcb->mutex);
            // add_log_message("Thread %d do processo PID %d detectou fim de execução\n", 
            //                thread_index, pcb->pid);
            break;
        }
        
        pthread_mutex_unlock(&pcb->mutex);
        
        // Simula execução por 500ms (conforme especificação)
        usleep(500000); // 500ms = 500.000 microssegundos
        
        // Decrementa remaining_time de forma segura
        pthread_mutex_lock(&pcb->mutex);
        
        if (pcb->remaining_time > 0) {
            pcb->remaining_time -= 500; // Decrementa 500ms
            
            // add_log_message("Thread %d do processo PID %d executou 500ms (restante: %dms)\n",
            //                thread_index, pcb->pid, pcb->remaining_time);
            
            // Se remaining_time <= 0, muda estado para FINISHED e sinaliza todas as threads
            if (pcb->remaining_time <= 0) {
                pcb->remaining_time = 0;
                pcb->state = FINISHED;
                pthread_cond_broadcast(&pcb->cv); // Acorda todas as threads do processo
                
                // add_log_message("Thread %d do processo PID %d finalizou execução completa\n", 
                //                thread_index, pcb->pid);
                pthread_mutex_unlock(&pcb->mutex);
                break;
            }
        }
        
        pthread_mutex_unlock(&pcb->mutex);
    }
    
    // add_log_message("Thread %d do processo PID %d terminada\n", thread_index, pcb->pid);
    free(tcb); // Libera a estrutura TCB
    return NULL;
}

int create_process_threads(PCB* pcb) {
    if (pcb == NULL || pcb->num_threads <= 0) {
        return 0;
    }
    
    // Aloca vetor para IDs das threads
    pcb->thread_ids = malloc(pcb->num_threads * sizeof(pthread_t));
    if (pcb->thread_ids == NULL) {
        add_log_message("ERRO: Falha ao alocar memoria para threads do processo PID %d\n", pcb->pid);
        return 0;
    }
    
    // Cria cada thread do processo
    for (int i = 0; i < pcb->num_threads; i++) {
        // Aloca e inicializa TCB para a thread
        TCB* tcb = malloc(sizeof(TCB));
        if (tcb == NULL) {
            add_log_message("ERRO: Falha ao alocar TCB para thread %d do processo PID %d\n", i, pcb->pid);
            // Limpa threads já criadas
            for (int j = 0; j < i; j++) {
                pthread_cancel(pcb->thread_ids[j]);
            }
            free(pcb->thread_ids);
            pcb->thread_ids = NULL;
            return 0;
        }
        
        tcb->pcb = pcb;
        tcb->thread_index = i;
        
        // Cria a thread
        if (pthread_create(&pcb->thread_ids[i], NULL, process_thread_function, tcb) != 0) {
            add_log_message("ERRO: Falha ao criar thread %d do processo PID %d\n", i, pcb->pid);
            free(tcb);
            // Limpa threads já criadas
            for (int j = 0; j < i; j++) {
                pthread_cancel(pcb->thread_ids[j]);
            }
            free(pcb->thread_ids);
            pcb->thread_ids = NULL;
            return 0;
        }
    }
    
    add_log_message("Processo PID %d criado com %d threads\n", pcb->pid, pcb->num_threads);
    return 1;
}

void* process_generator_thread(void* arg) {
    (void)arg; // Suprime warning
    
    add_log_message("Thread geradora iniciada\n");
    
    // Array para controlar quais processos já foram criados
    int* process_created = calloc(system_state.process_count, sizeof(int));
    if (process_created == NULL) {
        add_log_message("ERRO: Falha ao alocar memoria para controle de processos\n");
        system_state.generator_done = 1;
        return NULL;
    }
    
    int processes_remaining = system_state.process_count;
    int iterations = 0;
    
    // Loop principal: monitora tempo e cria processos conforme chegada
    while (processes_remaining > 0) {
        long current_time = get_current_time_ms();
        iterations++;
        
        // Timeout de segurança
        if (iterations > 10000) {
            add_log_message("Thread geradora terminando forcadamente\n");
            break;
        }
        
        // Verifica se algum processo deve ser criado neste momento
        for (int i = 0; i < system_state.process_count; i++) {
            if (!process_created[i]) {
                PCB* pcb = &system_state.pcb_list[i];
                
                // Chegou o tempo de criar este processo?
                if (current_time >= pcb->start_time) {
                    add_log_message("Criando processo PID %d (tempo: %ldms, chegada: %dms)\n", 
                                   pcb->pid, current_time, pcb->start_time);
                    
                    // Cria as threads do processo
                    if (create_process_threads(pcb)) {
                        log_process_created(pcb->pid, pcb->num_threads);
                        
                        // Adiciona o processo à fila de prontos
                        enqueue_process(&system_state.ready_queue, pcb);
                        add_log_message("Processo PID %d adicionado a fila de prontos\n", pcb->pid);
                        
                        process_created[i] = 1;
                        processes_remaining--;
                    } else {
                        add_log_message("ERRO: Falha ao criar threads do processo PID %d\n", pcb->pid);
                        // IMPORTANTE: Se falhou, o thread_ids já foi liberado em create_process_threads
                        // Não precisa fazer nada aqui
                    }
                }
            }
        }
        
        // Pequena pausa para não consumir CPU desnecessariamente
        usleep(10000); // 10ms
    }
    
    // Libera memória e sinaliza conclusão
    free(process_created);
    system_state.generator_done = 1;
    add_log_message("Thread geradora finalizou - todos os processos criados\n");
    
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
    // Liberar memória alocada (vetores de threads, lista de PCBs, filas)
    if (system_state.pcb_list != NULL) {
        
        for (int i = 0; i < system_state.process_count; i++) {
            PCB* pcb = &system_state.pcb_list[i];
            
            // Libera vetor de IDs das threads
            if (pcb->thread_ids != NULL) {
                free(pcb->thread_ids);
                pcb->thread_ids = NULL;
            }
            
            // Destruir mutexes e variáveis de condição
            pthread_mutex_destroy(&pcb->mutex);
            pthread_cond_destroy(&pcb->cv);
        }
        
        // Libera lista de PCBs
        free(system_state.pcb_list);
        system_state.pcb_list = NULL;
    }
    
    // Limpa escalonador
    cleanup_scheduler();
    
    // Limpa fila de prontos
    destroy_ready_queue(&system_state.ready_queue);
    
    // Limpa sistema de log (deve ser por último)
    cleanup_log_system();
}

void wait_for_all_threads() {
    if (system_state.pcb_list == NULL) return;
    
    add_log_message("=== AGUARDANDO TÉRMINO DE TODAS AS THREADS DOS PROCESSOS ===\n");
    
    for (int i = 0; i < system_state.process_count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        
        add_log_message("Aguardando %d threads do processo PID %d...\n", pcb->num_threads, pcb->pid);
        
        // Garantir término de todas as threads do processo (pthread_join)
        for (int j = 0; j < pcb->num_threads; j++) {
            if (pthread_join(pcb->thread_ids[j], NULL) == 0) {
                add_log_message("Thread %d do processo PID %d finalizada com sucesso\n", j, pcb->pid);
            } else {
                add_log_message("AVISO: Falha ao aguardar thread %d do processo PID %d\n", j, pcb->pid);
            }
        }
        
        add_log_message("Todas as threads do processo PID %d finalizaram\n", pcb->pid);
    }
    
    add_log_message("Todos os processos finalizaram execucao\n");
}

void cleanup_pcb_list(int count) {
    if (system_state.pcb_list == NULL) return;
    
    // Limpa recursos dos PCBs já inicializados
    for (int i = 0; i < count; i++) {
        PCB* pcb = &system_state.pcb_list[i];
        
        // Destroi mutex e variável de condição
        pthread_mutex_destroy(&pcb->mutex);
        pthread_cond_destroy(&pcb->cv);
        
        // Libera vetor de thread IDs
        if (pcb->thread_ids != NULL) {
            free(pcb->thread_ids);
        }
    }
    
    // Libera a lista de PCBs
    free(system_state.pcb_list);
    system_state.pcb_list = NULL;
    system_state.process_count = 0;
}