#include "mmap_internal.h"

#include <stdlib.h>
#include <string.h>

static bool mapinfo_equal(const struct MMapInfo *a, const struct MMapInfo *b) {
  return a->prot == b->prot && a->flags == b->flags && a->fd == b->fd &&
         a->offset == b->offset && a->original == b->original;
}

static struct RangeNode *alloc_node(uint64_t start, uint64_t end,
                                    struct MMapInfo info) {
  struct RangeNode *n = malloc(sizeof(struct RangeNode));
  if (!n)
    return NULL;
  n->start = start;
  n->end = end;
  n->info = info;
  n->left = NULL;
  n->right = NULL;
  n->height = 1;
  return n;
}

static int avl_height(const struct RangeNode *n) { return n ? n->height : 0; }

static int avl_balance(const struct RangeNode *n) {
  return n ? avl_height(n->left) - avl_height(n->right) : 0;
}

static void avl_update_height(struct RangeNode *n) {
  int lh = avl_height(n->left);
  int rh = avl_height(n->right);
  n->height = 1 + (lh > rh ? lh : rh);
}

static struct RangeNode *avl_rotate_right(struct RangeNode *y) {
  struct RangeNode *x = y->left;
  struct RangeNode *t = x->right;
  x->right = y;
  y->left = t;
  avl_update_height(y);
  avl_update_height(x);
  return x;
}

static struct RangeNode *avl_rotate_left(struct RangeNode *x) {
  struct RangeNode *y = x->right;
  struct RangeNode *t = y->left;
  y->left = x;
  x->right = t;
  avl_update_height(x);
  avl_update_height(y);
  return y;
}

static struct RangeNode *avl_rebalance(struct RangeNode *n) {
  avl_update_height(n);
  int bal = avl_balance(n);

  if (bal > 1) {
    if (avl_balance(n->left) < 0)
      n->left = avl_rotate_left(n->left);
    return avl_rotate_right(n);
  }

  if (bal < -1) {
    if (avl_balance(n->right) > 0)
      n->right = avl_rotate_right(n->right);
    return avl_rotate_left(n);
  }

  return n;
}

static struct RangeNode *avl_insert_node(struct RangeNode *root,
                                         struct RangeNode *node) {
  if (!root)
    return node;
  if (node->start < root->start)
    root->left = avl_insert_node(root->left, node);
  else
    root->right = avl_insert_node(root->right, node);
  return avl_rebalance(root);
}

static struct RangeNode *avl_min(struct RangeNode *n) {
  while (n->left)
    n = n->left;
  return n;
}

// Remove the node with the given start key. Sets *removed to the removed
// node (not freed). Returns the new subtree root.
static struct RangeNode *avl_remove_key(struct RangeNode *root, uint64_t start,
                                        struct RangeNode **removed) {
  if (!root) {
    *removed = NULL;
    return NULL;
  }

  if (start < root->start) {
    root->left = avl_remove_key(root->left, start, removed);
  } else if (start > root->start) {
    root->right = avl_remove_key(root->right, start, removed);
  } else {
    // Found the node.
    if (!root->left || !root->right) {
      struct RangeNode *child = root->left ? root->left : root->right;
      *removed = root;
      root->left = NULL;
      root->right = NULL;
      return child;
    }
    // 2 children: replace with in-order successor.
    struct RangeNode *succ = avl_min(root->right);
    // Copy successor's data into root.
    root->start = succ->start;
    root->end = succ->end;
    root->info = succ->info;
    // Remove the successor from the right subtree.
    root->right = avl_remove_key(root->right, succ->start, removed);
    // The logically removed node is root (with old data), but we reused it.
    // *removed was set to succ by the recursive call.
  }

  return avl_rebalance(root);
}

static struct RangeNode *inorder_pred(const struct RangeNode *root,
                                      uint64_t start) {
  const struct RangeNode *best = NULL;
  const struct RangeNode *n = root;
  while (n) {
    if (n->start < start) {
      best = n;
      n = n->right;
    } else {
      n = n->left;
    }
  }
  return (struct RangeNode *)best;
}

static struct RangeNode *inorder_succ(const struct RangeNode *root,
                                      uint64_t start) {
  const struct RangeNode *best = NULL;
  const struct RangeNode *n = root;
  while (n) {
    if (n->start > start) {
      best = n;
      n = n->left;
    } else {
      n = n->right;
    }
  }
  return (struct RangeNode *)best;
}

