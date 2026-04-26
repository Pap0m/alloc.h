#include <stddef.h>
#include <stdio.h>

#define ALLOC_IMPLEMENTATION
#include "alloc.h"

typedef struct {
  char* data;
  size_t length;
} String;

int main(void) {
    Node* root = {0};

    int values[] = {
        42, 15, 89, 23, 7, 64, 31, 95, 50, 12, 77, 3, 58, 21, 84, 
        39, 6, 91, 47, 18, 72, 29, 55, 82, 10, 63, 36, 99, 4, 68, 
        25, 53, 87, 14, 41, 76, 2, 60, 33, 92
    };
    int num_values = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < num_values; i++) {
        tree_insert(&root, values[i]);
    }

    printf("--- Tree after insertions ---\n");
    tree_print(root);
    printf("\n\n");

    printf("Deleting 2 (Red Leaf)...\n");
    tree_delete(&root, 2);

    printf("Deleting 31 (Black Node, internal)...\n");
    tree_delete(&root, 31);

    printf("Deleting 42 (The Root)...\n");
    tree_delete(&root, 42);

    printf("\n--- Tree after deletions ---\n");
    tree_print(root);
    printf("\n");

    printf("\n--- Search for Node 92 (Valid) ---\n");
    Node* node_92 = tree_search(root, 92);
    printf("Number: %d, Color: %s\n", node_92->data, node_92->color == BLACK ? "BLACK" : "RED");

    printf("\n--- Search for Node 1 (Invalid) ---\n");
    Node* node_1 = tree_search(root, 1);
    printf("Failed?: %s\n", node_1 == NULL ? "YES" : "NO");

    tree_destroy(root);
    root = NULL;

    tree_print(root);

    return 0;
}
