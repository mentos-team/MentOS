# MentOS Maintainer Knowledge Base

Durable, maintainer-grade documentation of architecture, invariants, failure
modes, and debugging knowledge accumulated during a deep security review of
PRs #188–#190 and the follow-up pre-existing-bug investigation (which produced
issues #192–#196).

**Verification baseline commits** (all observations are recorded against one of
these unless stated otherwise):

| Label | SHA | What it is |
|---|---|---|
| `BASE`  | `82f4314` | `main` at the time PRs #188/#189/#190 were reviewed (their common merge-base) |
| `MAIN`  | `62c638a` | `main` after #188 and #189 were merged; the baseline for issues #192–#196 |
| `DEV`   | `9311863` | local `develop` checkout at documentation time (differs from `MAIN`; see investigation-history.md) |

Line numbers cited as `file.c:NNN` are valid **only for the stated commit** and
will drift; function names and call-path descriptions are the stable
references.

## Document index

| Document | Read before / when |
|---|---|
| [architecture.md](architecture.md) | Touching any kernel subsystem; orientation for new agents |
| [process-lifecycle.md](process-lifecycle.md) | Changing task creation, fork, exit, waitpid, zombie reaping |
| [execve.md](execve.md) | Changing `sys_execve`, ELF loading, interpreter handling, argv/env |
| [vfs-and-fd-lifetime.md](vfs-and-fd-lifetime.md) | Changing fd lists, `vfs_file_t` lifetime, open/close/dup/destroy paths |
| [ext2.md](ext2.md) | Changing ext2 inode/block read/write, the image build, or file caching |
| [procfs.md](procfs.md) | Changing `/proc` generation or any proc-module read handler |
| [video-backends.md](video-backends.md) | Changing the console, `video.c`, or any video backend; adding a display mode |
| [pipes.md](pipes.md) | Changing `sys_pipe`, `pipe_close`, pipe create/unlink error paths |
| [memory-management.md](memory-management.md) | Reasoning about kmalloc/kfree/slab, UAF/double-free analysis |
| [syscall-boundaries.md](syscall-boundaries.md) | Adding or modifying ANY syscall that takes user pointers |
| [testing-and-ci.md](testing-and-ci.md) | Running, extending, or trusting the test suite; CI work |
| [security-model.md](security-model.md) | Any security-relevant change; triaging #191-class reports |
| [debugging-playbook.md](debugging-playbook.md) | Before reproducing anything in QEMU; host-side analysis techniques |
| [known-bugs.md](known-bugs.md) | Before fixing anything — check the open-issue landscape first |
| [investigation-history.md](investigation-history.md) | Understanding how the current findings were established |
| [false-positives-and-dismissed-findings.md](false-positives-and-dismissed-findings.md) | Before re-reporting something already investigated |

## Most important global invariants (short list)

1. **fd reference invariant** — every `fd_list` slot referencing a
   `vfs_file_t` holds exactly one increment of `file->count`. Producers:
   `sys_open`, `sys_creat`, `sys_dup`, `vfs_dup_task` (fork),
   `create_pipe_fd`, init stdio setup. See vfs-and-fd-lifetime.md.
2. **`close_f` owns decrements** — every filesystem `close_f` decrements
   `file->count` itself and frees at zero. Callers must never pre-decrement
   (the pre-#189 `vfs_destroy_task` bug did exactly this).
3. **`vfs_destroy_task` is the catch-all fd close** — `do_exit` does NOT close
   fds; the reaper in `sys_waitpid` does, via `vfs_destroy_task`.
4. **Kernel trusts syscall user pointers completely** (VERIFIED: no
   validation layer exists — issue #191). Never add code that dereferences
   user pointers "just a little more" without bounding it.
5. **A `qemu-test` exit code of 0 proves nothing** about guest test
   completion (issue #193). Always check the serial log.
6. **`ext2_read_block`/`ext2_write_block` reject block index 0** — but block
   0 inside `i_size` is a legal sparse hole (issue #192). Metadata paths may
   rely on the check; data paths must treat 0-in-`i_size` as a zero-filled
   hole.

## Related pre-existing documentation

- `.github/copilot-instructions.md` — C coding guidelines, conventional-commit
  format, branch/release workflow. This knowledge base does not duplicate it.
- `README.md` — basic build instructions (cmake + make).
