# Investigation history (chronological)

Session of Aug 17, 2026 (host: Ubuntu 24.04, gcc 13.3, qemu-system-i386).
Reviewer: coding agent acting as senior maintainer/security reviewer.

## Phase 0 — orientation

- Local checkout was on `develop` (`9311863`), clean. Discovered later that
  local remote-tracking refs were stale — GitHub `main` had moved to
  `62c638a` (post-#188/#189 merges). All conclusions re-verified against
  explicit SHAs thereafter. Lesson recorded in debugging-playbook.md.

## Phase 1 — review of PRs #188/#189/#190 (base `82f4314`)

- **#188 (shell history duplicate handling)**: bug EMPIRICALLY proven with
  a host unit test linking main's actual `lib/inc/ring_buffer.h` macro
  (old push shows `ls` where `id` expected; fixed push correct); wrap-around
  (>10 entries) re-verified. Verdict MERGE (93%). Merged as `276496a`.
- **#189 (vfs_destroy_task double decrement)**: root cause verified by
  enumerating every `close_f` (all decrement internally) and every fd_list
  producer (all increment once). Exploit mechanism (ext2 per-inode struct
  cache + per-fd-only write authorization) verified line-by-line; author's
  PoC not re-run. Verdict MERGE (85%). Merged as `62c638a`.
- **#190 (strncpy bounds in process.c)**: write overflows real (author PoC:
  EIP control; `-fno-stack-protector` confirmed). Found the fix
  incomplete: `strncpy` without forced termination → consumer over-reads
  (host ASan demo); `process.h` include-order fragility for NAME_MAX.
  Verdict REQUEST_CHANGES (88%). Still open.
- Incidental discovery: `make qemu-test` panics at test 10/39 (t_gid) on
  BASE and on ALL three PR heads with identical signatures, and exits 0 →
  pre-existing, and the harness masks it. Tests 1–9 pass.

## Phase 2 — pre-existing-bug investigation (base `MAIN` = `62c638a`)

Re-verified every candidate from scratch on `MAIN`; 12 candidates → 5
issues + 7 dismissed.

Investigations performed:
- **t_gid panic**: serial capture; pristine vs post-run rootfs.img compared
  with debugfs (both showed 27 data blocks for a 28-block size → not guest
  corruption); byte-identical `debugfs dump` initially masked the hole
  (holes dump as zeros) — resolved by reading the BLOCKS list; isolated
  mke2fs experiment proved all-zero tails become holes; read the kernel
  read path (`ext2_read_block` rejecting 0; loop bound in
  `ext2_read_inode_data`); traced crash propagation through
  `elf_load_file` exact-size read and `mm_destroy`-before-load in
  `__load_executable`. Filed **#192**.
- **Harness green-on-panic**: read panic.c (port 0x604), CMake qemu-test
  target, scripts/run-qemu-test exit logic, ubuntu.yml tapview step;
  reproduced exit-0 five times. Filed **#193**.
- **procfs read overflow**: wrote in-guest repro `t_aaa_procfs`
  (syslog-based); observed return 4 + reader SIGSEGV (user-mode fault
  ERR(100)); first attempt with printf produced no serial output (VGA-only)
  — documented. Filed **#194**.
- **sys_pipe multi-free**: wrote in-guest repro `t_aab_pipe` (fd exhaustion
  via repeated `open("/")` — 12 opens then EMFILE; then `pipe()`); captured
  the exact double-close log chain; read create_pipe_fd/pipe_close/sys_pipe
  to establish up-to-3 frees. Filed **#195**.
- **execve argv**: static proof of `args_location[256]` overflow and
  unbounded walks; verified libc passes vectors through; verified #190
  does not touch these helpers. Filed **#196** (in-guest execution of the
  300-argv PoC deliberately deferred — code path unambiguous).
- **kfree-on-slab concern**: read slab.c `pr_kfree` — routes via page
  metadata; FALSE POSITIVE, dismissed.
- **waitpid pid==1 filter, fd_list init/dead extension, shell printf
  format bug, self-referential sprintf, sys_close errno**: investigated,
  classified real-but-minor or latent-unreachable; dismissed from filing
  (rationales in false-positives-and-dismissed-findings.md).

Dup sweeps: `gh issue/pr list --state all --search ...` per finding; #56
identified as the 2024 precursor of #192 (closed with an invitation to
reopen on recurrence); #191/#121 identified as umbrellas to cross-reference;
no duplicates for #193–#196.

## Phase 3 — issue filing

Filed one at a time with final dup sweeps: #192, #193, #194, #195, #196
(titles/bodies preserved in the issues themselves; bodies drafted in
`/tmp/opencode/issue-bodies/`, transient).

## Phase 4 — this knowledge base

Written on the `develop` working tree (untracked files only; no commits,
branches, or pushes; no source modified). Baselines stated per file.

## What was NOT pursued, and why

- **In-guest run of #196's 300-argv PoC** — code-proven; deferred to keep
  the investigation read-only and because the crash location (kernel stack
  smash) could wedge the disposable VM unpredictably.
- **Write-side sparse-hole test** — requires guest-side file writes into
  holes; left as a recorded hypothesis in #192/ext2.md.
- **Symbolizing panic EIP 0xc00026f9** — nm/objdump on kernel.bin noted as
  available; the code-level chain was already fully established.
- **IPC, signals detail, keyboard/tty, scheduler algorithms** — out of
  scope of the findings; no observations recorded beyond architecture.md.
- **Why exactly 12 fd opens (not 13) exhausted the table** — expected
  16−3 stdio = 13; one slot unaccounted (possibly an open runtests/cwd fd);
  not chased; does not affect #195's chain (EMFILE is what matters).
- **The 2-D ring buffer `rb_history_get` returning 1/0 vs success** —
  reviewed only as far as #188 required.
