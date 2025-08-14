#ifndef LOG_H
#define LOG_H

#include "structures.h"

/**
 * Inicializa o sistema de log
 */
void init_log_system();

/**
 * Adiciona uma mensagem ao buffer de log
 * @param format String de formato (como printf)
 * @param ... Argumentos variáveis
 */
void add_log_message(const char* format, ...);

/**
 * Adiciona uma mensagem específica de início de execução de processo
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo
 */
void log_process_start(const char* scheduler_name, int pid);

/**
 * Adiciona uma mensagem específica de finalização de processo
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo
 */
void log_process_finish(const char* scheduler_name, int pid);

/**
 * Adiciona uma mensagem específica de preempção de processo
 * @param scheduler_name Nome da política de escalonamento
 * @param pid PID do processo preemptado
 */
void log_process_preempted(const char* scheduler_name, int pid);

/**
 * Adiciona uma mensagem de fim do escalonador
 */
void log_scheduler_end();

/**
 * Salva o conteúdo do log em arquivo
 * @param filename Nome do arquivo de saída
 * @return 1 se sucesso, 0 se erro
 */
int save_log_to_file(const char* filename);

/**
 * Libera a memória do sistema de log
 */
void cleanup_log_system();

/**
 * Retorna o nome da política de escalonamento
 * @param type Tipo de escalonamento
 * @return String com o nome da política
 */
const char* get_scheduler_name(SchedulerType type);

#endif // LOG_H
