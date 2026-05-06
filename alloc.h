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

typedef struct Header_Alloc Header_Alloc;

struct Header_Alloc {
  Color color;
  void *data;
  size_t size;
  int is_free;

  Header_Alloc *parent, *left, *right;
};

typedef struct {
  size_t size;
  int is_free;
} Footer_Alloc;

// API public functions
void *mem_alloc(size_t size);
void mem_free(void *ptr);
void tree_insert(Header_Alloc **root, Header_Alloc *z);
void tree_delete(Header_Alloc **root, Header_Alloc *z);
Header_Alloc *tree_search(Header_Alloc *root, size_t req_mem);
void tree_destroy(Header_Alloc *root);
void tree_print(Header_Alloc *root);
// tree_insert helper functions
void insert_fixup(Header_Alloc **root, Header_Alloc *z);
void left_rotate(Header_Alloc **root, Header_Alloc *x);
void right_rotate(Header_Alloc **root, Header_Alloc *y);
// tree_delete helper functions
void delete_fixup(Header_Alloc **root, Header_Alloc *x, Header_Alloc *x_parent);
void transplant(Header_Alloc **root, Header_Alloc *u, Header_Alloc *v);
Header_Alloc *tree_min(Header_Alloc *node);
Header_Alloc *tree_max(Header_Alloc *node);
Color get_color(Header_Alloc *node);

#endif // ALLOC_H_

#ifdef ALLOC_IMPLEMENTATION

static Header_Alloc *FREE_MEM_ROOT = NULL;

size_t align_to_16(size_t size) { return (val + 15) & ~15; }

// API public functions
void *mem_alloc(size_t req_size) {
  if (req_size <= 0)
    return NULL;

  size_t total_mem = req_size + sizeof(Header_Alloc) + sizeof(Fotter_Alloc);
  total_mem = align_to_16(total_mem);

  if (FREE_MEM_ROOT == NULL) {
    void *mem_region = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem_region == MAP_FAILED) {
      perror("Error mmap");
      exit(EXIT_FAILURE);
    }
    Header_Alloc *header = (Header_Alloc *)mem_region;

    header->data = header;
    header->size = PAGE_SIZE;
    header->is_free = 1;

    Footer_Alloc *footer =
        (Footer_Alloc *)((char *)header + header->size - sizeof(Footer_Alloc));

    footer->size = header->size;
    footer->is_free = header->is_free;

    tree_insert(&FREE_MEM_ROOT, header);
  }

  else {
    Header_Alloc *allocated_header = tree_search(&FREE_MEM_ROOT, total_mem);
    if (!allocated_header) {
      // TODO: Handle if there is no best fit node
    }
    tree_delete(&FREE_MEM_ROOT, allocated_header);

    if (allocated_header->size == total_mem->size)
      return (void *)(allocated_header + 1);

    allocated_header->size = total_mem;
    allocated_header->is_free = 0;

    Footer_Alloc *allocated_footer =
        (Footer_Alloc *)((char *)allocated_header + allocated_header->size +
                         sizeof(Footer_Alloc));
    allocated_footer->size = allocated_header->size;
    allocated_footer->is_free = allocated_header->is_free;

    Header_Alloc *remainder_header =
        (Header_Alloc *)((char *)allocated_header + allocated_header->size);
    remainder_header->size = original_total_size - found_node->size;
    remainder_header->is_free = 1;

    Footer_Alloc *remainder_footer =
        (Footer_Alloc *)((char *)remainder_header + remainder_header->size +
                         sizeof(Footer_Alloc));
    remainder_footer->size = remainder_header->size;
    remainder_footer->is_free = remainder_header->is_free;
  }

  return (void *)(header + 1);
}

void mem_free(void *ptr) {
  if (!ptr)
    return;

  Header_Alloc *header = (Header_Alloc *)ptr - 1;
  header->is_free = 1;

  // TODO: Merge stuff

  tree_insert(&FREE_MEM_ROOT, header);
}

void tree_insert(Header_Alloc **root, Header_Alloc *z) {
  if (root == NULL)
    return;

  // alloc new Header_Alloc
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

  Header_Alloc *parent = NULL;
  Header_Alloc *current = *root;

  while (current != NULL) {
    parent = current;
    // go to the left
    if (z->size < parent->size) {
      current = current->left;
    }
    // go to the right
    else if (z->size > parent->size) {
      current = current->right;
      // data exist
    } else {
    }
  }

  z->parent = parent;
  if (size < parent->size) {
    parent->left = z;
  } else {
    parent->right = z;
  }

  // fix Red-Black violations
  insert_fixup(root, z);

  return;
}

void tree_delete(Header_Alloc **root, Header_Alloc *z) {
  if (root == NULL)
    return;

  // empty tree case
  if (*root == NULL)
    return;

  // find the node to delete
  while (z != NULL && z->data != data) {
    if (data < z->data)
      z = z->left;
    else
      z = z->right;
  }

  if (z == NULL)
    return; // data not found

  Header_Alloc *y = z;
  Header_Alloc *x = NULL;
  Header_Alloc *x_parent = NULL; // used because 'x' might be NULL
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

Header_Alloc *tree_search(Header_Alloc *root, size_t req_mem) {
  if (root == NULL)
    return NULL;

  // find the block size that fits
  Header_Alloc *best_fit = root;
  while (current != NULL && current->size != req_mem) {
    if (req_mem >= best_fit->size)
      best_fit = best_fit->left;
  }

  if (best_fit == NULL)
    return NULL; // block size not found
  return best_fit;
}

void tree_destroy(Header_Alloc *root) {
  if (root != NULL) {
    tree_destroy(root->left);
    tree_destroy(root->right);
    free(root);
  }
}

void tree_print(Header_Alloc *root) {
  if (root != NULL) {
    tree_print(root->left);
    printf("%d ", root->data);
    tree_print(root->right);
  }
}

void insert_fixup(Header_Alloc **root, Header_Alloc *z) {
  // fix if the parent_node is red
  while (z->parent != NULL && z->parent->color == RED) {
    Header_Alloc *g = z->parent->parent;

    if (z->parent == g->left) {
      Header_Alloc *u = g->right;

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
      Header_Alloc *u = g->left;

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

void left_rotate(Header_Alloc **root, Header_Alloc *x) {
  Header_Alloc *y = x->right;
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

void right_rotate(Header_Alloc **root, Header_Alloc *y) {
  Header_Alloc *x = y->left;
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

void delete_fixup(Header_Alloc **root, Header_Alloc *x,
                  Header_Alloc *x_parent) {
  Header_Alloc *w = NULL;

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

void transplant(Header_Alloc **root, Header_Alloc *u, Header_Alloc *v) {
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

Header_Alloc *tree_min(Header_Alloc *node) {
  while (node->left != NULL) {
    node = node->left;
  }
  return node;
}

Header_Alloc *tree_max(Header_Alloc *node) {
  while (node->right != NULL) {
    node = node->right;
  }
  return node;
}

Color get_color(Header_Alloc *node) {
  return (node == NULL) ? BLACK : node->color;
}

#endif // ALLOC_IMPLEMENTATION
