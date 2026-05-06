#pragma once

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
typedef struct Footer_Alloc Footer_Alloc;
typedef struct Arena Arena;

struct Header_Alloc {
  Color color;
  void *data;
  // size = Header_Alloc + data + Footer_Alloc
  size_t size;
  int is_free;

  Header_Alloc *parent, *left, *right;

  Header_Alloc *next_same_size;
  Header_Alloc *prev_same_size;
};

struct Footer_Alloc {
  size_t size;
  int is_free;
};

// track mmap regions to properly munmap them later
struct Arena {
  void *start;
  size_t length;
  Arena *next;
};

// API public functions
void *mem_alloc(size_t size);
void mem_free(void *ptr);
void tree_insert(Header_Alloc **root, Header_Alloc *z);
void tree_delete(Header_Alloc **root, Header_Alloc *z);
Header_Alloc *tree_search(Header_Alloc *root, size_t req_mem);
void free_all(void);
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

// linked list and exact delete funcion
void tree_exact_delete(Header_Alloc **root, Header_Alloc *target);

#endif // ALLOC_H_

#ifdef ALLOC_IMPLEMENTATION

static Header_Alloc *FREE_MEM_ROOT = NULL;
static Arena *ARENA_HEAD = NULL;

static inline size_t align_to_16(size_t size) { return (size + 15) & ~15; }

// helper to expand heap dynamically
static void *extend_heap(size_t total_mem) {
  size_t required_mem = total_mem + sizeof(Arena);
  size_t map_size = (required_mem + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  void *mem_region = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem_region == MAP_FAILED) {
    perror("Error mmap");
    exit(EXIT_FAILURE);
  }

  // track the Arena
  Arena *arena = (Arena *)mem_region;
  arena->start = mem_region;
  arena->length = map_size;
  arena->next = ARENA_HEAD;
  ARENA_HEAD = arena;

  // Setup the header just after the Arena struct
  Header_Alloc *header = (Header_Alloc *)((char *)mem_region + sizeof(Arena));
  header->data = header;
  header->size = map_size - sizeof(Arena);
  header->is_free = 1;
  header->next_same_size = NULL;
  header->prev_same_size = NULL;

  Footer_Alloc *footer =
      (Footer_Alloc *)((char *)header + header->size - sizeof(Footer_Alloc));
  footer->size = header->size;
  footer->is_free = header->is_free;

  tree_insert(&FREE_MEM_ROOT, header);
  return header;
}

// API public functions
void *mem_alloc(size_t req_size) {
  if (req_size <= 0)
    return NULL;

  size_t total_mem = req_size + sizeof(Header_Alloc) + sizeof(Footer_Alloc);
  total_mem = align_to_16(total_mem);

  if (FREE_MEM_ROOT == NULL) {
    if (!extend_heap(total_mem))
      exit(EXIT_FAILURE);
  }

  Header_Alloc *allocated_header = tree_search(FREE_MEM_ROOT, total_mem);
  if (!allocated_header) {
    if (!extend_heap(total_mem))
      return NULL;
    allocated_header = tree_search(FREE_MEM_ROOT, total_mem);
  }

  if (allocated_header->next_same_size != NULL) {
    Header_Alloc *dup = allocated_header->next_same_size;
    allocated_header->next_same_size = dup->next_same_size;
    if (dup->next_same_size) {
      dup->next_same_size->prev_same_size = (Header_Alloc *)allocated_header;
    }
    dup->is_free = 0;
    allocated_header = dup; // Use the duplicate, leave tree intact
  } else {
    tree_exact_delete(&FREE_MEM_ROOT, allocated_header);
  }

  allocated_header->is_free = 0;

  size_t min_split_size =
      align_to_16(sizeof(Header_Alloc) + sizeof(Footer_Alloc));
  if (allocated_header->size - total_mem < min_split_size) {
    // Do not split, allocate entire block
    return (void *)(allocated_header + 1);
  }

  // split the block
  Footer_Alloc *allocated_footer =
      (Footer_Alloc *)((char *)allocated_header + total_mem -
                       sizeof(Footer_Alloc));
  allocated_footer->size = total_mem;
  allocated_footer->is_free = 0;

  Header_Alloc *remainder_header =
      (Header_Alloc *)((char *)allocated_header + total_mem);
  remainder_header->size = allocated_header->size - total_mem;
  remainder_header->is_free = 1;

  Footer_Alloc *remainder_footer =
      (Footer_Alloc *)((char *)remainder_header + remainder_header->size -
                       sizeof(Footer_Alloc));
  remainder_footer->size = remainder_header->size;
  remainder_footer->is_free = remainder_header->is_free;

  allocated_header->size = total_mem;

  tree_insert(&FREE_MEM_ROOT, remainder_header);

  return (void *)(allocated_header + 1);
}

