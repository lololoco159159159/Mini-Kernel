#ifndef RBTREE_H
#define RBTREE_H

#include "structures.h"
#include <stdbool.h>

// Forward declaration
struct PCB;

/**
 * Tipos de função para comparação e visita de nós
 */
typedef int (*rb_compare_func_t)(struct PCB* a, struct PCB* b);
typedef void (*rb_visit_func_t)(struct PCB* node);

/**
 * Insere nó na Red-Black Tree
 * @param root Ponteiro para raiz da árvore
 * @param new_node Nó a ser inserido
 * @param compare Função de comparação entre nós
 */
void rb_insert(PCB** root, PCB* new_node, rb_compare_func_t compare);

/**
 * Remove nó da Red-Black Tree
 * @param root Ponteiro para raiz da árvore
 * @param node Nó a ser removido
 */
void rb_remove(PCB** root, PCB* node);

/**
 * Encontra nó mais à esquerda (menor valor)
 * @param root Raiz da árvore
 * @return Nó com menor valor ou NULL se árvore vazia
 */
PCB* rb_leftmost(PCB* root);

/**
 * Encontra nó mais à direita (maior valor)
 * @param root Raiz da árvore
 * @return Nó com maior valor ou NULL se árvore vazia
 */
PCB* rb_rightmost(PCB* root);

/**
 * Busca nó específico na árvore
 * @param root Raiz da árvore
 * @param target Nó a ser buscado
 * @param compare Função de comparação
 * @return Nó encontrado ou NULL se não existe
 */
PCB* rb_search(PCB* root, PCB* target, rb_compare_func_t compare);

/**
 * Conta número de nós na árvore
 * @param root Raiz da árvore
 * @return Número de nós
 */
int rb_count_nodes(PCB* root);

/**
 * Percorre árvore em ordem crescente
 * @param root Raiz da árvore
 * @param visit Função chamada para cada nó visitado
 */
void rb_inorder_walk(PCB* root, rb_visit_func_t visit);

/**
 * Verifica se árvore está vazia
 * @param root Raiz da árvore
 * @return true se vazia, false caso contrário
 */
bool rb_is_empty(PCB* root);

#endif // RBTREE_H
