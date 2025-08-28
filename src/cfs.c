
#include "../lib/cfs.h"
#include "../lib/structures.h"
#include "../lib/rbtree.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

// Estrutura principal do CFS - Thread-safe, usando apenas Red-Black Tree
typedef struct {
    PCB* rb_root;                    // Raiz da Red-Black Tree
    uint64_t min_vruntime;           // Menor vruntime na árvore
    uint64_t total_weight;           // Peso total dos processos
    int nr_running;                  // Número de processos executando
    pthread_mutex_t cfs_mutex;       // Mutex para thread safety
    int is_initialized;              // Flag de inicialização
} CFSRunQueue;

// Instância única do CFS
static CFSRunQueue cfs_rq = {
    .rb_root = NULL,
    .min_vruntime = 0,
    .total_weight = 0,
    .nr_running = 0,
    .cfs_mutex = PTHREAD_MUTEX_INITIALIZER,
    .is_initialized = 0
};

// Instância única do CFS


/**
 * Tabela de pesos por prioridade (baseada no kernel Linux)
 */
static const int prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548,  7620,  6100,  4904,  3906,
    /*  -5 */ 3121,  2501,  1991,  1586,  1277,
    /*   0 */ 1024,   820,   655,   526,   423,
    /*   5 */ 335,    272,   215,   172,   137,
    /*  10 */ 110,     87,    70,    56,    45,
    /*  15 */ 36,      29,    23,    18,    15,
};

/**
 * Função de comparação para vruntime (usada pela rbtree no modo monoprocessador)
 * No modo multiprocessador, usa fila simples por simplicidade
 */
static int cfs_vruntime_compare(PCB* a, PCB* b) __attribute__((unused));
static int cfs_vruntime_compare(PCB* a, PCB* b) {
    if (a->vruntime < b->vruntime) return -1;
    if (a->vruntime > b->vruntime) return 1;
    return 0;
}

/**
 * Converte prioridade para peso
 */
static int priority_to_weight(int priority) {
    if (priority < 0) priority = 0;
    if (priority > 39) priority = 39;
    return prio_to_weight[priority];
}

/**
 * Calcula timeslice baseado no peso do processo
 */
static int cfs_calculate_timeslice(PCB* process) {
    const int sched_latency = 20000; // 20ms em microssegundos
    
    if (cfs_rq.total_weight == 0) return sched_latency;
    
    int timeslice = (sched_latency * process->weight) / cfs_rq.total_weight;
    return timeslice < 1000 ? 1000 : timeslice; // Mínimo 1ms
}

/**
 * Atualiza vruntime do processo
 */
static void cfs_update_vruntime(PCB* process, uint64_t runtime_ns) {
    // vruntime cresce mais lentamente para processos com maior peso (maior prioridade)
    uint64_t weighted_runtime = (runtime_ns * 1024) / process->weight;
    process->vruntime += weighted_runtime;
    
    // Atualiza min_vruntime (simplificado para multiprocessador)
    if ((uint64_t)process->vruntime < cfs_rq.min_vruntime) {
        cfs_rq.min_vruntime = (uint64_t)process->vruntime;
    }
}

// ========================= Interface Pública =========================

void cfs_init() {
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    if (!cfs_rq.is_initialized) {
        cfs_rq.rb_root = NULL;
        cfs_rq.min_vruntime = 0;
        cfs_rq.total_weight = 0;
        cfs_rq.nr_running = 0;
        cfs_rq.is_initialized = 1;
    }
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
}

void cfs_enqueue_process(PCB* process) {
    if (!process) return;
    
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    
    // Verifica se CFS foi inicializado
    if (!cfs_rq.is_initialized) {
        pthread_mutex_unlock(&cfs_rq.cfs_mutex);
        return;
    }
    
    // Inicializa campos CFS
    process->weight = priority_to_weight(process->priority);
    process->vruntime = cfs_rq.min_vruntime; // Novo processo inicia com min_vruntime
    
    // Insere na Red-Black Tree
    rb_insert(&cfs_rq.rb_root, process, cfs_vruntime_compare);
    // Atualiza estatísticas
    cfs_rq.total_weight += process->weight;
    cfs_rq.nr_running++;
    
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
}

PCB* cfs_pick_next() {
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    // Verifica se CFS foi inicializado ou árvore vazia
    if (!cfs_rq.is_initialized || rb_is_empty(cfs_rq.rb_root)) {
        pthread_mutex_unlock(&cfs_rq.cfs_mutex);
        return NULL;
    }
    // Seleciona o nó mais à esquerda (menor vruntime)
    PCB* next = rb_leftmost(cfs_rq.rb_root);
    if (next != NULL) {
        rb_remove(&cfs_rq.rb_root, next);
        if (cfs_rq.total_weight >= (uint64_t)next->weight) {
            cfs_rq.total_weight -= next->weight;
        }
        if (cfs_rq.nr_running > 0) {
            cfs_rq.nr_running--;
        }
    }
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
    return next;
}

void cfs_put_prev_process(PCB* process, uint64_t runtime_ns) {
    if (!process) return;
    
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    
    // Verifica se CFS foi inicializado
    if (!cfs_rq.is_initialized) {
        pthread_mutex_unlock(&cfs_rq.cfs_mutex);
        return;
    }
    
    // Atualiza vruntime baseado no tempo executado
    cfs_update_vruntime(process, runtime_ns);
    
    // Se processo ainda tem tempo, reinsere na árvore
    if (process->remaining_time > 0) {
        rb_insert(&cfs_rq.rb_root, process, cfs_vruntime_compare);
        cfs_rq.total_weight += process->weight;
        cfs_rq.nr_running++;
    }
    
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
}

int cfs_get_timeslice(PCB* process) {
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    int timeslice = cfs_calculate_timeslice(process);
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
    return timeslice;
}

bool cfs_has_processes() {
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    bool has = (cfs_rq.nr_running > 0);
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
    return has;
}

void cfs_cleanup() {
    pthread_mutex_lock(&cfs_rq.cfs_mutex);
    // Limpa a árvore
    while (!rb_is_empty(cfs_rq.rb_root)) {
        PCB* node = rb_leftmost(cfs_rq.rb_root);
        if (node) rb_remove(&cfs_rq.rb_root, node);
    }
    cfs_rq.min_vruntime = 0;
    cfs_rq.total_weight = 0;
    cfs_rq.nr_running = 0;
    cfs_rq.is_initialized = 0;
    pthread_mutex_unlock(&cfs_rq.cfs_mutex);
    pthread_mutex_destroy(&cfs_rq.cfs_mutex);
}
