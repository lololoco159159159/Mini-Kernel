#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>

#include "structures.h"
#include "queue.h"

// Protótipos para testes
void test_basic_operations();
void test_priority_operations();
void test_concurrent_access();
void test_edge_cases();
void* producer_thread(void* arg);
void* consumer_thread(void* arg);

// Estrutura para testes concorrentes
typedef struct {
    ReadyQueue* queue;
    int thread_id;
    int operations;
} TestData;

// Variáveis globais para teste
ReadyQueue test_queue;
PCB test_processes[10];
int total_operations = 0;
pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

int main() {
    printf("=== TESTE DA FILA DE PROCESSOS PRONTOS ===\n\n");
    
    // Inicializa processos de teste
    for (int i = 0; i < 10; i++) {
        test_processes[i].pid = i + 1;
        test_processes[i].priority = (i % 5) + 1;  // Prioridades 1-5
        test_processes[i].process_len = 1000 + (i * 100);
        test_processes[i].remaining_time = test_processes[i].process_len;
        test_processes[i].state = READY;
        test_processes[i].num_threads = 1;
        test_processes[i].start_time = i * 100;
        
        pthread_mutex_init(&test_processes[i].mutex, NULL);
        pthread_cond_init(&test_processes[i].cv, NULL);
        test_processes[i].thread_ids = malloc(sizeof(pthread_t));
    }
    
    // Executa testes
    test_basic_operations();
    test_priority_operations();
    test_edge_cases();
    test_concurrent_access();
    
    // Limpa recursos
    for (int i = 0; i < 10; i++) {
        pthread_mutex_destroy(&test_processes[i].mutex);
        pthread_cond_destroy(&test_processes[i].cv);
        free(test_processes[i].thread_ids);
    }
    
    printf("=== TODOS OS TESTES PASSARAM! ===\n");
    return 0;
}

void test_basic_operations() {
    printf("1. Testando operações básicas da fila...\n");
    
    init_ready_queue(&test_queue);
    
    // Teste: fila vazia
    assert(is_queue_empty(&test_queue) == 1);
    assert(get_queue_size(&test_queue) == 0);
    assert(dequeue_process(&test_queue) == NULL);
    
    // Teste: inserir elementos
    enqueue_process(&test_queue, &test_processes[0]);
    enqueue_process(&test_queue, &test_processes[1]);
    enqueue_process(&test_queue, &test_processes[2]);
    
    assert(is_queue_empty(&test_queue) == 0);
    assert(get_queue_size(&test_queue) == 3);
    
    printf("   ✓ Inserção e verificação de tamanho\n");
    
    // Teste: FIFO (First In, First Out)
    PCB* pcb1 = dequeue_process(&test_queue);
    assert(pcb1 == &test_processes[0]);
    assert(get_queue_size(&test_queue) == 2);
    
    PCB* pcb2 = dequeue_process(&test_queue);
    assert(pcb2 == &test_processes[1]);
    assert(get_queue_size(&test_queue) == 1);
    
    printf("   ✓ Ordem FIFO correta\n");
    
    // Teste: remoção específica
    enqueue_process(&test_queue, &test_processes[3]);
    enqueue_process(&test_queue, &test_processes[4]);
    
    assert(remove_process_from_queue(&test_queue, &test_processes[2]) == 1);
    assert(get_queue_size(&test_queue) == 2);
    assert(remove_process_from_queue(&test_queue, &test_processes[0]) == 0); // Não está na fila
    
    printf("   ✓ Remoção específica\n");
    
    destroy_ready_queue(&test_queue);
    printf("   ✓ Operações básicas OK\n\n");
}

void test_priority_operations() {
    printf("2. Testando operações de prioridade...\n");
    
    init_ready_queue(&test_queue);
    
    // Adiciona processos com diferentes prioridades
    enqueue_process(&test_queue, &test_processes[0]); // Prioridade 1
    enqueue_process(&test_queue, &test_processes[3]); // Prioridade 4
    enqueue_process(&test_queue, &test_processes[1]); // Prioridade 2
    enqueue_process(&test_queue, &test_processes[4]); // Prioridade 5
    
    // Teste: busca por maior prioridade
    PCB* highest = get_highest_priority_process(&test_queue);
    assert(highest == &test_processes[0]); // Prioridade 1 (maior)
    
    printf("   ✓ Busca por maior prioridade\n");
    
    // Teste: remoção por prioridade
    PCB* removed = dequeue_highest_priority_process(&test_queue);
    assert(removed == &test_processes[0]);
    assert(get_queue_size(&test_queue) == 3);
    
    // Próximo de maior prioridade deve ser prioridade 2
    highest = get_highest_priority_process(&test_queue);
    assert(highest == &test_processes[1]);
    
    printf("   ✓ Remoção por prioridade\n");
    
    // Teste: inserção ordenada por prioridade
    destroy_ready_queue(&test_queue);
    init_ready_queue(&test_queue);
    
    enqueue_process_by_priority(&test_queue, &test_processes[4]); // Prioridade 5
    enqueue_process_by_priority(&test_queue, &test_processes[1]); // Prioridade 2
    enqueue_process_by_priority(&test_queue, &test_processes[3]); // Prioridade 4
    enqueue_process_by_priority(&test_queue, &test_processes[0]); // Prioridade 1
    
    // Deve estar ordenado: P1, P2, P4, P5
    PCB* p1 = dequeue_process(&test_queue);
    PCB* p2 = dequeue_process(&test_queue);
    PCB* p3 = dequeue_process(&test_queue);
    PCB* p4 = dequeue_process(&test_queue);
    
    assert(p1->priority <= p2->priority);
    assert(p2->priority <= p3->priority);
    assert(p3->priority <= p4->priority);
    
    printf("   ✓ Inserção ordenada por prioridade\n");
    
    destroy_ready_queue(&test_queue);
    printf("   ✓ Operações de prioridade OK\n\n");
}