struct RangeNode *range_find(const struct RangeNode *root, uint64_t key) {
  const struct RangeNode *n = root;
  while (n) {
    if (key < n->start)
      n = n->left;
    else if (key >= n->end)
      n = n->right;
    else
      return (struct RangeNode *)n;
  }
  return NULL;
}

bool range_overlaps(const struct RangeNode *root, uint64_t start,
                    uint64_t end) {
  if (!root || start >= end)
    return false;
  // No overlap if entirely left or right.
  if (end <= root->start)
    return range_overlaps(root->left, start, end);
  if (start >= root->end)
    return range_overlaps(root->right, start, end);
  return true;
}

// Recursive in-order traversal for overlapping nodes.
static bool
get_overlapping_impl(const struct RangeNode *node, uint64_t start, uint64_t end,
                     bool (*cb)(uint64_t, uint64_t, struct MMapInfo, void *),
                     void *udata) {
  if (!node)
    return true;
  // If this node is entirely to the right, only check left subtree.
  if (node->start >= end)
    return get_overlapping_impl(node->left, start, end, cb, udata);
  // If this node is entirely to the left, only check right subtree.
  if (node->end <= start)
    return get_overlapping_impl(node->right, start, end, cb, udata);
  // Node overlaps. Visit left, this node, then right (in-order).
  if (!get_overlapping_impl(node->left, start, end, cb, udata))
    return false;
  if (!cb(node->start, node->end, node->info, udata))
    return false;
  return get_overlapping_impl(node->right, start, end, cb, udata);
}

void range_get_overlapping(
    const struct RangeNode *root, uint64_t start, uint64_t end,
    bool (*cb)(uint64_t, uint64_t, struct MMapInfo, void *), void *udata) {
  if (start >= end)
    return;
  get_overlapping_impl(root, start, end, cb, udata);
}

// Recursive in-order traversal for gaps.
static bool get_gaps_impl(const struct RangeNode *node, uint64_t start,
                          uint64_t end, uint64_t *cursor,
                          bool (*cb)(uint64_t, uint64_t, void *), void *udata) {
  if (!node)
    return true;
  // If this node is entirely to the right, only check left subtree.
  if (node->start >= end)
    return get_gaps_impl(node->left, start, end, cursor, cb, udata);
  // If this node is entirely to the left, only check right subtree.
  if (node->end <= start)
    return get_gaps_impl(node->right, start, end, cursor, cb, udata);
  // Node overlaps. Visit left, process this node, then right.
  if (!get_gaps_impl(node->left, start, end, cursor, cb, udata))
    return false;
  if (node->start > *cursor) {
    if (!cb(*cursor, node->start, udata))
      return false;
  }
  if (node->end > *cursor)
    *cursor = node->end;
  return get_gaps_impl(node->right, start, end, cursor, cb, udata);
}

void range_get_gaps(const struct RangeNode *root, uint64_t start, uint64_t end,
                    bool (*cb)(uint64_t, uint64_t, void *), void *udata) {
  if (start >= end)
    return;
  uint64_t cursor = start;
  get_gaps_impl(root, start, end, &cursor, cb, udata);
  if (cursor < end)
    cb(cursor, end, udata);
}

void range_update_all(struct RangeNode *root,
                      void (*cb)(struct MMapInfo *, void *), void *udata) {
  if (!root)
    return;
  range_update_all(root->left, cb, udata);
  cb(&root->info, udata);
  range_update_all(root->right, cb, udata);
}

void range_free_all(struct RangeNode *root) {
  if (!root)
    return;
  range_free_all(root->left);
  range_free_all(root->right);
  free(root);
}

size_t range_count(const struct RangeNode *root) {
  if (!root)
    return 0;
  return 1 + range_count(root->left) + range_count(root->right);
}

// Find the first overlapping node (by start key).
static struct RangeNode *find_first_overlap(struct RangeNode *root,
                                            uint64_t start, uint64_t end) {
  struct RangeNode *result = NULL;
  struct RangeNode *n = root;
  while (n) {
    if (n->end <= start) {
      n = n->right;
    } else if (n->start >= end) {
      n = n->left;
    } else {
      // n overlaps but there might be an earlier one in the left subtree.
      result = n;
      n = n->left;
    }
  }
  return result;
}

