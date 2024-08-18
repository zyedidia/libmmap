#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "tree.h"

static Node* nadd(Node* n, uint64_t key, uint64_t size, Node* allocn, MMInfo info);
static Node* nremove(Node* n, uint64_t key, Node** removed);
static Node* nsearchaddr(Node* n, uint64_t key);
static Node* nsearchsize(Node* n, uint64_t size);
static Node* nsearchcontains(Node* n, uint64_t key, uint64_t size);
static Node* nbalance(Node* n);
static Node* nrotateright(Node* n);
static Node* nrotateleft(Node* n);
static Node* nfindsmallest(Node* n);
static uint64_t max(uint64_t a, uint64_t b);

void
tput(Tree* t, uint64_t key, uint64_t size, Node* allocn, MMInfo info)
{
    t->root = nadd(t->root, key, size, allocn, info);
}

Node*
tremove(Tree* t, uint64_t key)
{
    Node* removed = NULL;
    t->root = nremove(t->root, key, &removed);
    return removed;
}

Node*
tsearchaddr(Tree* t, uint64_t key)
{
    return nsearchaddr(t->root, key);
}

Node*
tsearchsize(Tree* t, uint64_t size)
{
    return nsearchsize(t->root, size);
}

Node*
tsearchcontains(Tree* t, uint64_t key, uint64_t size)
{
    return nsearchcontains(t->root, key, size);
}

static Node*
nadd(Node* n, uint64_t key, uint64_t size, Node* allocn, MMInfo info)
{
    if (!n) {
        n = allocn;
        assert(allocn);
        *n = (Node) {
            .key = key,
            .size = size,
            .val = info,
            .maxsize = size,
            .maxend = key + size,
            .height = 1,
        };
        return n;
    }

    if (key < n->key) {
        n->left = nadd(n->left, key, size, allocn, info);
    } else if (key > n->key) {
        n->right = nadd(n->right, key, size, allocn, info);
    } else {
        // node already exists
        return NULL;
    }
    return nbalance(n);
}

static Node*
nremove(Node* n, uint64_t key, Node** removed)
{
    if (!n)
        return NULL;
    if (key < n->key) {
        n->left = nremove(n->left, key, removed);
    } else if (key > n->key) {
        n->right = nremove(n->right, key, removed);
    } else {
        if (n->left && n->right) {
            Node* rightmin = nfindsmallest(n->right);
            n->key = rightmin->key;
            n->val = rightmin->val;
            n->size = rightmin->size;
            n->right = nremove(n->right, rightmin->key, removed);
        } else if (n->left) {
            *removed = n;
            n = n->left;
        } else if (n->right) {
            *removed = n;
            n = n->right;
        } else {
            *removed = n;
            return NULL;
        }
    }
    return nbalance(n);
}

static Node*
nsearchaddr(Node* n, uint64_t key)
{
    if (!n)
        return NULL;
    if (key < n->key)
        return nsearchaddr(n->left, key);
    else if (key > n->key)
        return nsearchaddr(n->right, key);
    else
        return n;
}

static int
nheight(Node* n)
{
    return !n ? 0 : n->height;
}
static uint64_t
nmaxsize(Node* n)
{
    return !n ? 0 : n->maxsize;
}
static uint64_t
nmaxend(Node* n)
{
    return !n ? 0 : n->maxend;
}

static Node*
nsearchsize(Node* n, uint64_t size)
{
    if (!n)
        return NULL;
    if (n->size >= size)
        return n;
    else if (nmaxsize(n->left) >= size)
        return nsearchsize(n->left, size);
    else if (nmaxsize(n->right) >= size)
        return nsearchsize(n->right, size);

    assert(n->maxsize < size);
    return NULL;
}

// is [key1, key1+size1) inside [key2, key2+size2)
static bool
contained(uint64_t key1, uint64_t size1, uint64_t key2, uint64_t size2)
{
    return key1 >= key2 && key1 + size1 <= key2 + size2;
}

static Node*
nsearchcontains(Node* n, uint64_t key, uint64_t size)
{
    if (!n)
        return NULL;

    printf("%ld %ld %ld %ld = %d\n", key, size, n->key, n->size, contained(key, size, n->key, n->size));
    if (contained(key, size, n->key, n->size))
        return n;
    else if (n->left && contained(key, size, n->left->key, n->left->maxend))
        return nsearchcontains(n->left, key, size);
    else if (n->right && contained(key, size, n->right->key, n->right->maxend))
        return nsearchcontains(n->right, key, size);
    return NULL;
}

static void
nupdate(Node* n)
{
    n->height = 1 + max(nheight(n->left), nheight(n->right));
    n->maxsize = max(nmaxsize(n->left), nmaxsize(n->right));
    n->maxend = max(nmaxend(n->left), nmaxend(n->right));
}

static Node*
nbalance(Node* n)
{
    if (!n)
        return n;
    nupdate(n);

    int balance = nheight(n->left) - nheight(n->right);
    if (balance <= -2) {
        if (nheight(n->right->left) > nheight(n->right->right))
            n->right = nrotateright(n->right);
        return nrotateleft(n);
    } else if (balance >= 2) {
        if (nheight(n->left->right) > nheight(n->left->left))
            n->left = nrotateleft(n->left);
        return nrotateright(n);
    }
    return n;
}

static Node*
nrotateleft(Node* n)
{
    Node* newroot = n->right;
    n->right = newroot->left;
    newroot->left = n;

    nupdate(n);
    nupdate(newroot);
    return newroot;
}

static Node*
nrotateright(Node* n)
{
    Node* newroot = n->left;
    n->left = newroot->right;
    newroot->right = n;

    nupdate(n);
    nupdate(newroot);
    return newroot;
}

static uint64_t
max(uint64_t a, uint64_t b)
{
    return a > b ? a : b;
}

static Node*
nfindsmallest(Node* n)
{
    if (n->left)
        return nfindsmallest(n->left);
    return n;
}