void test_edge_cases() {
    printf("3. Testando casos extremos...\n");
    
    init_ready_queue(&test_queue);
    
    // Teste: operações em fila vazia
    assert(dequeue_process(&test_queue) == NULL);
    assert(get_highest_priority_process(&test_queue) == NULL);
    assert(dequeue_highest_priority_process(&test_queue) == NULL);
    assert(remove_process_from_queue(&test_queue, &test_processes[0]) == 0);
    assert(is_process_in_queue(&test_queue, &test_processes[0]) == 0);
    
    printf("   ✓ Operações em fila vazia\n");
    
    // Teste: um único elemento
    enqueue_process(&test_queue, &test_processes[0]);
    assert(get_queue_size(&test_queue) == 1);
    assert(get_highest_priority_process(&test_queue) == &test_processes[0]);
    assert(is_process_in_queue(&test_queue, &test_processes[0]) == 1);
    
    PCB* single = dequeue_process(&test_queue);
    assert(single == &test_processes[0]);
    assert(is_queue_empty(&test_queue) == 1);
    
    printf("   ✓ Fila com um elemento\n");
    
    // Teste: mesmo processo múltiplas vezes (não deveria acontecer, mas testa robustez)
    enqueue_process(&test_queue, &test_processes[0]);
    enqueue_process(&test_queue, &test_processes[1]);
    enqueue_process(&test_queue, &test_processes[0]); // Duplicado
    
    assert(get_queue_size(&test_queue) == 3);
    assert(remove_process_from_queue(&test_queue, &test_processes[0]) == 1);
    assert(get_queue_size(&test_queue) == 2);
    
    printf("   ✓ Manipulação de elementos duplicados\n");
    
    destroy_ready_queue(&test_queue);
    printf("   ✓ Casos extremos OK\n\n");
}

void test_concurrent_access() {
    printf("4. Testando acesso concorrente...\n");
    
    init_ready_queue(&test_queue);
    total_operations = 0;
    
    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;
    
    pthread_t threads[NUM_THREADS];
    TestData thread_data[NUM_THREADS];
    
    // Cria threads produtoras e consumidoras
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].queue = &test_queue;
        thread_data[i].thread_id = i;
        thread_data[i].operations = OPS_PER_THREAD;
        
        if (i % 2 == 0) {
            pthread_create(&threads[i], NULL, producer_thread, &thread_data[i]);
        } else {
            pthread_create(&threads[i], NULL, consumer_thread, &thread_data[i]);
        }
    }
    
    // Aguarda todas as threads terminarem
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("   ✓ %d operações concorrentes executadas\n", total_operations);
    printf("   ✓ Fila final: %d elementos\n", get_queue_size(&test_queue));
    
    destroy_ready_queue(&test_queue);
    printf("   ✓ Acesso concorrente OK\n\n");
}

void* producer_thread(void* arg) {
    TestData* data = (TestData*)arg;
    
    for (int i = 0; i < data->operations; i++) {
        int process_idx = (data->thread_id * data->operations + i) % 10;
        enqueue_process(data->queue, &test_processes[process_idx]);
        
        pthread_mutex_lock(&counter_mutex);
        total_operations++;
        pthread_mutex_unlock(&counter_mutex);
        
        usleep(1000); // 1ms
    }
    
    return NULL;
}

void* consumer_thread(void* arg) {
    TestData* data = (TestData*)arg;
    
    for (int i = 0; i < data->operations; i++) {
        PCB* pcb = dequeue_process(data->queue);
        
        if (pcb != NULL) {
            pthread_mutex_lock(&counter_mutex);
            total_operations++;
            pthread_mutex_unlock(&counter_mutex);
        }
        
        usleep(1500); // 1.5ms
    }
    
    return NULL;
}
