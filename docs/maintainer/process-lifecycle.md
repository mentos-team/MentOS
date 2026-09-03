# Process lifecycle

Verified against `BASE` = `82f4314`; spot-rechecked at `MAIN` = `62c638a`
(where noted).

## Structures

- `task_struct` (kernel/inc/process/process.h) — allocated from
  `task_struct_cache` (KMEM_CREATE slab) and fully `memset` to 0 at alloc in
  `__alloc_task` (VERIFIED, process.c). Relevant fields:
  - `fd_list` (`vfs_file_descriptor_t *`: `{file_struct, flags_mask}`),
    `max_fd`
  - `name[TASK_NAME_MAX_LENGTH]` (100 at `BASE`/`MAIN`; PR #190 changes the
    macro to `NAME_MAX`=255), `cwd[PATH_MAX]`
  - `mm` (mm_struct), `thread.regs` (eip/useresp/eax/...), `state`,
    `exit_code`, `parent`, `children`/`sibling` list heads, `run_list`
  - signal state: `sighand`, `blocked`, `pending`, `waiting_on`
- `init_process` global; init may not call exit (kernel_panic guard).

## Creation — `__alloc_task(source, parent, name)` (process.c)

1. slab-alloc + zero.
2. `pid_manager_get_free_pid()`.
3. fd list: `vfs_dup_task(proc, source)` if `source`, else `vfs_init_task(proc)`.
4. lists init (`run_list`, `children`, `sibling`), parent linkage.
5. `strcpy(proc->name, name)` (bounded by PR #190 to TASK_NAME_MAX_LENGTH);
   `cwd` copied from source or `"/"` (bounded to PATH_MAX by PR #190; source
   is always `resolve_path` output, which is NUL-terminated by construction —
   see execve.md "termination guarantees").
6. `memcpy` of thread context from source if fork.

## Fork — `sys_fork` (process.c)

- `scheduler_store_context(f, current)`; `__alloc_task(current, current,
  current->name)`; `proc->mm = mm_clone(current->mm)`; child `eax = 0`;
  inherits sid/pgid/uid/ruid/gid/rgid; `scheduler_enqueue_task(proc)`;
  parent gets pid.
- `vfs_dup_task` copies the fd array and does `++file_struct->count` per
  non-NULL slot, then `procr_create_entry_pid(task)` and
  `vfs_update_pipe_counts` (increments pipe readers/writers for inherited
  FIFO fds).

## Exit — `do_exit` (scheduler.c)

VERIFIED at `BASE` (scheduler.c:581) and unchanged in essence at `MAIN`:

1. init guard (kernel_panic).
2. `exit_code` stored, `state = EXIT_ZOMBIE`.
3. SIGCHLD to parent + `wake_up_process_on_queue(&waitpid_queue, parent)`.
4. Children re-parented to init (list splice).
5. `mm_destroy(runqueue.curr->mm)`.
6. **Does NOT close fds.** The zombie keeps its `fd_list` until reaping.
   (This is why `vfs_destroy_task` is the catch-all close path and why the
   pre-#189 double-decrement fired for every fd left open at exit.)

## Reap — `sys_waitpid` (scheduler.c)

Verified at `MAIN` (scheduler.c:511ff):

- Validates: `pid < -1 || pid == 0` → ESRCH; `pid == self` → ECHILD;
  options restricted to WNOHANG|WUNTRACED.
- Scans `children` for `EXIT_ZOMBIE`; matches specific pid only when
  `pid > 1` (see false-positives doc for the pid==1 quirk).
- Reap sequence: `pid_manager_mark_free` → `vfs_destroy_task(child)` →
  unlink from parent's children → dequeue if still on runqueue →
  `kmem_cache_free(child)`.
- No zombie + WNOHANG → 0; no zombie otherwise → `sleep_on(&waitpid_queue)`
  and returns `-EINTR` (userspace must retry).
- Reaping may also occur in the scheduler when a zombie becomes current
  (comment at `MAIN`), guarded by `list_head_empty(&child->run_list)`.

## Invariants (INVARIANT)

1. Exactly one `task_struct` per live/zombie pid while in `children` list;
   freed exactly once, by the reaper.
2. A zombie's `mm` is destroyed but its fd list and proc entry persist until
   reap. Any code walking a zombie's fds must therefore be robust to files
   whose backing was already released elsewhere.
3. Every fd slot referencing a file holds one `count` reference (see
   vfs-and-fd-lifetime.md) — `vfs_dup_task` and `vfs_destroy_task` are the
   fork/destroy pair that must balance.
4. `task->cwd` is always a NUL-terminated absolute path (producers:
   `__alloc_task`, `sys_chdir`, `sys_fchdir`, all via `resolve_path`).

## Known issues touching this area

- Pre-#189 `vfs_destroy_task` double-decrement (FIXED by merged PR #189 —
  verified at `MAIN`).
- Failed exec destroys `mm` then returns to userspace → kernel panic
  (issue #192 secondary effect / likely #121). See execve.md.