struct RangeNode *range_insert(struct RangeNode *root, uint64_t start,
                               uint64_t end, struct MMapInfo info, bool *err) {
  if (start >= end)
    return root;

  // Pre-allocate the new node.
  struct RangeNode *new_node = alloc_node(start, end, info);
  if (!new_node) {
    if (err)
      *err = true;
    return root;
  }

  // Track edge stubs.
  uint64_t left_start = 0, left_end = 0;
  struct MMapInfo left_info = {0};
  bool has_left = false;

  uint64_t right_start = 0, right_end = 0;
  struct MMapInfo right_info = {0};
  bool has_right = false;

  // Remove all overlapping nodes.
  struct RangeNode *overlap;
  while ((overlap = find_first_overlap(root, start, end)) != NULL) {
    if (overlap->start < start) {
      left_start = overlap->start;
      left_end = start;
      left_info = overlap->info;
      has_left = true;
    }
    if (overlap->end > end) {
      right_start = end;
      right_end = overlap->end;
      right_info = overlap->info;
      has_right = true;
    }
    struct RangeNode *removed = NULL;
    root = avl_remove_key(root, overlap->start, &removed);
    if (removed)
      free(removed);
  }

  // Insert edge stubs.
  if (has_left) {
    struct RangeNode *stub = alloc_node(left_start, left_end, left_info);
    if (!stub) {
      free(new_node);
      if (err)
        *err = true;
      return root;
    }
    root = avl_insert_node(root, stub);
  }
  if (has_right) {
    struct RangeNode *stub = alloc_node(right_start, right_end, right_info);
    if (!stub) {
      free(new_node);
      if (err)
        *err = true;
      return root;
    }
    root = avl_insert_node(root, stub);
  }

  // Insert the new range.
  root = avl_insert_node(root, new_node);

  // Coalesce with in-order predecessor.
  struct RangeNode *pred = inorder_pred(root, new_node->start);
  if (pred && pred->end == new_node->start &&
      mapinfo_equal(&pred->info, &new_node->info)) {
    // Extend pred to cover new_node.
    pred->end = new_node->end;
    struct RangeNode *removed = NULL;
    root = avl_remove_key(root, new_node->start, &removed);
    if (removed)
      free(removed);
    new_node = pred;
  }

  // Coalesce with in-order successor.
  struct RangeNode *succ = inorder_succ(root, new_node->start);
  if (succ && new_node->end == succ->start &&
      mapinfo_equal(&new_node->info, &succ->info)) {
    new_node->end = succ->end;
    struct RangeNode *removed = NULL;
    root = avl_remove_key(root, succ->start, &removed);
    if (removed)
      free(removed);
  }

  if (err)
    *err = false;
  return root;
}

struct RangeNode *range_remove(struct RangeNode *root, uint64_t start,
                               uint64_t end, bool *err) {
  if (start >= end)
    return root;

  // Track edge stubs.
  uint64_t left_start = 0, left_end = 0;
  struct MMapInfo left_info = {0};
  bool has_left = false;

  uint64_t right_start = 0, right_end = 0;
  struct MMapInfo right_info = {0};
  bool has_right = false;

  // Remove all overlapping nodes.
  struct RangeNode *overlap;
  while ((overlap = find_first_overlap(root, start, end)) != NULL) {
    if (overlap->start < start) {
      left_start = overlap->start;
      left_end = start;
      left_info = overlap->info;
      has_left = true;
    }
    if (overlap->end > end) {
      right_start = end;
      right_end = overlap->end;
      right_info = overlap->info;
      has_right = true;
    }
    struct RangeNode *removed = NULL;
    root = avl_remove_key(root, overlap->start, &removed);
    if (removed)
      free(removed);
  }

  // Reinsert edge stubs.
  if (has_left) {
    struct RangeNode *stub = alloc_node(left_start, left_end, left_info);
    if (!stub) {
      if (err)
        *err = true;
      return root;
    }
    root = avl_insert_node(root, stub);
  }
  if (has_right) {
    struct RangeNode *stub = alloc_node(right_start, right_end, right_info);
    if (!stub) {
      if (err)
        *err = true;
      return root;
    }
    root = avl_insert_node(root, stub);
  }

  if (err)
    *err = false;
  return root;
}
