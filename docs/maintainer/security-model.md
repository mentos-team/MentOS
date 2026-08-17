# Security model

Verified against `BASE` = `82f4314` / `MAIN` = `62c638a`. This file separates
**demonstrated** exploitability from **theoretical** impact everywhere.

## Observed trust boundary

- Ring separation with a syscall gate (`int 0x80`-style, dispatch via
  `sys_call_table`, kernel/src/system/syscall.c). No user-pointer validation
  anywhere (VERIFIED) → umbrella issue **#191** (open).
- Permission checks exist at VFS/fs level: `vfs_valid_open_permissions`
  (root/pid-0 bypass; owner/group/other bits), `vfs_valid_exec_permission`,
  setuid/setgid on exec. **Per-fd** write authorization is only
  `flags_mask` stored in the fd slot (read_write.c:63).

## Recurring bug classes found in one review cycle

1. **Raw-user-pointer string handling** — `strcpy`/`strlen`/unbounded walks
   on syscall inputs (#196; PR #190's original overflows; #191's class).
2. **Kernel→user overflow** — fs read handlers copying whole content into
   user buffers (#194).
3. **Refcount/lifetime violations** — pre-#189 `vfs_destroy_task` double
   decrement (FIXED); `sys_pipe` multi-free (#195).
4. **"Validation" that only checks for NULL** — pervasive.

## Why refcount bugs here are privilege escalations (demonstrated chain)

ext2 shares one `vfs_file_t` per inode (`ext2_find_vfs_file_with_inode`).
Count corruption → premature free or stale struct → slab slot reused by a
later open of a DIFFERENT file → two fds alias one struct → `sys_write`
authorizes on the attacker's fd `flags_mask` but writes through the shared
struct. PR #189's description contains a full working PoC (as `user`,
writing `PWNED!` into a root-owned 0444 file); the mechanism was verified
line-by-line at `BASE` (the exploit itself was not re-run by us —
DEMONSTRATED BY PR AUTHOR, VERIFIED BY CODE ANALYSIS).

## Demonstrated vs theoretical (explicit ledger)

| Issue | Demonstrated | Theoretical/expected |
|---|---|---|
| PR #190 pre-fix overflow | Author's PoC: EIP → 0xdeadbeef in sys_execve | — |
| #192 ext2 sparse read | Kernel panic in CI-run tests; unreadable binary | DoS only; data exposure unlikely |
| #194 procfs overflow | Reader SIGSEGV; full stat line written past 8-byte buffer | Corrupt adjacent user objects (credentials/heap in reader's space); content kernel-controlled, length bounded by stat line |
| #195 pipe multi-free | 2–3 frees of pipe_info (log-proven), 2nd pipe_close reads freed memory | Slab freelist aliasing → cross-type corruption (allocator-mechanics INFERENCE, not observed) |
| #196 argv stack overflow | CODE-PROVEN (array bound vs user argc); not executed in-guest | Kernel stack control-flow hijack; written values are kernel heap pointers to user-controlled strings |
| #191 arbitrary ptr syscalls | Community-reported read/write of arbitrary kernel addresses via sys_read/sys_write | Full kernel compromise |

## Memory-safety-sensitive APIs/patterns to treat as radioactive

- `strcpy`/`sprintf`/`strcat` on anything user-derived or inter-buffer
  (see `__procr_read`, `__procr_do_stat`).
- `strncpy(dst, src, sizeof(dst))` without `dst[sizeof(dst)-1] = 0` —
  creates non-termination over-reads (PR #190 review finding; repo's correct
  idiom exists in vfs.c superblock registration).
- Fixed-size stack arrays filled by counts derived from user data
  (`args_location[256]`).
- Manual `--count` before `close_f` (violates the close_f contract —
  vfs-and-fd-lifetime.md).
- `kfree` of objects whose backing may already be freed (#195 pattern:
  error paths stacked on error paths).

## Non-goals / not assessed

- No ASLR/stack-canary expectations: kernel builds `-fno-stack-protector`
  by design (toolchain file) — assume no compiler hardening.
- IPC (msg/sem/shm), signals beyond SIGCHLD/SIGSEGV paths, keyboard/tty
  input handling: not audited.
- Whether `runtests`'s userspace `outports(0x604, ...)` implies permissive
  IOPL for all userspace (observed to WORK, mechanism not investigated —
  flagged as a question, not a finding).
