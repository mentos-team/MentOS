# AGENTS.md — operational index

Coding style, commit format, and branch/release workflow:
`.github/copilot-instructions.md`. Deep subsystem knowledge:
`docs/maintainer/` (see map below).

## Repository map

| Path | Contents |
|---|---|
| `kernel/src/fs/` | VFS core (vfs.c), ext2, procfs, pipe, open/close/read/write |
| `kernel/src/process/` | process.c (alloc/fork/execve), scheduler.c (waitpid/exit), wait.c |
| `kernel/src/mem/` | paging, page_fault, mm, slab/buddy/zone allocators |
| `kernel/src/elf/` | ELF loader |
| `kernel/src/io/` | console (video.c + `video/` backends) + `/proc` module generators |
| `kernel/src/system/` | syscall dispatch, signals, panic, printk |
| `lib/` | shared freestanding libc (kernel AND userspace) |
| `userspace/bin/` | shell, init, runtests.c (test driver), utilities |
| `userspace/tests/` | t_*.c; built INTO `filesystem/bin/tests` (build dirties the tree) |
| `filesystem/` | rootfs staging tree → rootfs.img via mke2fs |
| `scripts/` | run-qemu-test, tapview |
| `.github/workflows/` | ubuntu.yml (build + test), macos.yml |

## Essential commands

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)          # everything (tests land in filesystem/bin/tests)
make -C build kernel.bin          # kernel only
make -C build qemu-test           # guest test run (WARNING: see below)
make -C build filesystem          # rebuild pristine rootfs.img (QEMU writes to it)
```

## Critical invariants — respect in every change

1. **fd reference invariant**: every `fd_list` slot holds exactly one
   `file->count` reference; removal always goes through `close_f`.
   → `docs/maintainer/vfs-and-fd-lifetime.md`
2. **`close_f` owns count decrements** — callers must never pre-decrement
   (the pre-#189 `vfs_destroy_task` bug).
3. **`do_exit` does NOT close fds** — zombie fds are closed by the reaper
   via `vfs_destroy_task`.
4. **Never trust syscall user pointers or lengths**: no validation layer
   exists (issue #191). No `strcpy`/`strlen`/unbounded walks on user
   memory; bound every copy and force NUL termination
   (`strncpy(dst, src, sizeof(dst)-1); dst[sizeof(dst)-1]=0;`); write at
   most `nbyte` to user buffers; enforce an ARG_MAX with `-E2BIG`.
   → `docs/maintainer/syscall-boundaries.md`, `execve.md`
5. **ext2 block index 0** is a valid sparse hole inside `i_size` (must read
   as zeros) — only invalid for metadata paths (issue #192).
   → `docs/maintainer/ext2.md`

## Test-harness warnings

- **`make qemu-test` exit code 0 does NOT prove the guest tests completed.**
  A kernel panic shuts QEMU down cleanly (exit 0); the CMake target ignores
  exit codes; CI swallows the TAP viewer's failure (issue #193). Currently
  main panics at test 10/39 — t_gid (issue #192). ALWAYS check the serial
  log: `grep -c PANIC` and the last `Running test (n/39)` counter.
- In-guest test output: use `syslog()` (serial); `printf` is VGA-only.
- New tests need BOTH `userspace/tests/CMakeLists.txt` (TEST_LIST) and
  `userspace/bin/runtests.c` (`all_tests[]`).
  → `docs/maintainer/testing-and-ci.md`

## Consult before changing a subsystem

| Changing... | Read first |
|---|---|
| task/fork/exit/waitpid | `docs/maintainer/process-lifecycle.md` |
| execve / ELF / argv | `docs/maintainer/execve.md` |
| fd lists, open/close/dup, file lifetime | `docs/maintainer/vfs-and-fd-lifetime.md` |
| ext2 or the disk image | `docs/maintainer/ext2.md` |
| /proc generation or read handlers | `docs/maintainer/procfs.md` |
| console / video.c / video backends | `docs/maintainer/video-backends.md` |
| pipes | `docs/maintainer/pipes.md` |
| allocators, UAF/double-free analysis | `docs/maintainer/memory-management.md` |
| any syscall | `docs/maintainer/syscall-boundaries.md` |
| CI / tests | `docs/maintainer/testing-and-ci.md` |
| anything security-relevant | `docs/maintainer/security-model.md` + `known-bugs.md` |
| reproducing anything | `docs/maintainer/debugging-playbook.md` |

Before fixing anything, check `docs/maintainer/known-bugs.md` (open issues
#190–#196) and `false-positives-and-dismissed-findings.md` (already-investigated
non-bugs — do not re-report).
