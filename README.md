# libmmap

A C++ library for tracking virtual memory regions. It manages an address space
bookkeeping structure (mappings, protections, metadata) without performing
actual OS-level `mmap`/`munmap`/`mprotect` calls. Real memory operations are
deferred to the caller via callbacks, making the library suitable for use in
userspace Linux emulators and sandboxing runtimes.

## Components

### RangeMap

`RangeMap<K, V>` is a header-only template that maps non-overlapping half-open
ranges `[start, end)` to values. Backed by `std::map` for O(log n) operations.
Adjacent ranges with equal values are automatically coalesced.

```cpp
mmap::RangeMap<int, int> m;
m.insert(0, 10, 1);       // [0, 10) -> 1
m.insert(5, 15, 2);       // [0, 5) -> 1, [5, 15) -> 2
auto e = m.find(7);       // returns entry [5, 15) -> 2
m.remove(3, 12);          // [0, 3) -> 1, [12, 15) -> 2
```

### AddrSpace

`AddrSpace` tracks virtual memory regions within a fixed address range. It
stores addresses internally in page units and converts at the API boundary.

```cpp
mmap::AddrSpace mm;
mm.init(0x10000, 0x100000, 4096);

uintptr_t p = mm.map_any(4096, PROT_READ, MAP_PRIVATE, -1, 0);

mm.unmap(p, 4096, [](uintptr_t start, size_t len, mmap::MapInfo info) {
    // perform actual munmap here
});
```

## API

### AddrSpace

| Method | Description |
|--------|-------------|
| `init(start, len, pagesize)` | Initialize address space |
| `reset()` | Clear all mappings |
| `map_any(len, prot, flags, fd, offset)` | Map at first available gap |
| `map_at(addr, len, prot, flags, fd, offset, ufn)` | Map at fixed address |
| `unmap(addr, len, ufn)` | Unmap a range |
| `query_page(addr, info)` | Query mapping info for an address |
| `protect(addr, len, prot, ufn)` | Change protection flags |

Callbacks (`UpdateFn`) are invoked for each affected region during `unmap`,
`map_at` (when overwriting), and `protect`. The callback receives the byte
address, length, and the previous `MapInfo` of the affected region.

### RangeMap

| Method | Description |
|--------|-------------|
| `insert(start, end, val)` | Insert range, splitting/replacing overlaps |
| `remove(start, end)` | Remove range, trimming partial overlaps |
| `find(key)` | Find entry containing a point |
| `overlaps(start, end)` | Check if any entry overlaps a range |
| `get_overlapping(start, end)` | Get all entries overlapping a range |
| `get_gaps(start, end)` | Get unmapped sub-ranges within a range |

## Building

```sh
meson setup build
ninja -C build
meson test -C build
```
