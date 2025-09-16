#pragma once

#include "mmap.h"
#include "tree.h"

#include <stdint.h>
#include <stdio.h>

typedef struct Node {
    uint64_t key;
    uint64_t size;
    // largest size in subtree
    uint64_t maxsize;
    // furthest endpoint in subtree
    uint64_t maxend;
    MMInfo val;

    int height;
    struct Node *left;
    struct Node *right;
} Node;

// is [key1, key1+size1) inside [key2, key2+size2)
static bool
contained(uint64_t key1, uint64_t size1, uint64_t key2, uint64_t size2)
{
    return key1 >= key2 && key1 + size1 <= key2 + size2;
}

// Put a node corresponding to [key, key+size) into the tree. Uses 'allocn' as
// the memory for the node (pre-allocated). The node is associated with 'info'.
void
tput(Tree *t, uint64_t key, uint64_t size, Node *allocn, MMInfo info);

// Remove a node starting at 'key' from the tree.
Node *
tremove(Tree *t, uint64_t key);

// Find a node starting at 'key'.
Node *
tsearchaddr(Tree *t, uint64_t key);

// Find a node with size at least 'size'.
Node *
tsearchsize(Tree *t, uint64_t size);

// Find a node that contains the region [key, key+size)
Node *
tsearchcontains(Tree *t, uint64_t key, uint64_t size);

// Find a node where n->key + n->size == end.
Node *
tsearchend(Tree *t, uint64_t end);

// Return the number of nodes that overlap with [key, key+size).
size_t
tnumoverlaps(Tree *t, uint64_t key, uint64_t size);

// Find nodes that overlap with [key, key+size) and return copies of them. The
// 'nodes' array must be pre-allocated with the correct number of overlapping
// nodes (and also passed via 'noverlaps' for assertion purposes)
void
toverlaps(Tree *t, uint64_t key, uint64_t size, Node **nodes, size_t noverlaps);
