#pragma once

#include <stdint.h>
#include <stdio.h>

#include "mmap.h"
#include "tree.h"

typedef struct Node {
    uint64_t key;
    uint64_t size;
    // largest size in subtree
    uint64_t maxsize;
    // furthest endpoint in subtree
    uint64_t maxend;
    MMInfo val;

    int height;
    struct Node* left;
    struct Node* right;
} Node;

void tput(Tree* t, uint64_t key, uint64_t size, Node* allocn, MMInfo info);

Node* tremove(Tree* t, uint64_t key);

Node* tsearchaddr(Tree* t, uint64_t key);

Node* tsearchsize(Tree* t, uint64_t size);

Node* tsearchcontains(Tree* t, uint64_t key, uint64_t size);
