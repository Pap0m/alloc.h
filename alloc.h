#pragma once

#include <cstddef>
#ifndef ALLOC_H_
#define ALLOC_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

typedef enum {
  BLACK,
  RED,
} Color;

typedef struct {
  void *base;
} Mem;

typedef struct Free_Mem Free_Mem;

struct Free_Mem {
  Color color;
  void *data;
  size_t size;
  // int is_free;

  Free_Mem *parent, *left, *right;
};

// declarations
Free_Mem *mem_alloc(size_t size);
// int mem_push(Mem* mem);
// int mem_pull(Mem* mem);

// implementation

// API public functions
Free_Mem *mem_alloc(size_t size);
void tree_insert(Free_Mem **root, int data);
void tree_delete(Free_Mem **root, int data);
Free_Mem *tree_search(Free_Mem *root, int data);
void tree_destroy(Free_Mem *root);
void tree_print(Free_Mem *root);
// tree_insert helper functions
void insert_fixup(Free_Mem **root, Free_Mem *z);
void left_rotate(Free_Mem **root, Free_Mem *x);
void right_rotate(Free_Mem **root, Free_Mem *y);
// tree_delete helper functions
void delete_fixup(Free_Mem **root, Free_Mem *x, Free_Mem *x_parent);
void transplant(Free_Mem **root, Free_Mem *u, Free_Mem *v);
Free_Mem *tree_min(Free_Mem *node);
Free_Mem *tree_max(Free_Mem *node);
Color get_color(Free_Mem *node);

#endif // ALLOC_H_

#ifdef ALLOC_IMPLEMENTATION

// API public functions
Free_Mem *mem_alloc(size_t size) {
  Free_Mem *node = {0};
  void *base = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) {
    perror("Error mmap");
    exit(EXIT_FAILURE);
  }
  Free_Mem *free_mem = (Free_Mem *)base;

  free_mem->data = base;

  return free_mem;
}

void tree_insert(Free_Mem **root, int data) {
  if (root == NULL)
    return;

  // alloc new Free_Mem
  Free_Mem *z = malloc(sizeof(Free_Mem));
  z->color = RED;
  z->data = data;
  z->parent = NULL;
  z->left = NULL;
  z->right = NULL;

  // empty tree case
  if (*root == NULL) {
    z->color = BLACK;
    *root = z;
    return;
  }

  Free_Mem *parent = NULL;
  Free_Mem *current = *root;

  while (current != NULL) {
    parent = current;
    // go to the left
    if (data < parent->data) {
      current = current->left;
    }
    // go to the right
    else if (data > parent->data) {
      current = current->right;
      // data exist
    } else {
      free(z);
      return;
    }
  }

  z->parent = parent;
  if (data < parent->data) {
    parent->left = z;
  } else {
    parent->right = z;
  }

  // fix Red-Black violations
  insert_fixup(root, z);

  return;
}

void tree_delete(Free_Mem **root, int data) {
  if (root == NULL)
    return;

  // empty tree case
  if (*root == NULL)
    return;

  // find the node to delete
  Free_Mem *z = *root;
  while (z != NULL && z->data != data) {
    if (data < z->data)
      z = z->left;
    else
      z = z->right;
  }

  if (z == NULL)
    return; // data not found

  Free_Mem *y = z;
  Free_Mem *x = NULL;
  Free_Mem *x_parent = NULL; // used because 'x' might be NULL
  Color y_original_color = y->color;

  // Case 1: No left child
  if (z->left == NULL) {
    x = z->right;
    x_parent = z->parent;
    transplant(root, z, z->right);
  }
  // Case 2: No right child
  else if (z->right == NULL) {
    x = z->left;
    x_parent = z->parent;
    transplant(root, z, z->left);
  }
  // Case 3: Two children
  else {
    y = tree_min(z->right); // find successor
    y_original_color = y->color;
    x = y->right;

    if (y->parent == z) {
      x_parent = y;
    } else {
      x_parent = y->parent;
      transplant(root, y, y->right);
      y->right = z->right;
      y->right->parent = y;
    }

    transplant(root, z, y);
    y->left = z->left;
    y->left->parent = y;
    y->color = z->color;
  }

  free(z);

  // fix up if we lost a BLACK node
  if (y_original_color == BLACK) {
    delete_fixup(root, x, x_parent);
  }
}

Free_Mem *tree_search(Free_Mem *root, int data) {
  if (root == NULL)
    return NULL;

  // find the node
  Free_Mem *current = root;
  while (current != NULL && current->data != data) {
    if (data < current->data)
      current = current->left;
    else
      current = current->right;
  }

  if (current == NULL)
    return NULL; // data not found
  return current;
}

void tree_destroy(Free_Mem *root) {
  if (root != NULL) {
    tree_destroy(root->left);
    tree_destroy(root->right);
    free(root);
  }
}

void tree_print(Free_Mem *root) {
  if (root != NULL) {
    tree_print(root->left);
    printf("%d ", root->data);
    tree_print(root->right);
  }
}

