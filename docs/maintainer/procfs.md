# procfs

Verified against `MAIN` = `62c638a` (line refs for `MAIN`).

## Structure

- procfs keeps a global list of `procfs_file_t` entries (name, inode, flags,
  timestamps, `dir_entry` with per-entry `sys_operations`/`fs_operations`,
  and a list of associated open `vfs_file_t`s).
- Module entries are registered by `proc_create_entry`/`proc_mkdir` with
  module-specific ops tables: `proc_running.c` (`/proc/<pid>/{cmdline,stat}`),
  `proc_system.c`, `proc_ipc.c`, `proc_video.c` (`/proc/video` — the console
  device used for init stdio), `proc_feedback.c`.
- **Template tables vs open files (surprising, verified)**: the module
  `vfs_file_operations_t` tables (procr_/procs_/procipc_/procv_/procfb_)
  frequently have `.close_f = NULL`, and procr_/procv_ also `open_f = NULL`.
  These live on the `proc_dir_entry_t` *templates*. When a file is actually
  opened, `procfs_open` (procfs.c:441 @`MAIN`) wraps it via
  `procfs_create_file_struct` which assigns `procfs_fs_operations`
  (`close_f = procfs_close`, valid). Hence NULL `close_f` is unreachable
  through any `fd_list` today — VERIFIED by enumerating all ops tables and
  all sites assigning `fs_operations` on `vfs_file_t`.

## Per-process entries

`procr_create_entry_pid(task)` creates `/proc/<pid>/{cmdline,stat}` on task
creation; `procr_destroy_entry_pid(task)` removes them during reap (called
from `vfs_destroy_task`). Entry `data` points back at the `task_struct`.

## Read path

`sys_read` → `vfs_read` → `procfs_read` (procfs.c) → looks up the entry by
inode → dispatches to the module's `dir_entry.fs_operations->read_f` →
e.g. `__procr_read` (proc_running.c:391-422 @`MAIN`) for cmdline/stat:

1. Builds content into `char support[BUFSIZ]` (512) —
   `__procr_do_cmdline` (strcpy of `task->name`!) or `__procr_do_stat`.
2. Computes `bytes_to_read = max(0, min(strlen(support) - offset, nbyte))`
   — correct arithmetic.
3. Copies with **`strcpy(buffer, support + offset)`** — writes
   `strlen(support) - offset + 1` bytes into the raw user `buffer`,
   ignoring `nbyte`. → **issue #194** (EMPIRICALLY REPRODUCED:
   `read(fd, char[8], 4)` on `/proc/<pid>/stat` returned 4 and destroyed the
   caller's stack → user-mode page fault `ERR(0)…(100)` → SIGSEGV).

Correct fix: `memcpy(buffer, support + offset, bytes_to_read)`.
Audit siblings (`proc_system.c`, `proc_ipc.c`, `proc_video.c`,
`proc_feedback.c`) for the same pattern before assuming only procr_ is
affected (pattern presence elsewhere: INFERENCE — not audited file-by-file
in this investigation).

## `task->name` consumers (relevant to PR #190's termination gap)

- `__procr_do_cmdline` (proc_running.c:58-62 @`MAIN`):
  `strcpy(buffer, task->name)` — ignores its `bufsize` parameter AND depends
  on `task->name` being NUL-terminated (not guaranteed post-#190 for
  ≥255-char names).
- `__procr_do_stat` (proc_running.c:82): `basename(task->name)` in a
  `sprintf("%s (%s)")` — same dependence.
- Plus ~15 `pr_debug`/`pr_info` `%s` sites across scheduler/wait/feedback.

## Related code-quality findings (not filed; see false-positives doc)

- `__procr_do_stat` builds output via repeated self-referential
  `sprintf(buffer, "%s ...", buffer, ...)` — UB per the C standard, works on
  the current gcc/i386 toolchain; `strcat(buffer, " 0")` is used elsewhere
  in the same function.
- `__procr_read`'s `support` is 512 bytes; a long `task->name` or many stat
  fields could overflow it via the same strcpy/sprintf patterns — combined
  risk with #196 (unbounded name length) is theoretical but worth a look
  when fixing #194 (HYPOTHESIS).

## /proc/video

`procv_module_init` registers the `video` entry with read/write/ioctl ops
and NULL close_f on the template; init's stdio opens it (wrapped by
procfs_fs_operations on open files, so closing works). Writes go to the
console; reads from the keyboard buffer.
