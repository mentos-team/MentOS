# VFS and fd lifetime

Verified against `BASE` = `82f4314`; the #189 fix verified merged at `MAIN` =
`62c638a`.

## Structures

- `task->fd_list`: array of `vfs_file_descriptor_t {vfs_file_t *file_struct;
  int flags_mask}`; `task->max_fd` starts at `MAX_OPEN_FD` = 16
  (kernel/inc/fs/vfs.h:13).
- `vfs_file_t` (slab object from `vfs_file_cache`, zeroed at alloc):
  `count` (fd references), `refcount` (pipes use it too), `name`, `flags`,
  `mask`, `uid/gid`, `f_pos`, `device`, `ino`, `fs_operations`,
  `sys_operations`, `siblings` list node.
- Superblock registry: `vfs_super_blocks` list; `vfs_get_superblock(path)`
  picks the filesystem by path prefix.

## THE core invariant (INVARIANT — audit all changes against it)

> Every `fd_list` slot referencing a `vfs_file_t` holds exactly one
> increment of `file->count`. Every removal of a slot reference goes through
> a `close_f` (which itself decrements `count` and frees at 0).

Producers (each verified to increment exactly once per reference):

| Site | How count is taken |
|---|---|
| `sys_open` (kernel/src/fs/open.c) | `vfs_open` → `vfs_open_abspath`: `file->count += 1` after fs open |
| `sys_creat` (kernel/src/fs/namei.c) | `vfs_creat`: `file->count += 1` |
| `sys_dup` (kernel/src/fs/vfs.c) | `file->count += 1` |
| `vfs_dup_task` (fork) | `++count` per inherited slot |
| `create_pipe_fd` (kernel/src/fs/pipe.c) | `pipe_create_file_struct` sets `count = 1` |
| init stdio (process.c, `BASE`:362-378) | `vfs_open` (+1) **plus deliberate extra `count++`** keeping the file alive past the temporary open — balanced overall |

Consumers:
- `sys_close` (open.c:56-77): NULLs the slot, then `vfs_close(file)` →
  validation (`count <= 0` guard, `close_f == NULL` guard) → `close_f(file)`.
- `vfs_destroy_task` (vfs.c): the catch-all for fds still open when a zombie
  is reaped (`sys_waitpid` → `vfs_destroy_task`; only caller verified).
  Post-#189 body (VERIFIED at `MAIN`): for each non-NULL slot, call
  `close_f(file_struct)` directly, NULL the slot, then `kfree(fd_list)`,
  `procr_destroy_entry_pid`.

### The `close_f` contract (VERIFIED across all implementations)

Every `close_f` **decrements `file->count` itself** and, at zero, performs
filesystem-specific teardown then `vfs_dealloc_file(file)`:

- `ext2_close` (ext2.c:3286-3325 @`MAIN`): `--count`; at 0: refuses to free
  `fs->root` (returns -EPERM leaving count at 0), else unlinks from
  `fs->opened_files` and frees.
- `procfs_close` (procfs.c:548-572): `--count`; at 0: unlink + free.
- `pipe_close` (pipe.c:833-898): mode-based reader/writer decrement,
  wakeups, `--count`; at 0: frees pipe_info if readers==writers==0, unlinks,
  frees file. (See pipes.md for the sys_pipe error-path violation.)
- `ata_close`, `null_close`: same `--count`, free-at-0 pattern.

Consequence: **callers must never pre-decrement `count`** — the pre-#189
`vfs_destroy_task` did (`--count` then `close_f` when 0) causing a double
decrement per still-open fd on reap. PR #189 (MERGED) removed the manual
decrement; verified correct against every producer above.

## ext2 per-inode file cache (critical for security reasoning)

`ext2_open` (ext2.c:3095ff @`MAIN`) looks up
`ext2_find_vfs_file_with_inode(fs, ino)` and REUSES the existing
`vfs_file_t` (sharing `count`, `f_pos`, mask/uid/gid) instead of allocating a
fresh one. Two opens of the same file = one struct, count 2.

This converts refcount corruption into privilege escalation: if `count` is
wrongly driven to -1 (the pre-#189 bug), the struct is never freed but is
stale-listed; later `vfs_alloc_file` may hand the SAME slab object to a new
open of a different file while an old fd still points at it. `sys_write`
authorizes on the **per-fd `flags_mask`** (read_write.c:63) and then operates
on the shared `file_struct` → an fd opened read-only can write into a file
whose struct got aliased. PR #189's PoC (in its description) demonstrates the
full chain; the mechanism was verified line-by-line at `BASE`.

## `get_unused_fd` and the (currently dead) growth path

`get_unused_fd` (vfs.c): scans for empty slot; `-EMFILE` when `fd >=
MAX_OPEN_FD` (16); calls `vfs_extend_task_fd_list` only when
`fd == task->max_fd`. Since `max_fd` starts at MAX_OPEN_FD = 16 and fds are
capped at 16, **the extension branch is unreachable today** — max 13
user-usable fds (3 stdio). Two latent defects if that ever changes (see
false-positives doc): `vfs_extend_task_fd_list` memsets only the OLD extent
of the new array, and `vfs_init_task`'s initial `kmalloc`'d list is not
explicitly zeroed (works because boot-time slab pages are fresh/zero).

## Dangerous lifecycle assumptions (do NOT rely on)

1. "exit closes my fds" — false; only reap does (`vfs_destroy_task`).
2. "close_f returns 0 always" — ext2 root returns -EPERM at count 0 without
   freeing (by design); pipe_close can free more than the file.
3. "each open returns a private file struct" — false in ext2 (shared cache,
   shared f_pos).
4. "NULL close_f can't be reached via fd_list" — true today (procfs wraps
   every opened file with `procfs_fs_operations`; the NULL-`close_f` tables
   belong to `proc_dir_entry_t` templates, e.g. procv_/procr_/procs_/
   procipc_/procfb_), VERIFIED via `procfs_open`/`procfs_create_file_struct`.
   But a new filesystem returning an unwrapped struct would crash
   `vfs_destroy_task` — it calls `close_f` unconditionally (post-#189;
   `vfs_close` has the NULL guard, `vfs_destroy_task` does not).
