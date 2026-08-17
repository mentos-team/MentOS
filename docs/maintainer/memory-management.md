# Memory management (as it relates to lifetime/UAF reasoning)

Verified against `MAIN` = `62c638a` unless noted.

## Allocator layers

- Slab caches on top of buddy/zone allocators (kernel/src/mem/alloc/).
  `KMEM_CREATE(type)` creates typed caches (e.g. `task_struct_cache`,
  `vfs_file_cache`, `vfs_superblock_cache` in `vfs_init`).
- `kmem_cache_alloc/kmem_cache_free` for cache objects.
- `kmalloc/kfree` (slab.c `pr_kmalloc`/`pr_kfree`): kmalloc routes to
  order-sized generic caches (`malloc_blocks[order]`).

## KEY FACT — `kfree()` is safe on slab objects (FALSE POSITIVE resolved)

`pr_kfree` (slab.c): resolves `get_page_from_virtual_address(ptr)`; if
`page->container.slab_main_page` is set → `kmem_cache_free(ptr)`, else
`free_pages_lowmem`. Page metadata distinguishes slab vs raw allocations, so
call sites like `pipe_open`/`pipe_unlink` doing `kfree(file)` on a
`vfs_alloc_file()` (cache) object are correct. Do not "fix" these to
`vfs_dealloc_file` on allocator-safety grounds (a consistency argument is
separate and was not evaluated).

## Double-free behavior (basis of #195's impact assessment)

`pr_kmem_cache_free` (slab.c): validates pointer/page/cache, runs optional
dtor, `KMEM_OBJ_FROM_ADDR`, pushes onto the cache freelist. No double-free
detection (no free-list audit / poisoning observed). CONSEQUENCE (INFERENCE
from mechanics, not directly observed): freeing the same object 2–3 times
inserts it 2–3 times into the freelist → subsequent allocations can hand
one memory region to multiple owners → silent cross-type corruption. This is
the stated impact model of issue #195.

`kmem_cache_free` failure paths return nonzero with pr_crit (NULL ptr, no
page, no cache) — they do not abort.

## Allocation zeroing

- `pr_vfs_alloc_file` memsets new `vfs_file_t` to 0 (VERIFIED).
- `__alloc_task` memsets `task_struct` to 0 (VERIFIED).
- kmalloc'd buffers are NOT generally zeroed — code must memset (e.g.
  `__pipe_inode_info_alloc` memsets; `elf_load_file` memsets its read
  buffer).
- Boot-time slab pages come up zeroed (this is why the un-zeroed `kmalloc`
  of the initial `fd_list` in `vfs_init_task` has never misbehaved —
  see false-positives doc).

## Debug options (observed in code)

`ENABLE_KMEM_TRACE`, `ENABLE_CACHE_TRACE`, `ENABLE_FILE_TRACE` — log
alloc/free pairs and resource tracking (used by `register_resource` /
`store_resource_info`). Enable for leak/UAF triage builds.

## mm / paging relationships observed

- `mm_struct` per task: `pgd`, arg/env start/end, code/data/brk extents,
  `total_vm`. `mm_clone` for fork; `mm_destroy` on `do_exit` and **during
  exec before the new image is loaded** (process.c:175 @`MAIN`) — the
  error-path hazard behind #192's panic and likely #121 (see execve.md).
- `paging_switch_pgd` used when loading init and during exec; user stack
  pointers (`thread.regs.useresp`) are then re-based by pushing args.
- Page-fault handler (mem/page_fault.c): prints decode
  `ERR(user rw present)` as `(%d%d%d)`; user-mode fault with non-present
  directory entry → SIGSEGV + scheduler_run; kernel-mode fault →
  `__page_fault_panic` → `kernel_panic`.
- Kernel heap/virtual layout observations: kernel EIP ~0xc00026f9, kernel
  ESP ~0xf75eec48, user stack top ~0xbfffxxxx in the repro runs (useful
  sanity anchors when reading panic logs).

## Knowledge gaps (not verified)

- Buddy/zone internals, dma handling, per-CPU caches if any.
- Where pipe buffer PAGES are allocated/freed (`__pipe_buffer_init` only
  zeroes structs; the page lifecycle lives in the read/write/confirm paths —
  untraced).
- vmem/vm_area machinery details.
