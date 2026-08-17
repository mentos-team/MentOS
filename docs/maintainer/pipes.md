# Pipes

Verified against `MAIN` = `62c638a` (line refs for `MAIN`). Core file:
kernel/src/fs/pipe.c.

## Structures

- `pipe_inode_info_t` (`pipe_info`): wait queues `read_wait`/`write_wait`,
  mutex, `numbuf` (= PIPE_NUM_BUFFERS) `pipe_buffer_t`s, read/write indices,
  `readers`/`writers` counts.
- `__pipe_inode_info_alloc` (pipe.c:145): kmalloc + memset 0, wait-queue
  init, buffer structs zero-initialized (`__pipe_buffer_init` sets
  len/offset/ops; **no pages allocated at init**), `readers = writers = 0`.
- `__pipe_inode_info_dealloc` (pipe.c:195): `__pipe_buffer_deinit` each buf
  (resets fields only — page freeing happens elsewhere; not traced in this
  investigation — see memory-management.md gaps), then `kfree(pipe_info)`.

## File creation

`pipe_create_file_struct` (pipe.c:619): slab-alloc `vfs_file_t` via
`vfs_alloc_file`, zeroes it, `flags = S_IFIFO | (flags & O_ACCMODE)`,
`fs_operations = &pipe_fs_operations`, `refcount = 1`, **`count = 1`**
(pipe.c:664 — the fd reference), name from path (named pipes).

Anonymous: `sys_pipe` (pipe.c:1207) →
1. `__pipe_inode_info_alloc(&anonymous_pipe_ops)`.
2. buffer-confirm loop.
3. `create_pipe_fd(pipe_info, O_RDONLY)` + `create_pipe_fd(O_WRONLY)`
   (pipe.c:1127): allocates the file, `file->device = pipe_info`,
   `get_unused_fd()`; on fd failure logs `"Failed to allocate file
   descriptor."` (pipe.c:1152) and calls **`pipe_close(file)`**.
4. **Only after both fds succeed**: `pipe_info->readers = 1; writers = 1`
   (pipe.c:1252-1253).

Named: `pipe_open` (pipe.c:697) — first scans the caller's fd_list for an
existing FIFO with the same name and access mode and REUSES it (no new
count); else allocates file + pipe_info. `sys_open` of a named pipe ends
here via the normal `count += 1` path.

## `pipe_close` (pipe.c:833-898) — the contract

1. Validate file/device.
2. Decrement `readers` or `writers` by `file->flags & O_ACCMODE` (with
   already-zero warnings — pipe.c:855/864).
3. If `writers == 0`: wake readers.
4. `--file->count`; at 0: if `readers == 0 && writers == 0` →
   `__pipe_inode_info_dealloc(pipe_info)`; unlink `file->siblings`;
   `vfs_dealloc_file(file)`.

## Fork interaction

`vfs_dup_task` → `vfs_update_pipe_counts` (pipe.c:1160): for each inherited
FIFO fd, `++readers` or `++writers` by mode.

## Unlink

`pipe_unlink` (pipe.c ~795-825): scans fd_list by name, and on match:
`list_head_remove(&file->siblings)`, `__pipe_inode_info_dealloc(device)`,
`kfree(file)` — bypasses the `close_f` contract entirely (direct kfree of a
slab object — SAFE per allocator routing, see memory-management.md, but
leaves accounting to the caller's fd slot clearing). Not deeply audited.

## Issue #195 — the multi-free chain (VERIFIED FACT + EMPIRICALLY
REPRODUCED)

During `sys_pipe`, `readers == writers == 0` for the whole creation phase
(they're set last). Therefore any `pipe_close` during creation satisfies the
`count`-0 AND `readers==0 && writers==0` conditions and frees `pipe_info`.

Failure sequence with the fd table exhausted (`get_unused_fd` → -EMFILE):

1. `create_pipe_fd(O_RDONLY)` fails → `pipe_close(file1)`:
   readers/writers stay 0, `--count → 0` → **frees pipe_info (#1)**.
2. `create_pipe_fd(O_WRONLY)` fails → `pipe_close(file2)` on the SAME,
   already-freed pipe_info (device pointer copied before failure):
   operates on freed memory, `--count → 0` again → **frees again (#2)**.
3. `sys_pipe` error path: `sys_close` of any created fd (partial-failure
   case: another free through pipe_close) plus unconditional
   `__pipe_inode_info_dealloc(pipe_info)` (pipe.c:1248) → **free #3**.

Reproduction (issue #195, test `t_aab_pipe`): exhaust fds by opening `/`
repeatedly (observed: 12 opens then errno 24 EMFILE), call `pipe()`.
Serial signature observed:

```
[ER | pipe.c:1152] Failed to allocate file descriptor.
[WR | pipe.c:864 ] Readers count is already zero.
[ER | pipe.c:1152] Failed to allocate file descriptor.
[WR | pipe.c:855 ] Writers count is already zero.
[ER | pipe.c:1241] Failed to create file descriptors.
```

`pipe()` returns -1 and the kernel survives — the slab freelist now holds
the object multiple times (aliasing on future allocations is the EXPECTED
effect; not directly observed — INFERENCE from allocator mechanics).

## Fix direction (recorded in #195)

Make ownership unambiguous: set readers/writers per end as created (not only
after both succeed), or make creation failure not go through `pipe_close`
(free the file directly), and drop the unconditional dealloc in `sys_pipe`'s
error path once close owns the lifetime.

## Not investigated

- Blocking semantics (`pipe_read`/`pipe_write` wait loops, wakeups) beyond
  what `pipe_close` shows; pipe buffer page alloc/dealloc sites
  (`pipe_buffer_confirm` etc.).
