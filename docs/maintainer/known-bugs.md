# Known bugs — status ledger

Baselines: issues #192–#196 verified against `MAIN` = `62c638a`. PR #190
reviewed against `BASE` = `82f4314`.

## Open issues filed from this investigation

### #192 — ext2 sparse holes fail to read; qemu-test panic at t_gid
- **Status**: open. **Severity**: high. **Subsystem**: kernel/src/fs/ext2.c.
- **Proven root cause**: `mke2fs` writes all-zero tail blocks as holes;
  `ext2_read_inode_data` includes the hole block;
  `ext2_read_block` rejects block 0 → whole read fails. Secondary: failed
  exec after `mm_destroy` → kernel panic (see execve.md).
- **Reproduction**: deterministic; `make qemu-test` panics at test 10/39
  (EMPIRICALLY REPRODUCED on 5 builds). Isolated mke2fs experiment proves
  the image side.
- **Related**: #56 (2024 same bug, t_alarm; closed unresolved-root-cause),
  PR #59 (partial fix), #121 (likely same panic class), #193 (masks it),
  #192↔#194/#195 repro placement (t_gid blocks 30 tests).
- **Outstanding hypotheses**: write-side mirror (writing into a hole needs
  block alloc; `ext2_write_block` also rejects 0) — untested. Panic EIP
  0xc00026f9 not symbolized.

### #193 — qemu-test/CI green on kernel panic
- **Status**: open. **Severity**: high (developer/CI).
- **Proven root cause**: four stacked defects — panic.c clean QEMU shutdown
  (port 0x604) when `runtests`; CMake qemu-test target runs QEMU directly;
  run-qemu-test accepts exit {0,1}; CI swallows tapview exit
  (ubuntu.yml:101). All four verified in code; behavior EMPIRICALLY
  REPRODUCED (exit 0 on panic, 5/5 runs).
- **Related**: #192 (the masked regression), PR #46 (test-runner origin).
- **Outstanding hypotheses**: none; fix directions recorded in the issue.

### #194 — procfs read ignores nbyte (kernel→user overflow)
- **Status**: open. **Severity**: medium-high (security).
- **Proven root cause**: `__procr_read` `strcpy(buffer, support+offset)`
  after computing a correct `bytes_to_read` (proc_running.c:416-419 @`MAIN`).
- **Reproduction**: EMPIRICAL — `read(fd, char[8], 4)` on `/proc/<pid>/stat`
  returns 4, reader killed by SIGSEGV (user-mode fault).
- **Related**: #191 (umbrella), procfs.md (sibling modules unaudited for
  the same pattern).

### #195 — sys_pipe error path multi-frees pipe_inode_info
- **Status**: open. **Severity**: medium-high (security).
- **Proven root cause**: readers/writers set only after both fds succeed;
  `pipe_close` during creation frees pipe_info; sys_pipe error path frees
  again (+sys_close partial path). Up to 3 frees (pipes.md has the exact
  chain).
- **Reproduction**: EMPIRICAL — fd exhaustion + pipe(); serial signature
  captured (two "already zero" warnings = double pipe_close on freed
  pipe_info). Freelist-aliasing impact is INFERENCE, not observed.
- **Related**: allocator mechanics in memory-management.md.

### #196 — sys_execve argv >256 entries overflows kernel stack; unbounded
user-string walks
- **Status**: open. **Severity**: high (security).
- **Proven root cause (CODE-PROVEN)**: `char *args_location[256]` indexed
  by user argc (`__push_args_on_stack`, process.c:70/75 @`MAIN`);
  `__count_args` unbounded walk; `__count_args_bytes` raw `strlen`s.
- **Reproduction**: NOT yet executed in-guest (test sketch in the issue);
  code path is unambiguous. Host libc passes vectors through unvalidated.
- **Related**: #190 (complementary, same function), #191, #121.

## Open PR

### PR #190 — security(kernel): strncpy bounds in process.c
- **Status**: open; review verdict REQUEST_CHANGES (88% confidence).
- Fixes the write overflows (bounds match destinations; NAME_MAX resize
  consistent — verified). Blocking: no forced NUL termination (host-ASan
  demo of consumer over-read); `process.h` missing `limits.h` include.
- Non-blocking: add long-name/long-path execve tests; note pre-existing
  qemu-test red (its "all tests pass" claim was an artifact of #193).

## Relevant older issues

- **#191** (open) — umbrella: no user-pointer validation in syscalls
  (`sys_read`/`sys_write` arbitrary kernel access). #194/#196 are concrete
  instances; keep cross-referenced, don't close on partial fixes.
- **#121** (open) — "crashes with some bad executables": likely the
  exec-after-mm_destroy panic path (#192 secondary) and/or #190-class
  overflows. Unconfirmed mapping.
- **#56** (closed 2024) — original sparse-tail occurrence (t_alarm);
  historical context for #192.
- **#124** (open) — macOS M2 build failures; not investigated.
