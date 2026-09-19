# Syscall boundaries

Verified against `BASE` = `82f4314` and `MAIN` = `62c638a`.

## The one-sentence reality (VERIFIED FACT)

Syscall handlers take raw pointers from `pt_regs` (ebx/ecx/edx) and
dereference them in kernel space with supervisor rights, so the hardware
protects nothing. There was **no user-pointer validation layer** at all
until #191 stage one; there is one now, and each syscall has to opt into
it by hand. Umbrella issue **#191** ("security: Lack of userspace pointer
validation in syscalls") stays open until every pointer-taking syscall is
either gated or listed below with the reason it is not.

## Observed patterns per syscall (not exhaustive — audited where the investigation touched)

| Syscall | User inputs | Handling |
|---|---|---|
| `execve` | filename, argv, envp | NULL checks only; `strcpy`/`strncpy` + unbounded walks (`__count_args`, `__count_args_bytes`, `__push_args_on_stack`) — see execve.md, #196 |
| `read` | buf | **Gated since #191 stage one**: `paging_is_user_range(buf, nbytes)` → `-EFAULT`; then passed to fs `read_f` which copies INTO user memory. The gate bounds the buffer, not the copy: honouring `nbytes` stays the fs layer's own duty, which procfs did not do until #282 closed #194 |
| `write` | buf | **Gated since #191 stage one**: `paging_is_user_range` → `-EFAULT`; then passed to fs `write_f`; authorized by per-fd `flags_mask` ONLY (read_write.c:63) |
| `time` | time_t * | **Gated since #191 stage one**: NULL stays legitimate (POSIX, and the kernel's own callers pass it); any other pointer must be user memory |
| `pipe` | fds[2] | **Gated since #191 stage one**: the two descriptors are only stored through a validated pointer, and NULL no longer has a check of its own — it fails the gate and reports `-EFAULT` like every other pointer the caller does not own, instead of the bare `-1` it used to return |
| `waitpid` | status | **Gated since #191 stage two**: NULL stays legitimate, any other pointer must be writable user memory before `*status = child->exit_code` |
| open/close/chdir/etc. | path strings | `resolve_path` copies into kernel `PATH_MAX` buffers (bounded), but the SOURCE `path` is walked unbounded by tokenizers/strlen inside resolve and fs layers (INFERENCE: same class, not separately reproduced). Since #284 the walk no longer truncates: a component longer than `NAME_MAX - 1` characters, or a token that does not fit the caller buffer, fails with `-ENAMETOOLONG` instead of resolving to a shorter name |

## The user-pointer validation primitives (#191 stages one and two)

`paging_is_user_range(address, length)` (kernel/src/mem/paging.c) answers
whether a range belongs to the current task's user address space: it must
stay below `PROCAREA_END_ADDR`, and every page it covers must be present
and user-marked in the current page directory. Two details that are easy
to get wrong and were learned the hard way:

- the current directory comes out of CR3 as a **physical** address and
  must go through `get_page_from_physical_address` +
  `get_virtual_address_from_page` before anything reads it — the way the
  page-fault handler already does;
- the `user` bit cannot be skipped even though the range check keeps the
  kernel area out: the kernel dereferences these pointers with supervisor
  rights, and every address space inherits the identity-mapped first
  megabyte (supervisor-only) from the main directory, which the range
  check alone would happily accept.

Stage two added the two answers the first version could not give, and a
second primitive:

- `paging_is_user_range_writable(address, length)` is the write direction.
  It also requires `rw` on both levels, because the kernel writes through
  these pointers with supervisor rights and `CR0.WP` is never set, so the
  hardware would not refuse a read-only user page. Use it wherever the
  kernel *writes* through a caller pointer, and the read-only variant
  wherever it only reads.
- a page that is not faulted in yet is no longer refused: when the page
  tables say `user` but not `present`, the address is looked up in the
  current task's `mmap_list`, and an area containing it makes the page the
  caller's. Note that `vm_area_find` cannot be used for this — it matches
  an area by its exact `vm_start`, so it only ever recognises the first
  page of one — and `vm_flags` cannot be tested either, because
  `vm_area_create` never sets it and `sys_mmap` overwrites it with the
  `MAP_*` flags.
- `strnlen_user(str, maxlen)` measures a caller-supplied string without
  ever walking past a page the caller does not own: it proves each page
  before reading a byte of it, and answers `-EFAULT` or `-ENAMETOOLONG`
  instead of a length. `strlen` on a user pointer is exactly the unbounded
  walk this whole exercise exists to prevent — never reintroduce it.

