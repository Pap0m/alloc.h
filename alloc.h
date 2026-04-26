#pragma once

#ifndef ALLOC_H_
#define ALLOC_H_

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

typedef enum {
    BLACK,
    RED,
} Color;

typedef struct {
    void* base;

    Color color;
} Bin;

typedef struct {
    void* base;
} Mem;

typedef struct Node Node;

struct Node {
    Color color;
    int data;
    
    Node* parent;
    Node* left_child;
    Node* right_child;
};

// declarations
// Mem* mem_init(void);
// int mem_push(Mem* mem);
// int mem_pull(Mem* mem);


// implementation
// Mem* mem_init(void) {
//     Mem *mem = {0};
//     void* base = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
//     if (base == MAP_FAILED) {
//         perror("Error mmap");
//         exit(EXIT_FAILURE);
//     }
//     mem = (Mem*)base;
//
//     mem->base = base;
//     return mem;
// }

// API public functions
void  tree_insert(Node** root, int data);
void  tree_delete(Node** root, int data);
Node* tree_search(Node* root, int data);
void  tree_destroy(Node* root);
void  tree_print(Node* root);
// tree_insert helper functions
void  insert_fixup(Node** root, Node* z);
void  left_rotate(Node** root, Node* x);
void  right_rotate(Node** root, Node* y);
// tree_delete helper functions
void  delete_fixup(Node** root, Node* x, Node* x_parent);
void  transplant(Node** root, Node* u, Node* v);
Node* tree_min(Node* node);
Node* tree_max(Node* node);
Color get_color(Node* node);

#endif // ALLOC_H_

#ifdef ALLOC_IMPLEMENTATION

// API public functions
void tree_insert(Node** root, int data) {
    if (root == NULL) return;

    // alloc new Node
    Node* z = malloc(sizeof(Node));
    z->color = RED;
    z->data = data;
    z->parent = NULL;
    z->left_child = NULL;
    z->right_child = NULL;

    // empty tree case
    if (*root == NULL) {
        z->color = BLACK;
        *root = z;
        return;
    }

    Node* parent = NULL;
    Node* current = *root; 

    while (current != NULL) {
        parent = current;
        // go to the left 
        if (data < parent->data) {
            current = current->left_child;
        } 
        // go to the right 
        else if (data > parent->data) {
            current = current->right_child;
        // data exist
        } else {
            free(z);
            return;
        }
    }

    z->parent = parent;
    if (data < parent->data) {
        parent->left_child = z;
    } else {
        parent->right_child = z;
    }

    // fix Red-Black violations
    insert_fixup(root, z);

    return;
}

void tree_delete(Node** root, int data) {
    if (root == NULL) return;

    // empty tree case
    if (*root == NULL) return;

    // find the node to delete
    Node* z = *root;
    while (z != NULL && z->data != data) {
        if (data < z->data) z = z->left_child;
        else z = z->right_child;
    }

    if (z == NULL) return; // data not found
    
    Node* y = z;
    Node* x = NULL;
    Node* x_parent = NULL; // used because 'x' might be NULL
    Color y_original_color = y->color;

    // Case 1: No left child
    if (z->left_child == NULL) {
        x = z->right_child;
        x_parent = z->parent;
        transplant(root, z, z->right_child);
    }
    // Case 2: No right child
    else if (z->right_child == NULL) {
        x = z->left_child;
        x_parent = z->parent;
        transplant(root, z, z->left_child);
    }
    // Case 3: Two children
    else {
        y = tree_min(z->right_child); // find successor
        y_original_color = y->color;
        x = y->right_child;

        if (y->parent == z) {
            x_parent = y; 
        } else {
            x_parent = y->parent;
            transplant(root, y, y->right_child);
            y->right_child = z->right_child;
            y->right_child->parent = y;
        }

        transplant(root, z, y);
        y->left_child = z->left_child;
        y->left_child->parent = y;
        y->color = z->color;
    }

    free(z);

    // fix up if we lost a BLACK node
    if (y_original_color == BLACK) {
        delete_fixup(root, x, x_parent); 
    }
}

Node* tree_search(Node* root, int data) {
    if (root == NULL) return NULL;

    // find the node
    Node* current = root;
    while (current != NULL && current->data != data) {
        if (data < current->data) current = current->left_child;
        else current = current->right_child;
    }

    if (current == NULL) return NULL; // data not found
    return current;
}

void tree_destroy(Node* root) {
  if (root != NULL) {
    tree_destroy(root->left_child);
    tree_destroy(root->right_child);
    free(root);
  }
}

void tree_print(Node* root) {
  if (root != NULL) {
    tree_print(root->left_child);
    printf("%d ", root->data);
    tree_print(root->right_child);
  }
}

