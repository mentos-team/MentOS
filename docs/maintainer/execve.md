# execve — the complete pipeline

Verified against `BASE` = `82f4314` (PR #190 context) and `MAIN` = `62c638a`.
Function: `sys_execve(pt_regs_t *f)` in kernel/src/process/process.c
(`MAIN` process.c:542). Related: PR #190 (open, changes reviewed here),
issue #196 (argv stack overflow, filed from this investigation).

## Trust boundary (VERIFIED FACT)

`f->ebx` = filename, `f->ecx` = argv, `f->edx` = envp — all raw user
pointers, used with zero validation. Only NULL checks on filename/argv/
argv[0]; envp NULL is substituted with a default environment. The libc
wrapper (`lib/src/unistd/exec.c:61-65`) passes vectors straight through.

## Pipeline in order

1. **Save name/filename** into stack buffers `char name_buffer[NAME_MAX]`
   (255) and `char saved_filename[PATH_MAX]` (4096):
   - pre-#190: `strcpy` → kernel stack overflow from user-controlled
     `argv[0]`/filename. PR #190's own PoC (in its description) demonstrates
     EIP control (`0xdeadbeef`); build confirmed `-fno-stack-protector`
     (tools/toolchain-i686-elf.cmake; USERSPACE_CFLAGS).
   - with #190: `strncpy` bounded to destination size — **but no forced
     NUL termination** (see below).
2. **Copy argv/envp to kernel memory**: `argc = __count_args(origin_argv)`,
   `argv_bytes/__count_args_bytes` (strlen per string), same for envp;
   `kmalloc(argv_bytes + envp_bytes)`; `__push_args_on_stack` walks the RAW
   user vectors copying strings and building pointer arrays.
3. **`__load_executable(filename, current, &eip)`** (process.c:134ff at
   `BASE`):
   - `vfs_open(path, O_RDONLY)`; ops presence check;
     `vfs_valid_exec_permission`; setuid/setgid handling;
     ELF-vs-shebang detection.
   - **`mm_destroy(task->mm)` happens HERE — before the new image is
     loaded** (process.c:175 at `MAIN` numbering).
   - `__reset_process` builds fresh mm; `elf_load_file` (elf.c):
     `vfs_fstat` → `kmalloc(st_size)` → `memset 0` →
     `vfs_read(file, buffer, 0, st_size)` — requires the read to return
     EXACTLY st_size, else error.
   - Shebang path: builds `int_argv` (original filename as argv[1]),
     re-copies args, `goto start` with interpreter-loop guard.
   - error label `close_and_return` → `vfs_close(file)` and returns < 0.
4. On success: `paging_switch_pgd(current->mm->pgd)`, push args/env onto new
   user stack (`__push_args_on_stack` on saved vectors), record
   arg_start/arg_end/env_start/env_end, push main's argv/envp, set
   eip/useresp.
5. `strncpy(current->name, name_buffer, TASK_NAME_MAX_LENGTH)` (post-#190)
   and `kfree(args_mem)`.

## Error paths and the proven crash chain

VERIFIED (empirically reproduced as part of #192 investigation):
if step 3 fails AFTER `mm_destroy` (e.g. `vfs_read` short read because of the
ext2 sparse-hole bug), `sys_execve` returns a negative errno to a process
whose address space no longer exists → user-stack fetch faults in kernel
mode → `kernel_panic("Page fault!")`. Signature observed repeatedly:
`EIP 0xc00026f9, cr2 0xbfffedf8, ESP 0xf75eec48` (kernel stack), panic from
`page_fault.c`/`panic.c` (kernel EIP not symbolized in this investigation —
symbolization via `nm`/`objdump` on `kernel.bin` is available but was not
performed).

Invariant violated: **exec must not destroy the old mm until the new image
is fully loaded, or must not return to userspace on failure.**

## Known weaknesses

1. **#196 (CODE-PROVEN, not yet executed in-guest)**:
   `__push_args_on_stack` (process.c:65-90 at `MAIN`) uses fixed
   `char *args_location[256]` (process.c:70) indexed by user argc → argv/envp
   with >256 entries overflows the kernel stack with kernel-heap pointers.
   `__count_args` (process.c:37-44) walks the user vector unbounded until a
   NULL; `__count_args_bytes` (process.c:49-57) `strlen`s raw user strings.
   No ARG_MAX exists.
2. **PR #190 termination gap (EMPIRICALLY DEMONSTRATED on host)**:
   `strncpy(dst, src, n)` leaves dst unterminated when `strlen(src) >= n`;
   sources are raw user strings. A ≥255-byte argv[0] leaves
   `current->name` fully populated with no NUL; consumers over-read:
   `__procr_do_cmdline` `strcpy(buffer, task->name)` (proc_running.c:60),
   `__procr_do_stat` `basename(task->name)` (proc_running.c:82), ~15
   `pr_debug("%s", ...->name)` sites. Host ASan repro confirmed unbounded
   read past the field when following bytes are nonzero; in practice reads
   usually stop at adjacent `error_no == 0` — luck, not guarantee.
   Repo idiom for correct bounded copy: `strncpy(dst, src, sizeof(dst)-1);
   dst[sizeof(dst)-1] = 0;` (vfs.c superblock registration).
3. `process.h` defines `TASK_NAME_MAX_LENGTH NAME_MAX` (post-#190) without
   including a header that defines `NAME_MAX` — compiles only via include
   order (fragile).
4. `saved_filename` is later used as `int_argv[1]` in the interpreter path
   and `strlen`'d by `__count_args_bytes` — an unterminated 4096-byte
   filename (post-#190, exactly-PATH_MAX source) would over-read (same
   strncpy issue).

## Termination guarantees that DO hold (VERIFIED)

- `resolve_path` (kernel/src/fs/namei.c) always outputs a NUL-terminated
  path: internal buffers are zero-initialized, appends guarded by
  `strlen(buffer) + tokenlen + 1 < buflen`, and the final copy is
  `strncpy(abspath, buffer, buflen)` from a terminated internal buffer.
  `task->cwd` (fed exclusively by resolve_path) is therefore always
  terminated, and the `strncpy(proc->cwd, source->cwd, PATH_MAX)` copies are
  safe as deployed.

## Relationship to #190 and #196

- PR #190 fixes the write-overflow (name/filename copies) but not
  termination and not the argv helpers → verdict REQUEST_CHANGES at review
  time; #196 was filed for the `args_location[256]` + unbounded-walk class.
- The #196 fix (in `sys_execve`): counts are bounded (`MAX_ARG_COUNT`
  entries, `MAX_ARG_STRLEN` bytes per string, `ARG_MAX` bytes per vector,
  all in `lib/inc/limits.h`, enforced with `-E2BIG`); the fixed
  `args_location[256]` is replaced by caller-owned arrays sized from the
  validated counts (`argv_locations`/`envp_locations`); user strings are
  copied through `strnlen`-bounded pushes (`__push_user_strings_on_stack`),
  kernel copies through trusted pushes; the interpreter rebuild re-checks
  its combined budget against `ARG_MAX`. The libc `execve()` wrapper passes
  vectors through unchanged, so userspace sees the same `E2BIG` the kernel
  returns.
