//peguei o exemplo do Giovanni de um lab de tbo (ou pelo menos oq restou do q enviei como resposta)
//se vc tiver uma implementacao salva, pd trocar essa, n sei se no lab tinha q alterar algo

//att: parece estar funcionando normal, ainda n quebrou


#include "../lib/rbtree.h"
#include "../lib/structures.h"
#include <stdlib.h>
#include <stdio.h>


// Utilitários para manipulação de ponteiros
#define RB_PARENT(n)   ((n)->rb_parent)
#define RB_LEFT(n)     ((n)->rb_left)
#define RB_RIGHT(n)    ((n)->rb_right)
#define RB_COLOR(n)    ((n)->rb_color)

// Rotações
static void rb_rotate_left(PCB **root, PCB *x) {
    PCB *y = x->rb_right;
    x->rb_right = y->rb_left;
    if (y->rb_left)
        y->rb_left->rb_parent = x;
    y->rb_parent = x->rb_parent;
    if (!x->rb_parent)
        *root = y;
    else if (x == x->rb_parent->rb_left)
        x->rb_parent->rb_left = y;
    else
        x->rb_parent->rb_right = y;
    y->rb_left = x;
    x->rb_parent = y;
}

static void rb_rotate_right(PCB **root, PCB *y) {
    PCB *x = y->rb_left;
    y->rb_left = x->rb_right;
    if (x->rb_right)
        x->rb_right->rb_parent = y;
    x->rb_parent = y->rb_parent;
    if (!y->rb_parent)
        *root = x;
    else if (y == y->rb_parent->rb_left)
        y->rb_parent->rb_left = x;
    else
        y->rb_parent->rb_right = x;
    x->rb_right = y;
    y->rb_parent = x;
}

// Fix-up após inserção
static void rb_insert_fixup(PCB **root, PCB *z) {
    while (z->rb_parent && z->rb_parent->rb_color == RB_RED) {
        if (z->rb_parent == z->rb_parent->rb_parent->rb_left) {
            PCB *y = z->rb_parent->rb_parent->rb_right;
            if (y && y->rb_color == RB_RED) {
                z->rb_parent->rb_color = RB_BLACK;
                y->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                z = z->rb_parent->rb_parent;
            } else {
                if (z == z->rb_parent->rb_right) {
                    z = z->rb_parent;
                    rb_rotate_left(root, z);
                }
                z->rb_parent->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                rb_rotate_right(root, z->rb_parent->rb_parent);
            }
        } else {
            PCB *y = z->rb_parent->rb_parent->rb_left;
            if (y && y->rb_color == RB_RED) {
                z->rb_parent->rb_color = RB_BLACK;
                y->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                z = z->rb_parent->rb_parent;
            } else {
                if (z == z->rb_parent->rb_left) {
                    z = z->rb_parent;
                    rb_rotate_right(root, z);
                }
                z->rb_parent->rb_color = RB_BLACK;
                z->rb_parent->rb_parent->rb_color = RB_RED;
                rb_rotate_left(root, z->rb_parent->rb_parent);
            }
        }
    }
    (*root)->rb_color = RB_BLACK;
}

void rb_insert(PCB **root, PCB *z, rb_compare_func_t compare) {
    if (!z || !compare) return;
    PCB *y = NULL;
    PCB *x = *root;
    while (x) {
        y = x;
        if (compare(z, x) < 0)
            x = x->rb_left;
        else
            x = x->rb_right;
    }
    z->rb_parent = y;
    if (!y)
        *root = z;
    else if (compare(z, y) < 0)
        y->rb_left = z;
    else
        y->rb_right = z;
    z->rb_left = NULL;
    z->rb_right = NULL;
    z->rb_color = RB_RED;
    rb_insert_fixup(root, z);
}


// Substitui u por v na árvore
static void rb_transplant(PCB **root, PCB *u, PCB *v) {
    if (!u->rb_parent)
        *root = v;
    else if (u == u->rb_parent->rb_left)
        u->rb_parent->rb_left = v;
    else
        u->rb_parent->rb_right = v;
    if (v)
        v->rb_parent = u->rb_parent;
}

// Mínimo da subárvore
static PCB* rb_minimum(PCB *node) {
    while (node->rb_left)
        node = node->rb_left;
    return node;
}

