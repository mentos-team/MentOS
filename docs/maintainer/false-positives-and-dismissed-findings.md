# False positives and dismissed findings

Everything below was investigated during the #188–#190 review / pre-existing
bug hunt and consciously NOT promoted to a GitHub issue. Recorded so nobody
re-investigates (or worse, re-reports) them without new evidence.
Baselines: `BASE` 82f4314 / `MAIN` 62c638a.

## 1. `sys_waitpid` pid==1 filter quirk

- **Looked suspicious**: `if ((pid > 1) && (child->pid != pid)) continue;`
  (scheduler.c:554 @`MAIN`) — specific-pid matching only applies when
  `pid > 1`, so `waitpid(1, ...)` matches ANY zombie child (like -1).
- **Investigation**: read the full validation block; POSIX says pid>0 waits
  for that child.
- **Actually a bug?** Yes, a minor semantic deviation (pid==1 currently
  behaves like -1).
- **Why not filed**: negligible user impact (nobody waits on pid 1 in
  practice), would be issue-noise; better as a drive-by fix in a future
  syscall-hygiene change. Confidence it's real: 90%; issue-worthiness: 40%.

## 2. `fd_list` initialization and dead extension path

- **Looked suspicious**: `vfs_extend_task_fd_list` memsets only
  `task->max_fd` (OLD extent) of the NEW array (uninitialized tail beyond
  copied region); `vfs_init_task`'s initial list comes from `kmalloc`
  without zeroing; `get_unused_fd` growth branch looked reachable.
- **Investigation**: `max_fd` starts at `MAX_OPEN_FD` = 16 and fds are
  capped at 16 in `get_unused_fd`, so `fd == task->max_fd` never triggers
  extension — the branch is DEAD CODE today (max 13 user fds + 3 stdio).
  Initial kmalloc'ed list works because boot-time slab pages are zero.
- **Actually a bug?** Two latent defects (uninitialized new-array tail;
  reliance on fresh-page zeroing), unreachable at present.
- **Why not filed**: latent-only; fix opportunistely when/if MAX_OPEN_FD
  growth is implemented (or file then, with a reproducer).

## 3. `kfree()` on slab-cache objects (pipe_open / pipe_unlink)

- **Looked suspicious**: `kfree(new_file)` on objects from
  `vfs_alloc_file()` (a kmem_cache).
- **Investigation**: slab.c `pr_kfree` routes via
  `page->container.slab_main_page` → `kmem_cache_free` for slab pages,
  `free_pages_lowmem` otherwise.
- **Actually a bug?** NO — FALSE POSITIVE (allocator handles both).
- **Why not recorded as an issue**: not a bug. Do keep the pipe_unlink
  *bypass of the close_f contract* in mind separately (pipes.md) — that's a
  consistency observation, not an allocator-safety one.

## 4. shell.c SIGCHLD handler error path: `printf("... (%s) ...", SIGCHLD, ...)`

- **Looked suspicious**: int passed to `%s` (shell.c:1398 @`MAIN`) — UB if
  executed.
- **Investigation**: only reachable when `sigaction(SIGCHLD, ...)` fails at
  shell startup; not observed.
- **Actually a bug?** Real latent UB in a practically unreachable path
  (format-string fix is trivial: `("%d")`).
- **Why not filed**: cosmetic; bundle into any shell cleanup PR.

## 5. `__procr_do_stat` self-referential `sprintf(buffer, "%s ...", buffer, ...)`

- **Looked suspicious**: reading and writing the same buffer in sprintf —
  undefined behavior per C standard.
- **Investigation**: pattern used ~30 times in the function; works on the
  current gcc/i386 toolchain (glibc-style sprintf processes the source
  before writing); no overflow observed for current stat content lengths.
- **Actually a bug?** UB-by-standard, benign-by-implementation; also a
  genuine future-maintenance hazard (buffer growth is unchecked).
- **Why not filed**: code-quality concern; mention when touching #194 (same
  function family) rather than as its own issue.

## 6. `sys_close` returns -1 (not -EBADF) for an already-closed fd

- **Looked suspicious**: `return -1;` when `file == NULL` (open.c).
- **Investigation**: userspace `close()` wrappers don't distinguish; errno
  untouched.
- **Actually a bug?** POSIX-deviating errno semantics, cosmetic.
- **Why not filed**: no observable impact found; trivial drive-by fix.

## 7. Tests skipped in runtests / TEST_LIST

- `t_big_write`, `t_periodic1/2/3`, `t_mkdir_nospace` commented out of
  `all_tests[]`; `t_time` disabled in CMake TEST_LIST.
- **Verdict**: INTENTIONAL (explicit comments), not infrastructure rot.
  Do not "fix" silently; re-enabling changes CI meaning (and t_big_write
  predates the #192 panic boundary anyway — it sorts after t_gid).
- `t_mkdir_nospace` (#264) is the one skipped for runtime rather than
  meaning: it fills the 32 MB image block by block to force an allocation
  failure, ~2 min of guest time, because ext2 rewrites the block bitmap,
  the group descriptor and the superblock per allocated block and
  `ext2_find_free_block` rescans from the start each time. Run it by hand
  when touching the ext2 allocation paths.

## 8. Byte-identical `debugfs dump` of t_gid "disproving" the hole

- **Methodology trap hit during #192**: dumping the inode yields a file
  identical to the original BECAUSE holes read as zeros and t_gid's tail
  bytes ARE zeros. The BLOCKS list, not the dump, exposes the hole.
- Recorded here so the next investigator doesn't re-derive the wrong
  conclusion from the same experiment.
