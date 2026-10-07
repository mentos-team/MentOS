# Process lifecycle

Verified against `develop` at `53e7572`.

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

## Single-thread-per-process design assumption

MentOS currently has one schedulable `task_struct` per process. There is no
separate thread-group object, no shared signal state, and no reference-counted
address-space ownership. A PID therefore identifies both the process and its
only schedulable task. This is a design constraint, not a complete thread
model.

Several current behaviors rely on that constraint:

- **Address-space ownership:** each task owns its `mm_struct`. `fork()` clones
  the address space, `execve()` replaces and destroys the old `mm`, and exit
  destroys the task's `mm` before the task is reaped. Threads sharing an `mm`
  would require explicit lifetime management so one thread's `execve()` or
  exit cannot destroy mappings still used by another.
- **Scheduling and kernel execution:** the scheduler switches page directories
  at trap boundaries. `scheduler_reschedule_from_trap()` saves a userspace
  return frame in `thread.regs`; `schedule()` separately saves a live kernel
  continuation in `thread.kernel_esp`. A syscall that encounters a wait
  condition can publish a caller-owned wait entry and suspend its continuation
  with `schedule()`. The continuation resumes at the same call site after
  wakeup; the trap-boundary scheduler must never return that task through the
  interrupted task's userspace frame. See `docs/maintainer/scheduler-contexts.md`.
- **Signals:** pending queues and masks belong to an individual task. Stop and
  continue handling does not coordinate queues or state across a thread group,
  and signal delivery has no group-shared pending queue. Those behaviors are
  currently only meaningful for a one-task process.
- **Process-wide operations:** operations that replace or destroy a process
  image, or change stopped/running state, act directly on the one task. A
  threaded design must define whether each operation targets one thread or
  the whole process, and coordinate concurrent users of shared resources.

These dependencies are marked at their implementation sites in
`process.c`, `mm.c`, `scheduler.c`, `signal.c`, and `pipe.c`; this section is
the shared explanation. Do not interpret the markers as partial thread
support: adding threads requires an explicit task-group model, shared-resource
ownership rules, and resumable per-task kernel execution contexts.

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
- No zombie + WNOHANG → 0; no zombie otherwise → caller-owned wait entry,
  `schedule()`, and a rescan of the children list after wakeup.
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
