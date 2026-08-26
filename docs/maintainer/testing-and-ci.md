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

## qemu-test behavior — FIXED (issue #193)

The guest now uses QEMU's `isa-debug-exit` device for explicit failure signaling:

- **Suite completion**: `runtests` writes 0x10 to port 0x501 → host exit 33
- **Panic**: `kernel_panic()` writes 0x11 to port 0x501 → host exit 35
- **Encoding**: host_exit = (guest_value << 1) | 1
- These values avoid collision with generic QEMU exit 1 (startup failure)

### Implementation components

1. **Guest signaling** (kernel/src/system/panic.c:18, userspace/bin/runtests.c:235):
   - Panic path: `outports(0x501, 0x11)` → host exit 35
   - Completion path: `outports(0x501, 0x10)` → host exit 33

2. **Host wrapper** (scripts/run-qemu-test):
   - Treats exit 33 as normal suite completion, then validates `test.log` with `tapview`
   - Returns success only when QEMU completed normally and TAP reports zero failures
   - Any other QEMU exit code (1, 3, 35, 124, timeout, etc.) is failure
   - **Streamer lifecycle** (issue #218): the two `tail -F | sed` live-stream
     pipelines run each in their own process group (`set -m` window around the
     launch; the subshell PID doubles as the PGID). The normal path kills both
     groups after QEMU exits (following a 1 s drain so the final lines are still
     streamed); `EXIT`/`INT`/`TERM`/`HUP` traps run the same cleanup, so no
     streamer survives any exit path and piped callers always receive EOF.
     `tail --pid` is deliberately NOT used (GNU-only; absent on BSD/macOS).

3. **CI integration** (.github/workflows/ubuntu.yml:91, 101):
   - Uses `scripts/run-qemu-test` with proper exit-code handling
   - `tapview` failure exit propagates (no longer swallowed)

4. **Local development** (CMakeLists.txt:313-317):
   - `make qemu-test` now uses `scripts/run-qemu-test` wrapper
   - Wrapper translates guest exit codes and validates TAP before returning success
   - Consistent with CI pass/fail semantics

### Exit code reference

| Scenario                          | Guest write | Host exit | Recognized as |
|-----------------------------------|-------------|-----------|---------------|
| All tests pass                   | 0x10        | 33        | Success after TAP validation |
| One or more tests fail           | 0x10        | 33        | Failure via `tapview`        |
| Kernel panic                     | 0x11        | 35        | Failure       |
| QEMU startup failure             | N/A         | 1         | Failure       |
| QEMU timeout                     | N/A         | 124       | Failure       |
| QEMU error/crash                 | N/A         | 0,2,4+    | Failure       |

## Reliable completion check

With the fix applied, failure detection is now automatic:

1. **Local testing with proper signaling**: `./scripts/run-qemu-test build 600`
   - Exit 0 = QEMU completed normally and all TAP tests passed
   - Non-zero = test failure, malformed/incomplete TAP, panic, timeout, or QEMU error

2. **Manual inspection (still useful for debugging)**:
   - `grep -c "PANIC"` serial output — >0 means guest died
   - `grep "Running test" | tail -1` — compare counter against total
   - `cat build/test.log | scripts/tapview` — TAP validation

3. **TAP validation**: the wrapper validates `test.log` after normal guest completion,
   so failed tests and missing/short plans fail both local `qemu-test` and CI.

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
  `scripts/run-qemu-test build 600`, then a diagnostic tapview step;
  artifacts test.log/serial.log uploaded `if: always()`.
- macos.yml exists (issue #124 tracks build problems there; not examined).
