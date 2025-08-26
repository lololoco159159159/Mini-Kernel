#ifndef LOGGER_H
#define LOGGER_H

#include <sys/time.h>

// Funções de log
void add_to_log(const char* message);
void save_log_to_file();
long get_current_time_ms();

// Variáveis globais de tempo
extern struct timeval start_time_global;

#endif
