# Architecture

Verified against `BASE` = `82f4314` and `MAIN` = `62c638a` unless noted.

## Layout

```
boot/        multiboot boot code, linker scripts (boot.lds, kernel.lds), boot.S/boot.c
kernel/
  inc/       kernel headers mirrored per-subsystem (process/, fs/, mem/, ...)
  src/
    kernel.c / kernel.c? kmain                     — init flow (kmain in kernel/src/kernel.c)
    process/ process.c, scheduler.c, wait.c, pid_manager.c,
             scheduler_algorithm.c, scheduler_feedback.c
    fs/      vfs.c, ext2.c, procfs.c, pipe.c, open.c, namei.c, read_write.c,
             readdir.c, stat.c, attr.c, fcntl.c, ioctl.c, sync.c, fhs.c
    mem/     paging.c, page_fault.c, mm/mm.c, vm_area.c, vmem.c,
             alloc/{slab.c, buddy_system.c, zone_allocator.c, heap.c}
    elf/     elf.c (ELF loader)
    io/      video.c, proc_running.c, proc_system.c, proc_ipc.c,
             proc_video.c, proc_feedback.c, debug.c
    drivers/ ata.c, mem.c, ps2.c, keyboard/, mouse.c, rtc.c, fdc.c
    system/  syscall.c (dispatch table), signal.c, panic.c, printk.c, errno.c
    ipc/     ipc.c, msg.c, sem.c, shm.c            (not investigated in depth)
lib/         freestanding libc used by kernel AND userspace
             (inc/limits.h: NAME_MAX 255, PATH_MAX 4096; inc/stddef.h: BUFSIZ 512;
              inc/ring_buffer.h: generic ring-buffer macros)
userspace/
  bin/       shell.c, runtests.c, init.c, and the standard utilities
  tests/     t_*.c test programs (built INTO the source tree, see testing-and-ci.md)
filesystem/  the rootfs staging tree — binaries are copied here by the build;
             mke2fs turns it into rootfs.img
iso/         grub cfg for boot/test ISOs (grub.cfg, grub.cfg.runtests)
scripts/     run-qemu-test, tapview
tools/       toolchain-i686-elf.cmake (cross-compiler settings; the observed
             workspace builds used host gcc -m32 instead)
```

## Subsystems and ownership boundaries (VERIFIED FACTS from call-path tracing)

- **Process/scheduler** owns `task_struct` allocation (`task_struct_cache`
  slab), runqueue, zombie state, wait queues. `do_exit` marks zombie + moves
  children to init + `mm_destroy` — it does NOT close fds.
- **VFS** owns `fd_list` arrays and the `vfs_file_t` cache; dispatches to
  filesystem `fs_operations`/`sys_operations` vtables. Superblock registry
  (`vfs_super_blocks` list) maps path prefixes to mounted filesystems.
- **Filesystems** (ext2, procfs, pipe) own file content and the meaning of
  `close_f`; each maintains its own opened-file bookkeeping (ext2 keeps a
  per-inode cache list `fs->opened_files`; procfs keeps per-entry lists).
- **MM** owns page tables (`pgd`), `mm_struct` per process, slab/buddy
  allocators. `paging_switch_pgd` is used during exec and init creation.
- **Syscall layer** (`kernel/src/system/syscall.c`) populates
  `sys_call_table[]` and dispatches; handlers receive `pt_regs_t *` with raw
  user arguments in ebx/ecx/edx — no pointer validation anywhere
  (VERIFIED; issue #191).

## Initialization flow (traced during PR review)

`kmain` (kernel/src/kernel.c) → caches/zones → VFS init (`vfs_init` creates
`vfs_superblock_cache`, `vfs_file_cache`) → mount root (ext2 via
`ext2_mount`) → proc modules (`procv_module_init` creates `/proc/video`, ...)
→ `process_create_init`:
- `__alloc_task(NULL, NULL, "init")`
- stdio: `vfs_open("/proc/video", ...)` three times with an extra explicit
  `count++` each, installed into fd 0/1/2 (VERIFIED at `BASE`
  kernel/src/process/process.c:362-378).
- `__load_executable("/bin/init", ...)`; args/envp pushed onto the new stack;
  jump to init.

Init runs `/bin/init`, which execs `/bin/login` → shell → runtests (in test
builds, driven by `grub.cfg.runtests`).

## Major call paths established during this investigation

- `sys_open` → `get_unused_fd` → `vfs_open` → `resolve_path` →
  `vfs_open_abspath` → `sb->root->fs_operations->open_f` →
  `file->count += 1` → slot installed.
- `sys_close` → NULL slot → `vfs_close` → `close_f` (decrements count, frees
  at 0).
- `sys_execve` → save name/filename (raw `strcpy`, bounded by PR #190) →
  `__count_args`/`__count_args_bytes`/`__push_args_on_stack` on RAW user
  vectors → `__load_executable` → `vfs_open` → permission checks →
  `mm_destroy(old)` → `__reset_process` → `elf_load_file` (fstat → kmalloc →
  `vfs_read` full file) → segment mapping → shebang interpreter loop →
  `paging_switch_pgd` → push args → set `eip`/`useresp` → copy name.
- `do_exit` → zombie + SIGCHLD + wake parent → (later) `sys_waitpid` →
  `vfs_destroy_task(child)` (closes remaining fds via `close_f`) →
  `kmem_cache_free(child)`.
- `sys_read`/`sys_write` → per-fd `flags_mask` check (write only) →
  `vfs_read`/`vfs_write` → filesystem op copies to/from the RAW user buffer.
- Read path for `/proc/<pid>/stat`: `sys_read` → `vfs_read` →
  `procfs_read` → module dispatch → `__procr_read` (builds
  `support[BUFSIZ]`, then `strcpy`s the whole thing to the user buffer —
  issue #194).

## Surprising implementation details a future agent will misunderstand

- **The kernel and userspace share one libc tree** (`lib/`). Changing
  `lib/inc/limits.h` etc. affects both.
- **Tests build in-tree**: `userspace/tests/CMakeLists.txt` copies test
  binaries to `${CMAKE_SOURCE_DIR}/filesystem/bin/tests`, i.e. the source
  tree is dirtied by building tests. Do not commit those binaries (they are
  presumably gitignored, but be deliberate).
- **`vfs_file_t` structs are shared per-inode in ext2** — two `open()`s of
  the same file return the SAME struct with `count == 2` (cache in
  `ext2_open` via `ext2_find_vfs_file_with_inode`). This makes refcount
  bugs privilege-escalation bugs (see vfs-and-fd-lifetime.md, security-model.md).
- **`kfree()` is safe on slab objects** — it routes via page metadata
  (`page->container.slab_main_page`). See memory-management.md; do not
  "fix" call sites that kfree cache objects.
- **`qemu-test` target does not use `scripts/run-qemu-test`** and a kernel
  panic shuts QEMU down cleanly (exit 0). See testing-and-ci.md.
- **`mke2fs` makes sparse holes for all-zero blocks** in the build image —
  the root cause of the long-standing t_gid/t_alarm load failures
  (ext2.md, issue #192).
- Local `develop` and GitHub `main` have diverged historically; when
  reviewing PRs, always compute the PR's true base (merge-base with its
  head), never trust the local checkout (see investigation-history.md).
