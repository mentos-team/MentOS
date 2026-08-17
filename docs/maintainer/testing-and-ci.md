# Testing and CI

Verified against `MAIN` = `62c638a` (line refs for `MAIN` unless noted).

## Build/test architecture

- CMake-only (no top-level Makefile; use `build/` dir + generated make).
  Typical: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && make -C build -j`.
- Observed workspace used HOST gcc 13 with `-m32` (CMakeCache
  `CMAKE_C_COMPILER=/usr/bin/gcc`); `tools/toolchain-i686-elf.cmake` exists
  for an i686-elf cross toolchain but was not used in our builds.
- Key targets: `kernel.bin`, `bootloader.bin`, `filesystem` (rootfs.img via
  mke2fs), `cdrom.iso`, `cdrom_test.iso`, `qemu`, `qemu-test`, `programs`,
  `tests`, per-program targets (`prog_shell`, `test_t_list`, ...).
- Host deps used: qemu-system-i386, grub-mkrescue, xorriso, mke2fs,
  debugfs/dumpe2fs (e2fsprogs), nasm, gcc-multilib.
- **Tests build INTO the source tree**: `userspace/tests/CMakeLists.txt`
  installs to `${CMAKE_SOURCE_DIR}/filesystem/bin/tests`
  (`MENTOS_TESTS_DIR`). Building tests dirties the working tree with
  binaries.

## Test registration (two places, both required)

1. `userspace/tests/CMakeLists.txt` → `TEST_LIST`.
2. `userspace/bin/runtests.c` → `all_tests[]` (order = execution order).
   Intentionally skipped there: `t_big_write`, `t_periodic1/2/3`;
   `t_time` disabled in CMake TEST_LIST.

## Guest output routing (critical for debugging)

- `printf` goes to the VGA console — INVISIBLE when capturing serial only
  (`-nographic` + `-serial stdio`).
- `syslog()` output arrives on the serial console (runtests itself uses
  `syslog(LOG_INFO, "Running test (%2d/%2d): %s", ...)`). Write in-guest
  repro/tests with `openlog(..., LOG_CONS|LOG_PID, LOG_USER)` + `syslog`.

## qemu-test behavior — issue #193 (EMPIRICALLY REPRODUCED, 5 runs)

Four independent reasons a kernel panic looks like success:

1. `kernel_panic` (kernel/src/system/panic.c:11,20) writes `0x2000` to port
   `0x604` (QEMU ACPI shutdown) when `runtests` is set → **clean QEMU exit,
   host exit code 0**. `runtests` itself shuts down the same way on normal
   completion (runtests.c:231).
2. CMake `qemu-test` target (CMakeLists.txt:314-319) runs QEMU DIRECTLY —
   no timeout, no exit-code interpretation, does not use
   `scripts/run-qemu-test` at all.
3. `scripts/run-qemu-test` (line 58) accepts BOTH 0 and 1 as success:
   `if [ "${EXIT_CODE}" -eq 1 ] || [ "${EXIT_CODE}" -eq 0 ]`.
4. CI (`.github/workflows/ubuntu.yml:101`) swallows tapview's failure exit:
   `cat build/test.log | scripts/tapview || echo "tapview failed"`.
   (Test step itself: ubuntu.yml:91 `./scripts/run-qemu-test build 600`.)

`isa-debug-exit` semantics: guest exit value 0 → host exit 1; the script's
`-eq 1` acceptance was presumably meant for that but 0 (panic path) is also
accepted.

Result: every panicking run ends with `[100%] Built target qemu-test` and
`$? = 0`. This masked the #192 regression on main (panic at test 10/39 —
tests 11–39 never run, TAP plan incomplete, CI green).

## Reliable completion check (procedure — use this, not exit codes)

1. Run QEMU capturing serial (see debugging-playbook.md).
2. `grep -c "PANIC"` serial output — 1 means the guest died.
3. `grep "Running test" | tail -1` — compare the (n/39) counter against the
   full list; a stuck counter < total means incomplete run.
4. For TAP consumers: missing/short plan in `test.log` (tapview flags
   "Expected N tests but only M ran") — currently only visible if tapview's
   exit is allowed to propagate.

## Current known suite state (at `MAIN`)

- Tests 1–9 pass (t_abort, t_alarm, t_chdir, t_creat, t_dup, t_environ,
  t_exit, t_exec, t_fork).
- Test 10 (t_gid) panics the kernel (#192); 11–39 unreachable.
- The two investigation repros (`t_aaa_procfs`, `t_aab_pipe`, named to sort
  first) run at positions 1–2 when registered; they lived only in a
  disposable clone and are NOT in the repository — copy them from
  investigation-history.md / issues #194/#195 if needed.

## CI workflow shape (ubuntu.yml)

- build job: matrix of compilers; cmake Release build only.
- test job: Release + `EMULATOR_OUTPUT_TYPE=OUTPUT_LOG`; builds rootfs,
  debugfs sanity-dumps it, builds `cdrom_test.iso`, runs
  `scripts/run-qemu-test build 600`, then the (swallowed) tapview step;
  artifacts test.log/serial.log uploaded `if: always()`.
- macos.yml exists (issue #124 tracks build problems there; not examined).
