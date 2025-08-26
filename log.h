#ifndef LOG_H
#define LOG_H

#include "structures.h"

/**
 * Inicializa o sistema de log
 * Configura o buffer global para armazenar mensagens durante toda a simulação
 */
void init_log_system();

/**
 * Adiciona uma mensagem formatada ao buffer de log
 * Thread-safe através de mutex
 * @param format String de formato (como printf)
 * @param ... Argumentos variáveis
 */
void add_log_message(const char* format, ...);

/**
 * Adiciona uma mensagem formatada ao buffer essencial de log
 * Usado para logs de execução que devem aparecer no arquivo final
 * @param format String de formato (como printf)
 * @param ... Argumentos variáveis
 */
void add_essential_log_message(const char* format, ...);

/**
 * Adiciona mensagem de início do sistema
 */
void log_system_start();

/**
 * Adiciona mensagem de início de execução de processo
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo
 */
void log_process_start(const char* scheduler_name, int pid);

/**
 * Adiciona mensagem de início de execução de processo para Round Robin
 * @param pid PID do processo  
 * @param quantum Tempo do quantum em ms
 */
void log_process_start_rr(int pid, int quantum);
void log_process_start_priority(int pid, int priority);
void log_process_finish_priority(int pid);

// Funções para multiprocessador
void log_process_start_cpu(const char* scheduler_name, int pid, int cpu_id);
void log_process_start_rr_cpu(int pid, int quantum, int cpu_id);  
void log_process_start_priority_cpu(int pid, int priority, int cpu_id);

/**
 * Adiciona mensagem de finalização de processo
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo
 */
void log_process_finish(const char* scheduler_name, int pid);

/**
 * Adiciona mensagem de preempção de processo
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo preemptado
 */
void log_process_preempted(const char* scheduler_name, int pid);

/**
 * Adiciona mensagem de quantum expirado (Round Robin)
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo
 */
void log_quantum_expired(const char* scheduler_name, int pid);

/**
 * Adiciona mensagem de criação de processo
 * @param pid PID do processo criado
 * @param num_threads Número de threads do processo
 */
void log_process_created(int pid, int num_threads);

/**
 * Adiciona mensagem de fim do escalonador
 */
void log_scheduler_end();

/**
 * Salva o conteúdo completo do log em arquivo
 * DEVE ser chamado apenas no final da simulação
 * @param filename Nome do arquivo de saída (recomendado: "log_execucao_minikernel.txt")
 * @return 1 se sucesso, 0 se erro
 */
int save_log_to_file(const char* filename);

/**
 * Libera completamente a memória do sistema de log
 * Deve ser chamado no cleanup do sistema
 */
void cleanup_log_system();

/**
 * Retorna o nome da política de escalonamento
 * @param type Tipo de escalonamento
 * @return String com o nome da política
 */
const char* get_scheduler_name(SchedulerType type);

/**
 * Adiciona timestamp ao log (tempo em milissegundos desde o início)
 * @param message Mensagem a ser logada com timestamp
 */
void add_log_with_timestamp(const char* message);

#endif // LOG_H