void insert_fixup(Node** root, Node* z) {
    // fix if the parent_node is red 
    while(z->parent != NULL && z->parent->color == RED) {
        Node* g = z->parent->parent;

        if (z->parent == g->left_child) {
            Node* u = g->right_child;

            // Case 1: The uncle is red (recoloring) 
            if (u != NULL && u->color == RED) {
                z->parent->color = BLACK;
                u->color = BLACK;
                g->color = RED;
                z = g; // go up to grandparent and repeat
            } 
            else {
                // Case 2: The uncle is black and the new node is a right child (left rotation)
                if (z == z->parent->right_child) {
                    z = z->parent;
                    left_rotate(root, z);
                }
                // Case 3: The uncle is black and the new node is a left child (right rotation)
                z->parent->color = BLACK;
                g->color = RED;
                right_rotate(root, g);
            }
        } else {
            Node* u = g->left_child;

            // Case 1: The uncle is red (recoloring) 
            if (u != NULL && u->color == RED) {
                z->parent->color = BLACK;
                u->color = BLACK;
                g->color = RED;
                z = g; // go up to grandparent and repeat
            } 
            else {
                // Case 2: The uncle is black and the new node is a left child (left rotation)
                if (z == z->parent->left_child) {
                    z = z->parent;
                    right_rotate(root, z);
                }
                // Case 3: The uncle is black and the new node is a left child (right rotation)
                z->parent->color = BLACK;
                g->color = RED;
                left_rotate(root, g);
            }
        }
    }
    (*root)->color = BLACK;
    return;
}

void  left_rotate(Node** root, Node* x) {
    Node* y = x->right_child;
    x->right_child = y->left_child;

    if (y->left_child != NULL) {
        y->left_child->parent = x;
    }
    y->parent = x->parent;

    if (x->parent == NULL) {
        *root = y; 
    } else if (x == x->parent->left_child) {
        x->parent->left_child = y;
    } else {
        x->parent->right_child = y;
    }
    y->left_child = x;
    x->parent = y;

    return;
}

void  right_rotate(Node** root, Node* y) {
    Node* x = y->left_child;
    y->left_child = x->right_child;

    if (x->right_child != NULL) {
        x->right_child->parent = y;
    }
    x->parent = y->parent;

    if (y->parent == NULL) {
        *root = x; 
    } else if (y == y->parent->right_child) {
        y->parent->right_child = x; 
    } else {
      y->parent->left_child = x;
    }
    x->right_child = y;
    y->parent = x;

    return;
}


void delete_fixup(Node** root, Node* x, Node* x_parent) {
    Node* w = NULL;

    // while x is not root and x is black
    while (x != *root && get_color(x) == BLACK) {
        // x is a left child
        if (x == x_parent->left_child) {
            w = x_parent->right_child; // w is x sibling
            
            // Case 1: sibling is RED
            if (get_color(w) == RED) {
                w->color = BLACK;
                x_parent->color = RED;
                left_rotate(root, x_parent);
                w = x_parent->right_child; // update sibling
            }
            // Case 2: sibling is BLACK and both of sibling's children are BLACK
            if (get_color(w->left_child) == BLACK && get_color(w->right_child) == BLACK) {
                if (w != NULL) w->color = RED;
                x = x_parent; // move up 
                x_parent = x->parent;
            } 
            else {
                // Case 3: sibling is BLACK and sibling's right child is BLACK but left must be RED
                if (get_color(w->right_child) == BLACK) {
                    if (w->left_child != NULL) w->left_child->color = BLACK;
                    if (w != NULL) w->color = RED;
                    right_rotate(root, w);
                    w = x_parent->right_child;
                }

                // Case 4: sibling is BLACK and sibling's right child is RED
                if (w != NULL) w->color = x_parent->color;
                x_parent->color = BLACK;
                if (w != NULL && w->right_child != NULL) w->right_child->color = BLACK;
                left_rotate(root, x_parent);

                x = *root; // force exit
            }
        }
        // x is a right child
        else {
            w = x_parent->left_child; // w is x sibling
            
            // Case 1: sibling is RED
            if (get_color(w) == RED) {
                w->color = BLACK;
                x_parent->color = RED;
                right_rotate(root, x_parent);
                w = x_parent->left_child; // update sibling
            }
            // Case 2: sibling is BLACK and both of sibling's children are BLACK
            if (get_color(w->right_child) == BLACK && get_color(w->left_child) == BLACK) {
                if (w != NULL) w->color = RED;
                x = x_parent; // move up 
                x_parent = x->parent;
            } 
            else {
                // Case 3: sibling is BLACK and sibling's left child is BLACK but right must be RED
                if (get_color(w->left_child) == BLACK) {
                    if (w->right_child != NULL) w->right_child->color = BLACK;
                    if (w != NULL) w->color = RED;
                    left_rotate(root, w);
                    w = x_parent->left_child;
                }

                // Case 4: sibling is BLACK and sibling's left child is RED
                if (w != NULL) w->color = x_parent->color;
                x_parent->color = BLACK;
                if (w != NULL && w->left_child != NULL) w->left_child->color = BLACK;
                right_rotate(root, x_parent);

                x = *root; // force exit
            }
        }
    }

    // set extra BLACK to normal BLACK
    if (x != NULL) {
        x->color = BLACK; 
    }
}

void transplant(Node** root, Node* u, Node* v) {
    if (u->parent == NULL) {
        *root = v; 
    } else if (u == u->parent->left_child) {
        u->parent->left_child = v; 
    } else {
        u->parent->right_child = v;
    }

    if (v != NULL) {
        v->parent = u->parent; 
    }
}

Node* tree_min(Node* node) {
    while (node->left_child != NULL) {
        node = node->left_child;
    } 
    return node;
}

Node* tree_max(Node* node) {
    while (node->right_child != NULL) {
        node = node->right_child; 
    }
    return node;
}

Color get_color(Node* node) {
    return (node == NULL) ? BLACK : node->color;
}


#endif // ALLOC_IMPLEMENTATION