Address arguments are a different class and must NOT go through either
range check: `mmap`, `munmap`, `brk`, `shmat` and `shmdt` name addresses
that nothing requires to be mapped yet, so they get a bounds check against
`PROCAREA_END_ADDR` instead of a page-table walk.

Gated as of stage two: `read`, `write`, `time`, `pipe`, `waitpid`,
`stat`, `fstat`, `statfs`, `fstatfs`, `uname`, `getcwd`, `readlink`,
`getdents`, `chdir`, `open`, `creat`, `unlink`, `mkdir`, `rmdir`,
`symlink`, `chmod`, `chown`, `lchown`, `syslog`, `sigaction`,
`sigprocmask`, `sigpending`, `nanosleep`, `getitimer`, `setitimer`,
`sched_setparam`, `sched_getparam`, `semop`, `semctl`, `msgsnd`,
`msgrcv`, `msgctl`, `mmap`, `munmap`, `brk`, `shmat`, `shmdt`.

Still not gated, deliberately:

- `execve` validates the filename and `argv[0]`, and nothing else. The
  rest of `argv` and `envp` — arrays of pointers whose every element is a
  string — are still walked raw. #196 bounded the counting, it did not
  validate the pointers.
- `ioctl` and `fcntl` take an opaque `unsigned long data` that is a
  pointer only for some requests. There is nothing to gate generically at
  the syscall boundary: only the driver knows whether a given request
  carries a pointer, how large the object is, and which way it is copied,
  so **the gate belongs in the `ioctl_f` / `fcntl_f` implementation**.
  This is not a formality — `procv_ioctl` dereferenced `data` raw in both
  directions for `TCGETS` and `TCSETS`, one call away from any process
  with a terminal, until #394. Audited at that point: `procfs_ioctl` and
  `ext2_ioctl` do not dereference `data`, and `pipe_fcntl` treats it as a
  bitmask, so `procv_ioctl` was the only offender.
- `shmctl` implements only `IPC_RMID` and never touches `buf`. The day a
  command reads or writes it, it needs
  `paging_is_user_range_writable(buf, sizeof(*buf))`.

Two traps when adding a gate. Some of these entry points are also called
from inside the kernel with kernel pointers, and gating them naively
panics: that is why `sys_chmod`/`do_chmod` and `sys_getcwd`/`do_getcwd`
are split, and why `sys_time(NULL)` stayed legitimate. And where a syscall
already had a `NULL` check returning something else, delete it rather than
stack the two — `paging_is_user_range(NULL, n)` already fails, and a
second error code for the same mistake is a bug waiting to be reported.

## Contracts a future syscall must NOT assume exist

- No `copy_from_user`/`copy_to_user` with access_ok-style checks.
- No ARG_MAX: argv/envp size is bounded only by kmalloc success (#196).
- No guarantee that user buffers are NUL-terminated where the kernel
  `strlen`s them.
- No per-fd vs per-file permission reconciliation: authorization uses the
  fd's stored `flags_mask` while operations act on the possibly-shared
  `file_struct` (ext2 per-inode cache) — see vfs-and-fd-lifetime.md.

## Buffer-size contracts (VERIFIED VIOLATIONS)

- `/proc/<pid>/{stat,cmdline}` read ignores `nbyte` (`strcpy` instead of
  bounded memcpy) — **#194**, empirically reproduced.
- `__procr_do_cmdline(buffer, bufsize, task)` ignores `bufsize`.

## Argument-count considerations (#196, CODE-PROVEN)

- `__push_args_on_stack`: `char *args_location[256]` overflowed by argv/envp
  with >256 entries (kernel-stack OOB write of heap pointers).
- `__count_args`: unbounded vector walk until NULL.
- `__count_args_bytes`: `strlen` on each raw user string.
- Fix direction: ARG_MAX-style caps + `-E2BIG`; sized allocation instead of
  the fixed array; `strnlen` with limit.

## Rules for adding/changing syscalls (derived from the above)

1. Never `strcpy`/`strlen` a user pointer; copy bounded (`strnlen` first or
   byte-counted copy) into a kernel buffer sized to a limit you define.
2. Never write more to a user buffer than the syscall's size argument.
3. Reject oversized inputs with a real errno (`-E2BIG`, `-ENAMETOOLONG`)
   rather than truncating silently — truncation without forced NUL
   termination creates downstream over-reads (the PR #190 review lesson).
4. Assume any pointer may be non-canonical, unmapped, or pointing into
   kernel memory until #191 is fixed.
