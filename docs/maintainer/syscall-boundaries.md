# Syscall boundaries

Verified against `BASE` = `82f4314` and `MAIN` = `62c638a`.

## The one-sentence reality (VERIFIED FACT)

There is **no user-pointer validation layer**: syscall handlers take raw
pointers from `pt_regs` (ebx/ecx/edx) and dereference them in kernel space.
This is the subject of open umbrella issue **#191** ("security: Lack of
userspace pointer validation in syscalls"), which documents `sys_read`/
`sys_write` arbitrary kernel memory read/write via malicious pointers.

## Observed patterns per syscall (not exhaustive — audited where the
## investigation touched)

| Syscall | User inputs | Handling |
|---|---|---|
| `execve` | filename, argv, envp | NULL checks only; `strcpy`/`strncpy` + unbounded walks (`__count_args`, `__count_args_bytes`, `__push_args_on_stack`) — see execve.md, #196 |
| `read` | buf | **Gated since #191 stage one**: `paging_is_user_range(buf, nbytes)` → `-EFAULT`; then passed to fs `read_f` which copies INTO user memory; procfs overflows it (#194) |
| `write` | buf | **Gated since #191 stage one**: `paging_is_user_range` → `-EFAULT`; then passed to fs `write_f`; authorized by per-fd `flags_mask` ONLY (read_write.c:63) |
| `time` | time_t * | **Gated since #191 stage one**: NULL stays legitimate (POSIX, and the kernel's own callers pass it); any other pointer must be user memory |
| `pipe` | fds[2] | **Gated since #191 stage one**: the two descriptors are only stored through a validated pointer |
| `waitpid` | status | `*status = child->exit_code` direct store (not deeply audited) |
| open/close/chdir/etc. | path strings | `resolve_path` copies into kernel `PATH_MAX` buffers (bounded), but the SOURCE `path` is walked unbounded by tokenizers/strlen inside resolve and fs layers (INFERENCE: same class, not separately reproduced). Since #284 the walk no longer truncates: a component longer than `NAME_MAX - 1` characters, or a token that does not fit the caller buffer, fails with `-ENAMETOOLONG` instead of resolving to a shorter name |

## The user-pointer validation primitive (#191 stage one)

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

Gated so far: `read`, `write`, `time`, `pipe` — the four the issue names
first. The umbrella stays open until the remaining pointer-taking
syscalls (stat family, uname #259, sigaction, ipc, readlink buffers,
waitpid status, ...) are migrated the same way.

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
