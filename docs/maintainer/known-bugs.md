# Known bugs — historical root-cause record

**Every issue in this file is closed.** Nothing here is a current defect, and
this file is not where the project's open bugs live — that is the GitHub
issue tracker, which is the only place that can tell you the truth about
status. What is kept here is the analysis: the proven root causes, the
reproductions, and the reasoning that led to each fix, which are worth more
than the status line ever was and do not go stale the same way.

That distinction is not academic. Two independent readers of an earlier
version of this file believed the `t_gid` panic under #192 was still
happening, because the entry said "open" and nothing prompted them to check
whether the baseline commit was their own HEAD. One of them changed where
they placed a new test to avoid a panic that does not occur (#220).

Baselines for the observations below: issues #192–#196 were analysed against
`MAIN` = `62c638a`, PR #190 against `BASE` = `82f4314`. Line numbers are valid
only for those commits; function names and call paths are the stable
references.

## Issues filed from this investigation — all closed

### #192 — ext2 sparse holes fail to read; qemu-test panic at t_gid
- **Status**: CLOSED. Fixed by `e37fe1d` (#267, read holes as zeros) and
  `9c868a8` (#356/#368, tell a hole apart from a block that could not be
  read). The panic at test 10 described below has not reproduced since.
  **Severity** was: high. **Subsystem**: kernel/src/fs/ext2/.
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
- **Status**: CLOSED. Fixed by `756ad8a` (#280) and the wrapper work that
  followed: `scripts/run-qemu-test` now revalidates `test.log` through
  `tapview` and maps QEMU's exit codes itself, so a panic cannot leave
  behind a green run. **Severity** was: high (developer/CI).
- **Proven root cause**: four stacked defects — panic.c clean QEMU shutdown
  (port 0x604) when `runtests`; CMake qemu-test target runs QEMU directly;
  run-qemu-test accepts exit {0,1}; CI swallows tapview exit
  (ubuntu.yml:101). All four verified in code; behavior EMPIRICALLY
  REPRODUCED (exit 0 on panic, 5/5 runs).
- **Related**: #192 (the masked regression), PR #46 (test-runner origin).
- **Outstanding hypotheses**: none; fix directions recorded in the issue.

### #194 — procfs read ignores nbyte (kernel→user overflow)
- **Status**: CLOSED. Fixed by `ec5df7d` (#282). **Severity** was:
  medium-high (security).
- **Proven root cause**: `__procr_read` `strcpy(buffer, support+offset)`
  after computing a correct `bytes_to_read` (proc_running.c:416-419 @`MAIN`).
- **Reproduction**: EMPIRICAL — `read(fd, char[8], 4)` on `/proc/<pid>/stat`
  returns 4, reader killed by SIGSEGV (user-mode fault).
- **Related**: #191 (umbrella), procfs.md (sibling modules unaudited for
  the same pattern).

### #195 — sys_pipe error path multi-frees pipe_inode_info
- **Status**: CLOSED. **Severity** was: medium-high (security).
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
- **Status**: FIXED (in-guest reproduced pre-fix and post-fix). **Severity**:
  high (security).
- **Proven root cause (CODE-PROVEN)**: `char *args_location[256]` indexed
  by user argc (`__push_args_on_stack`, process.c:70/75 @`MAIN`);
  `__count_args` unbounded walk; `__count_args_bytes` raw `strlen`s.
- **Reproduction**: in-guest via `t_execve_bigargv` — on the unfixed kernel
  the suite panics with an `Assertion failed` in `sys_execve` during the
  257-entry case (the stack smash corrupts the accounting before the
  assert trips); with the fix, 257/300/1024-entry vectors exec correctly
  and 1025 entries, a 8192-byte string, or a vector above `ARG_MAX` return
  `E2BIG` (`56/56` tests, Debug and Release, 0 panics).
- **Fix**: bounded counts (`MAX_ARG_COUNT`/`MAX_ARG_STRLEN`/`ARG_MAX`,
  `lib/inc/limits.h`), caller-owned position arrays sized from the
  validated counts, `strnlen`-bounded pushes of user strings, `ARG_MAX`
  re-check on the interpreter rebuild.
- **Related**: #190 (complementary, same function), #191, #121.


## The PR this investigation reviewed

### PR #190 — security(kernel): strncpy bounds in process.c
- **Status**: merged; issue #190 closed. The review verdict recorded at the
  time was REQUEST_CHANGES (88% confidence), and the blocking points below
  were addressed before it landed.
- Fixes the write overflows (bounds match destinations; NAME_MAX resize
  consistent — verified). Blocking: no forced NUL termination (host-ASan
  demo of consumer over-read); `process.h` missing `limits.h` include.
- Non-blocking: add long-name/long-path execve tests; note pre-existing
  qemu-test red (its "all tests pass" claim was an artifact of #193).

## Relevant older issues

- **#191** (closed) — umbrella: no user-pointer validation in syscalls
  (`sys_read`/`sys_write` arbitrary kernel access). #194/#196 were concrete
  instances. Closed across five PRs (#384, #387, #395, #402, #403), which
  also produced the `uaccess` layer in `kernel/inc/mem/uaccess.h`; see
  syscall-boundaries.md.
- **#121** (closed) — "crashes with some bad executables": was suspected to
  be the exec-after-mm_destroy panic path (#192 secondary) and/or
  #190-class overflows. The mapping was never confirmed and the symptom
  stopped reproducing.
- **#56** (closed 2024) — original sparse-tail occurrence (t_alarm);
  historical context for #192.
- **#124** (closed) — macOS M2 build failures.