void mem_free(void *ptr) {
  if (ptr == NULL)
    return;

  Header_Alloc *h_ptr = ((Header_Alloc *)ptr - 1);
  h_ptr->is_free = 1;

  // look right
  Header_Alloc *h_next = (Header_Alloc *)((char *)h_ptr + h_ptr->size);
  if (h_next->is_free == 1) {
    tree_exact_delete(&FREE_MEM_ROOT, h_next);
    h_ptr->size += h_next->size;
  }

  // look left
  Footer_Alloc *f_prev = (Footer_Alloc *)((char *)h_ptr - sizeof(Footer_Alloc));
  if (f_prev->is_free == 1) {
    Header_Alloc *h_prev = (Header_Alloc *)((char *)h_ptr - f_prev->size);
    tree_exact_delete(&FREE_MEM_ROOT, h_prev);
    h_prev->size += h_ptr->size;
    h_ptr = h_prev;
    // shift focus to the left block
  }

  // update footer
  Footer_Alloc *f_ptr =
      (Footer_Alloc *)((char *)h_ptr + h_ptr->size - sizeof(Footer_Alloc));
  f_ptr->size = h_ptr->size;
  f_ptr->is_free = 1;

  tree_insert(&FREE_MEM_ROOT, h_ptr);

  return;
}

void tree_exact_delete(Header_Alloc **root, Header_Alloc *target) {
  if (root == NULL || *root == NULL || target == NULL)
    return;

  // Case A: target is in linked list, but is not the tree head
  if (target->prev_same_size != NULL) {
    target->prev_same_size->next_same_size = target->next_same_size;
    if (target->next_same_size) {
      target->next_same_size->prev_same_size = target->prev_same_size;
    }
    return;
  }

  // Case B: target is the tree head, but has duplicates
  // swap it
  if (target->next_same_size != NULL) {
    Header_Alloc *replacement = target->next_same_size;
    replacement->prev_same_size = NULL; // it's the new head

    // Inherit tree topology
    replacement->color = target->color;
    replacement->parent = target->parent;
    replacement->left = target->left;
    replacement->right = target->right;

    if (target->parent == NULL) {
      *root = replacement;
    } else if (target == target->parent->left) {
      target->parent->left = replacement;
    } else {
      target->parent->right = replacement;
    }

    if (target->left)
      target->left->parent = replacement;
    if (target->right)
      target->right->parent = replacement;

    return;
  }

  // Case C: normal tree node with no duplicates
  tree_delete(root, target);
}

void tree_insert(Header_Alloc **root, Header_Alloc *z) {
  if (root == NULL || z == NULL)
    return;

  z->color = RED;
  z->parent = NULL;
  z->left = NULL;
  z->right = NULL;
  z->next_same_size = NULL;
  z->prev_same_size = NULL;

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
    } else {
      z->next_same_size = current->next_same_size;
      if (current->next_same_size) {
        current->next_same_size->prev_same_size = z;
      }
      current->next_same_size = z;
      z->prev_same_size = current;
      return; // not alter tree topology
    }
  }

  z->parent = parent;
  if (z->size < parent->size) {
    parent->left = z;
  } else {
    parent->right = z;
  }

  // fix Red-Black violations
  insert_fixup(root, z);

  return;
}

void tree_delete(Header_Alloc **root, Header_Alloc *z) {
  if (root == NULL || *root == NULL || z == NULL)
    return;

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

  // fix up if we lost a BLACK node
  if (y_original_color == BLACK) {
    delete_fixup(root, x, x_parent);
  }
}

Header_Alloc *tree_search(Header_Alloc *root, size_t req_mem) {
  if (root == NULL)
    return NULL;

  Header_Alloc *best_fit = NULL;
  Header_Alloc *current = root;

  while (current != NULL) {
    if (current->size >= req_mem) {
      best_fit = current;
      current = current->left;
    } else {
      current = current->right;
    }
  }

  return best_fit;
}

void free_all(void) {
  Arena *current = ARENA_HEAD;
  while (current != NULL) {
    Arena *next = current->next;
    munmap(current->start, current->length);
    current = next;
  }

  ARENA_HEAD = NULL;
  FREE_MEM_ROOT = NULL;
}

void tree_print(Header_Alloc *root) {
  if (root != NULL) {
    tree_print(root->left);
    printf("%zu ", root->size);
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
        // Case 3: sibling is BLACK and sibling's right child is BLACK but
        // left must be RED
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
        // Case 3: sibling is BLACK and sibling's left child is BLACK but
        // right must be RED
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