void insert_fixup(Free_Mem **root, Free_Mem *z) {
  // fix if the parent_node is red
  while (z->parent != NULL && z->parent->color == RED) {
    Free_Mem *g = z->parent->parent;

    if (z->parent == g->left) {
      Free_Mem *u = g->right;

      // Case 1: The uncle is red (recoloring)
      if (u != NULL && u->color == RED) {
        z->parent->color = BLACK;
        u->color = BLACK;
        g->color = RED;
        z = g; // go up to grandparent and repeat
      } else {
        // Case 2: The uncle is black and the new node is a right child (left
        // rotation)
        if (z == z->parent->right) {
          z = z->parent;
          left_rotate(root, z);
        }
        // Case 3: The uncle is black and the new node is a left child (right
        // rotation)
        z->parent->color = BLACK;
        g->color = RED;
        right_rotate(root, g);
      }
    } else {
      Free_Mem *u = g->left;

      // Case 1: The uncle is red (recoloring)
      if (u != NULL && u->color == RED) {
        z->parent->color = BLACK;
        u->color = BLACK;
        g->color = RED;
        z = g; // go up to grandparent and repeat
      } else {
        // Case 2: The uncle is black and the new node is a left child (left
        // rotation)
        if (z == z->parent->left) {
          z = z->parent;
          right_rotate(root, z);
        }
        // Case 3: The uncle is black and the new node is a left child (right
        // rotation)
        z->parent->color = BLACK;
        g->color = RED;
        left_rotate(root, g);
      }
    }
  }
  (*root)->color = BLACK;
  return;
}

void left_rotate(Free_Mem **root, Free_Mem *x) {
  Free_Mem *y = x->right;
  x->right = y->left;

  if (y->left != NULL) {
    y->left->parent = x;
  }
  y->parent = x->parent;

  if (x->parent == NULL) {
    *root = y;
  } else if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }
  y->left = x;
  x->parent = y;

  return;
}

void right_rotate(Free_Mem **root, Free_Mem *y) {
  Free_Mem *x = y->left;
  y->left = x->right;

  if (x->right != NULL) {
    x->right->parent = y;
  }
  x->parent = y->parent;

  if (y->parent == NULL) {
    *root = x;
  } else if (y == y->parent->right) {
    y->parent->right = x;
  } else {
    y->parent->left = x;
  }
  x->right = y;
  y->parent = x;

  return;
}

void delete_fixup(Free_Mem **root, Free_Mem *x, Free_Mem *x_parent) {
  Free_Mem *w = NULL;

  // while x is not root and x is black
  while (x != *root && get_color(x) == BLACK) {
    // x is a left child
    if (x == x_parent->left) {
      w = x_parent->right; // w is x sibling

      // Case 1: sibling is RED
      if (get_color(w) == RED) {
        w->color = BLACK;
        x_parent->color = RED;
        left_rotate(root, x_parent);
        w = x_parent->right; // update sibling
      }
      // Case 2: sibling is BLACK and both of sibling's children are BLACK
      if (get_color(w->left) == BLACK && get_color(w->right) == BLACK) {
        if (w != NULL)
          w->color = RED;
        x = x_parent; // move up
        x_parent = x->parent;
      } else {
        // Case 3: sibling is BLACK and sibling's right child is BLACK but left
        // must be RED
        if (get_color(w->right) == BLACK) {
          if (w->left != NULL)
            w->left->color = BLACK;
          if (w != NULL)
            w->color = RED;
          right_rotate(root, w);
          w = x_parent->right;
        }

        // Case 4: sibling is BLACK and sibling's right child is RED
        if (w != NULL)
          w->color = x_parent->color;
        x_parent->color = BLACK;
        if (w != NULL && w->right != NULL)
          w->right->color = BLACK;
        left_rotate(root, x_parent);

        x = *root; // force exit
      }
    }
    // x is a right child
    else {
      w = x_parent->left; // w is x sibling

      // Case 1: sibling is RED
      if (get_color(w) == RED) {
        w->color = BLACK;
        x_parent->color = RED;
        right_rotate(root, x_parent);
        w = x_parent->left; // update sibling
      }
      // Case 2: sibling is BLACK and both of sibling's children are BLACK
      if (get_color(w->right) == BLACK && get_color(w->left) == BLACK) {
        if (w != NULL)
          w->color = RED;
        x = x_parent; // move up
        x_parent = x->parent;
      } else {
        // Case 3: sibling is BLACK and sibling's left child is BLACK but right
        // must be RED
        if (get_color(w->left) == BLACK) {
          if (w->right != NULL)
            w->right->color = BLACK;
          if (w != NULL)
            w->color = RED;
          left_rotate(root, w);
          w = x_parent->left;
        }

        // Case 4: sibling is BLACK and sibling's left child is RED
        if (w != NULL)
          w->color = x_parent->color;
        x_parent->color = BLACK;
        if (w != NULL && w->left != NULL)
          w->left->color = BLACK;
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

void transplant(Free_Mem **root, Free_Mem *u, Free_Mem *v) {
  if (u->parent == NULL) {
    *root = v;
  } else if (u == u->parent->left) {
    u->parent->left = v;
  } else {
    u->parent->right = v;
  }

  if (v != NULL) {
    v->parent = u->parent;
  }
}

Free_Mem *tree_min(Free_Mem *node) {
  while (node->left != NULL) {
    node = node->left;
  }
  return node;
}

Free_Mem *tree_max(Free_Mem *node) {
  while (node->right != NULL) {
    node = node->right;
  }
  return node;
}

Color get_color(Free_Mem *node) { return (node == NULL) ? BLACK : node->color; }

#endif // ALLOC_IMPLEMENTATION
