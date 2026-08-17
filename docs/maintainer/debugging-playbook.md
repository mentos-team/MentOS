# Debugging playbook

Workflows actually used during this investigation (all verified working on
Ubuntu 24.04 host, Aug 2026). SHAs: `BASE` 82f4314, `MAIN` 62c638a.

## Disposable clone strategy (keeps the real repo clean)

```
git clone --local . /tmp/opencode/mentos-review
cd /tmp/opencode/mentos-review
git remote set-url origin https://github.com/mentos-team/MentOS.git   # if SSH unavailable
git fetch origin <sha> && git branch prNNN <sha>                      # PR heads
```
- PR head SHAs: `gh pr view N --json commits` (also title/body/files).
- **`gh pr view N` bare fails** with a GraphQL "Projects (classic)
  deprecated" error — always pass `--json`/`--jq` or `--patch`.
- Determine a PR's true base: `git merge-base prHead main` + compare with
  `origin/main` from a FRESH fetch — local remote-tracking refs can be
  stale (bit us: local origin/main pointed at an ancient release while
  GitHub main had moved).

## Build (host gcc -m32)

```
cmake -S . -B /tmp/opencode/build-X -DCMAKE_BUILD_TYPE=Debug
make -C /tmp/opencode/build-X -j$(nproc)            # everything incl. tests
make -C /tmp/opencode/build-X kernel.bin            # kernel only
make -C /tmp/opencode/build-X cdrom_test.iso        # test ISO
```

## QEMU invocation that works for serial capture

```
timeout 200 qemu-system-i386 -vga std -m 1096M -nodefaults -serial stdio \
  -drive file=BUILD/rootfs.img,format=raw,if=ide,index=0,media=disk \
  -device isa-debug-exit -boot d -cdrom BUILD/cdrom_test.iso -no-reboot \
  > run.log 2>&1; echo "exit=$?"
```
- `-no-reboot` avoids reboot loops after panics.
- **Exit code 0 does NOT mean success** (panic shuts down cleanly — #193).
  Always check the log (below).
- rootfs.img is WRITTEN by runs; rebuild (`make filesystem`) before
  pristine-image inspection.

## Serial-log analysis

```
grep -c "PANIC" run.log                 # >0 → guest died
grep "Running test" run.log | tail -1   # completion counter (n/39)
grep -E "PG_FLT|ERR\(0\)" run.log       # fault decode ERR(user rw present) as (%d%d%d)
```
Kernel-vs-user anchor values seen in repro runs: kernel EIP ~0xc00xxxxx,
kernel ESP ~0xf75xxxxx, user stack ~0xbfffxxxx. Panic EIPs can be
symbolized against build kernel.bin with nm/objdump (not yet exercised
here — noted as available).

## In-guest repro tests (technique)

1. Add `userspace/tests/t_aaa_<name>.c` (names sorting FIRST run before
   t_gid at position 10 — crucial while #192 lives).
2. Register in `userspace/tests/CMakeLists.txt` (TEST_LIST) AND
   `userspace/bin/runtests.c` (`all_tests[]`).
3. Use `syslog()`, never `printf` (VGA-only) — see testing-and-ci.md.
4. Rebuild `cdrom_test.iso`, run QEMU as above.
5. These modifications live only in the disposable clone.

## ext2 image forensics (e2fsprogs)

```
debugfs -R "stat <70>" rootfs.img      # size + BLOCKS + TOTAL (counts IND block)
debugfs -R "blocks <70>" rootfs.img
debugfs -R "dump <70> /tmp/f" rootfs.img && cmp /tmp/f filesystem/bin/tests/t_gid
debugfs -R "ls -l /bin/tests" rootfs.img
dumpe2fs -h rootfs.img                  # block size / counts / inode size
```
Hole detection rule: `TOTAL` (with IND) vs `ceil(size/4096)` data blocks —
shortfall ⇒ sparse holes. NOTE: `debugfs dump` reads holes as zeros, so a
byte-identical dump does NOT disprove a hole — check the block list.

## Isolated mke2fs sparse experiment (proved #192's root cause)

```
mkdir -p ext && dd if=/dev/zero bs=4096 count=27 | tr '\0' '\253' > ext/f
printf '\x00\x00\x00\x00' >> ext/f                       # all-zero 4-byte tail
mke2fs -q -N 0 -d ext -b 4096 -t ext2 -F img 4M
debugfs -R "stat <12>" img | grep -E "Size|TOTAL|BLOCKS" # 27 data blocks, hole at 27
```

## Host-side C-semantics validation

- **Ring-buffer logic (PR #188)**: extract the 2D macro block from
  `lib/inc/ring_buffer.h` (`sed -n '296,427p'` at `BASE`) into a test
  header, compile a main.c exercising old-vs-new push/fetch — proved the
  bug and the fix, including full-history wrap-around.
- **strncpy non-termination (PR #190)**: small program with
  `char name[255]` inside a struct surrounded by poisoned bytes;
  `strnlen` shows 255/255 = unterminated; letting strlen walk into nonzero
  surroundings trips ASan (`-fsanitize=address`) — demonstrating unbounded
  consumer over-read.

## Comparing a PR against base main

1. Fetch base (`git fetch origin <baseSha>`) and PR head; branch both.
2. `git diff <base>..<prHead> -- <paths>` for the real diff (ignore
   GitHub's merge-base guesswork).
3. Extract base versions of files for line-accurate reference:
   `git show <base>:kernel/src/fs/vfs.c > /tmp/main_vfs.c`.
4. Build + qemu-test BOTH base and PR with SEPARATE build dirs; identical
   failure signatures (same test counter, same panic) ⇒ pre-existing, not
   PR-caused. This pattern (base/188/189/190 matrix) is how #192 was
   isolated.

## GitHub hygiene

- `gh issue list --state all --search "..."` (also `gh pr list`) for dup
  sweeps before filing; `gh issue view N --json ...` for bodies/comments.
- `gh issue create --repo ... --title ... --body-file file.md` keeps long
  bodies intact.