// Fix-up após remoção
static void rb_remove_fixup(PCB **root, PCB *x, PCB *x_parent) {
    while ((!x || x->rb_color == RB_BLACK) && x != *root) {
        if (x == x_parent->rb_left) {
            PCB *w = x_parent->rb_right;
            if (w && w->rb_color == RB_RED) {
                w->rb_color = RB_BLACK;
                x_parent->rb_color = RB_RED;
                rb_rotate_left(root, x_parent);
                w = x_parent->rb_right;
            }
            if ((!w->rb_left || w->rb_left->rb_color == RB_BLACK) &&
                (!w->rb_right || w->rb_right->rb_color == RB_BLACK)) {
                if (w) w->rb_color = RB_RED;
                x = x_parent;
                x_parent = x->rb_parent;
            } else {
                if (!w->rb_right || w->rb_right->rb_color == RB_BLACK) {
                    if (w->rb_left) w->rb_left->rb_color = RB_BLACK;
                    w->rb_color = RB_RED;
                    rb_rotate_right(root, w);
                    w = x_parent->rb_right;
                }
                if (w) w->rb_color = x_parent->rb_color;
                x_parent->rb_color = RB_BLACK;
                if (w && w->rb_right) w->rb_right->rb_color = RB_BLACK;
                rb_rotate_left(root, x_parent);
                x = *root;
            }
        } else {
            PCB *w = x_parent->rb_left;
            if (w && w->rb_color == RB_RED) {
                w->rb_color = RB_BLACK;
                x_parent->rb_color = RB_RED;
                rb_rotate_right(root, x_parent);
                w = x_parent->rb_left;
            }
            if ((!w->rb_right || w->rb_right->rb_color == RB_BLACK) &&
                (!w->rb_left || w->rb_left->rb_color == RB_BLACK)) {
                if (w) w->rb_color = RB_RED;
                x = x_parent;
                x_parent = x->rb_parent;
            } else {
                if (!w->rb_left || w->rb_left->rb_color == RB_BLACK) {
                    if (w->rb_right) w->rb_right->rb_color = RB_BLACK;
                    w->rb_color = RB_RED;
                    rb_rotate_left(root, w);
                    w = x_parent->rb_left;
                }
                if (w) w->rb_color = x_parent->rb_color;
                x_parent->rb_color = RB_BLACK;
                if (w && w->rb_left) w->rb_left->rb_color = RB_BLACK;
                rb_rotate_right(root, x_parent);
                x = *root;
            }
        }
    }
    if (x) x->rb_color = RB_BLACK;
}

void rb_remove(PCB **root, PCB *z) {
    if (!z || !*root) return;
    PCB *y = z;
    PCB *x = NULL;
    PCB *x_parent = NULL;
    int y_original_color = y->rb_color;
    if (!z->rb_left) {
        x = z->rb_right;
        x_parent = z->rb_parent;
        rb_transplant(root, z, z->rb_right);
    } else if (!z->rb_right) {
        x = z->rb_left;
        x_parent = z->rb_parent;
        rb_transplant(root, z, z->rb_left);
    } else {
        y = rb_minimum(z->rb_right);
        y_original_color = y->rb_color;
        x = y->rb_right;
        if (y->rb_parent == z) {
            if (x) x->rb_parent = y;
            x_parent = y;
        } else {
            rb_transplant(root, y, y->rb_right);
            y->rb_right = z->rb_right;
            if (y->rb_right) y->rb_right->rb_parent = y;
            x_parent = y->rb_parent;
        }
        rb_transplant(root, z, y);
        y->rb_left = z->rb_left;
        if (y->rb_left) y->rb_left->rb_parent = y;
        y->rb_color = z->rb_color;
    }
    if (y_original_color == RB_BLACK)
        rb_remove_fixup(root, x, x_parent);
    // Limpa ponteiros do nó removido
    z->rb_left = z->rb_right = z->rb_parent = NULL;
}

/**
 * Encontra o nó mais à esquerda (menor vruntime)
 */
PCB* rb_leftmost(PCB* root) {
    if (!root) return NULL;
    
    while (root->rb_left) {
        root = root->rb_left;
    }
    return root;
}

/**
 * Encontra o nó mais à direita (maior vruntime)
 */
PCB* rb_rightmost(PCB* root) {
    if (!root) return NULL;
    
    while (root->rb_right) {
        root = root->rb_right;
    }
    return root;
}

/**
 * Busca nó na árvore
 */
PCB* rb_search(PCB* root, PCB* target, rb_compare_func_t compare) {
    while (root) {
        int cmp = compare(target, root);
        if (cmp == 0) {
            return root;
        } else if (cmp < 0) {
            root = root->rb_left;
        } else {
            root = root->rb_right;
        }
    }
    return NULL;
}

/**
 * Conta número de nós na árvore
 */
int rb_count_nodes(PCB* root) {
    if (!root) return 0;
    return 1 + rb_count_nodes(root->rb_left) + rb_count_nodes(root->rb_right);
}

/**
 * Verifica se árvore está vazia
 */
bool rb_is_empty(PCB* root) {
    return (root == NULL);
}

/**
 * Percorre árvore em ordem
 */
void rb_inorder_walk(PCB* root, rb_visit_func_t visit) {
    if (!root || !visit) return;
    
    rb_inorder_walk(root->rb_left, visit);
    visit(root);
    rb_inorder_walk(root->rb_right, visit);
}
