#ifndef MMAP_INTERNAL_H
#define MMAP_INTERNAL_H

#include "mmap.h"

struct RangeNode {
  uint64_t start; // inclusive, page units
  uint64_t end;   // exclusive, page units
  struct MMapInfo info;
  struct RangeNode *left;
  struct RangeNode *right;
  int height;
};

// Find the node whose range [start, end) contains 'key'.
// Returns NULL if no such node.
struct RangeNode *range_find(const struct RangeNode *root, uint64_t key);

// Insert range [start, end) with given info. Overlapping ranges are
// split/removed. Adjacent ranges with equal info are coalesced.
// Returns the new tree root. Sets *err = true on allocation failure.
struct RangeNode *range_insert(struct RangeNode *root, uint64_t start,
                               uint64_t end, struct MMapInfo info, bool *err);

// Remove all mappings within [start, end). Partially overlapping ranges
// are trimmed/split. Returns the new tree root. Sets *err = true on
// allocation failure (splitting requires one malloc).
struct RangeNode *range_remove(struct RangeNode *root, uint64_t start,
                               uint64_t end, bool *err);

// Return true if any node overlaps [start, end).
bool range_overlaps(const struct RangeNode *root, uint64_t start, uint64_t end);

// Call cb for each node overlapping [start, end), in order.
// Callback returns false to stop iteration.
void range_get_overlapping(
    const struct RangeNode *root, uint64_t start, uint64_t end,
    bool (*cb)(uint64_t, uint64_t, struct MMapInfo, void *), void *udata);

// Call cb for each gap within [start, end), in order.
// Callback returns false to stop iteration.
void range_get_gaps(const struct RangeNode *root, uint64_t start, uint64_t end,
                    bool (*cb)(uint64_t, uint64_t, void *), void *udata);

// Call cb for every node in the tree (in-order traversal).
void range_update_all(struct RangeNode *root,
                      void (*cb)(struct MMapInfo *, void *), void *udata);

// Free all nodes in the tree.
void range_free_all(struct RangeNode *root);

// Return the number of nodes in the tree.
size_t range_count(const struct RangeNode *root);

#endif // MMAP_INTERNAL_H
