#include "scheduler.h"
#include "process_manager.h"
#include "logger.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Uso: %s <arquivo_entrada>\n", argv[0]);
        return 1;
    }
    
    // Determinar número de CPUs
    int num_cpus = 1;
    #ifdef MULTIPROCESSADOR
    num_cpus = 2;
    #endif
    
    // Inicializar
    gettimeofday(&start_time_global, NULL);
    initialize_scheduler(num_cpus);
    read_input(argv[1]);
    
    // Criar threads
    pthread_t generator_thread, scheduler_thread;
    
    pthread_create(&generator_thread, NULL, process_generator_thread_function, NULL);
    pthread_create(&scheduler_thread, NULL, scheduler_thread_function, NULL);
    
    // Aguardar threads terminarem
    pthread_join(generator_thread, NULL);
    pthread_join(scheduler_thread, NULL);
    
    // Finalizar
    save_log_to_file();
    cleanup_resources();
    
    return 0;
}
