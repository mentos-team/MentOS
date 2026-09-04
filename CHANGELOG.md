# Changelog

All notable changes to MentOS are recorded in this file.

The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **On the accuracy of what follows.** This file was introduced at v0.9.7, so
> only that release was written as it happened. Earlier entries are
> reconstructed, and not all to the same standard:
>
> - **v0.9.7** comes from the release's own record — 20 issues in its
>   milestone, each with a reproduction and a regression test.
> - **v0.9.6** is condensed from its release notes, which were written
>   deliberately and are faithful.
> - **v0.9.1 to v0.9.5** are derived from the auto-generated release notes,
>   which were lists of pull-request titles. They do not distinguish a fix from
>   a refactor and give no consequence for any change, so those entries are
>   grouped rather than itemised, and are the coarsest in this file.
>
> Every version links to its GitHub release, where the original text remains.

## [Unreleased]

## [0.9.7] - 2026-09-04

Correctness work on the ext2 driver, the ATA block layer underneath it, and the
integer types both rest on. The theme is failures that were reported as
success: eleven of the twenty issues below are a function that could not tell
its caller something had gone wrong.

### Security

- A truncating open wrote kernel heap bytes into the file being truncated, where
  any program that could read the file could read them back. The write length
  was computed as an offset, so every iteration after the first asked the
  driver for more bytes than the one-block buffer held ([#315]).
- On-disk directory-entry names of exactly 255 characters were copied into
  255-byte fields without room for a terminator, writing one byte out of bounds
  of the array and leaving the string unterminated ([#285]).
- `/tmp` and `/var/tmp` were created world-writable **without** their sticky
  bit, because the creation mask dropped exactly the bit the FHS table asked
  for, so any user could delete another user's files there. `/root` stayed at
  0755 instead of 0700 ([#263]).

### Fixed

- `rmdir` removed a directory that still held dot-prefixed files, leaving them
  with no path and inodes nothing would ever free. The check for `.` and `..`
  compared one character, so `.bashrc` and anything else dot-prefixed was
  invisible to it ([#341]).
- A failed ATA sector read returned the *previous* sector's contents and
  reported success, so ext2 could read a stale block as an inode table. The
  transfer functions were `void`. Transient status races are now retried and a
  persistent failure propagates ([#291]).
- `write()` reported success when it could not allocate a block, so a full
  filesystem discarded data silently ([#303]).
- A directory needing a second block replaced its first one, losing every entry
  it held ([#309]).
- The directory-entry iterator looped forever on a zeroed directory block, so
  one `rmdir` on a corrupt directory hung the kernel ([#304]).
- Unlinking a large file leaked its indirect index blocks ([#302]).
- `O_TRUNC` zeroed a file's contents but never set its size to zero, so `stat`
  reported the old length, reading returned that many zero bytes instead of
  end-of-file, and the blocks were never released ([#320]).
- A write ending exactly on a block boundary allocated and wrote one block more
  than it needed, permanently attributed to the file ([#311]).
- `ext2_creat` returned NULL without touching `errno` and left the inode it had
  allocated marked in use; `vfs_creat` then overwrote whatever the filesystem
  had reported with `ENOENT`, so a full disk, a read-only device and a request
  to create the root directory were all reported as "no such file or
  directory" ([#305], [#289]). It also leaked the parent reference on every
  success ([#323]).
- A mount point claimed the paths just outside it: the longest-prefix match
  compared only the prefix, so `/dev/nullx` opened as the null device and
  `/procx` was handed to procfs ([#289]).
- `int64_t` and `uint64_t` were 32 bits wide. Casts made specifically to gain
  range gained nothing and could not be warned about, which left the ELF bounds
  checks unable to detect the overflow they were written for, truncated ext2
  offsets past 4 GiB, and made the ATA IDENTIFY structure 508 bytes instead of
  512 — so the driver read 254 of the drive's 256 words ([#270]).
- `getcwd` reported failure with `(char *)-1` instead of NULL, which made every
  `== NULL` check already written in the tree dead code ([#231]).
- A process whose `SIGSEGV` handler returned got one kernel error line per
  delivery — 341722 of 341824 lines in one minute — starving every other
  process rather than only itself ([#296]).

### Added

- `INT64_MIN`, `INT64_MAX` and `UINT64_MAX`, which the header did not define,
  and compile-time width checks on every type in `stdint.h` so a future change
  to a typedef fails the build ([#270]).
- A bounded retry on ATA sector transfers, with the recovery logged rather than
  passed over in silence, and `warn_unused_result` on both transfer functions
  so a dropped error is a build failure ([#291]).
- Regression tests for the integer widths, the 255-character name boundary,
  mount-point boundaries, and `rmdir` on dot-prefixed files. The suite went
  from 59 registered tests to 68.

### Changed

- The ext2 driver is now one translation unit per concern — ten units and a
  private header, each 220 to 690 lines, replacing a single 4412-line file.
  Every function body was verified byte-identical to the one it replaced
  ([#317]).
- Every symbol the ext2 driver exports carries an `ext2_` prefix. Inside the
  module a leading `__` now means static, without exception ([#319]).
- `scripts/run-qemu-test` refuses to boot an image older than what it should
  contain, naming the offending file. Running the wrapper by hand had produced
  a silently passing verification three times ([#289]).
- `t_fhs` treats a directory-mode mismatch as a failure. It counted one as a
  pass "since the directory exists", which is how three wrong modes shipped
  under a green suite ([#263]).
- The CI badges are grouped by what they answer, and each workflow's trigger is
  documented ([#316]).

### Removed

- `CREAT_LAST_COMPONENT`, a path-resolution flag that was set in two places and
  read in none. Resolution never required the last component to exist, which is
  why `O_CREAT` worked, so the flag had nothing to ask for ([#289]).
- `ext2_clean_inode_content`, whose only remaining caller passed an inode of
  size zero, making its loop body unreachable ([#327]).

### Known issues

Carried forward, each with a reproduction in its issue:

- [#191] syscalls do not validate user pointers, with [#259] as a concrete
  instance in `sys_uname`. This is a deliberate state of a teaching kernel
  rather than an oversight, but it is not a property to assume away.
- [#344] several ext2 read-modify-write sequences discard the read result, so a
  failed read writes a zeroed metadata block back. Only reachable through the
  transient that [#291] now retries, and filed with [#342] and [#343] once that
  fix made such errors visible at all.
- [#336] intermittent single-test failures whose likely cause was [#291]. The
  mechanism fits and the suite has been green since, which is an explanation
  rather than a proof.
- [#338] there is no way to make a sector read fail on demand, so the storage
  error paths — including several fixed in this release — have no test that
  reaches them.

## [0.9.6] - 2026-09-03

98 commits and 47 issues. Almost entirely correctness work on the kernel, the
filesystem and the test harness. Condensed here; the
[release notes](https://github.com/mentos-team/MentOS/releases/tag/v0.9.6) carry
the full text.

### Fixed

- User-mode page faults on kernel-mapped addresses deliver `SIGSEGV` to the
  faulting process instead of panicking the kernel, so a null dereference from
  an unprivileged program no longer takes the system down (#237). An
  unregistered in-range syscall number can no longer dispatch through a NULL
  pointer (#206).
- `execve` builds the new image transactionally, so a failed load leaves the
  caller intact (#208); the shebang path lost an out-of-bounds write, a
  use-after-free and two leaks (#209, #227). Oversized `argv`/`envp` vectors
  are rejected instead of overflowing the kernel stack (#196).
- Fatal signals encode the terminating signal in the wait status, so
  `WIFSIGNALED` works (#234). `kill` reaches tasks that are not currently
  scheduled (#143) and `waitpid` waits passively (#57).
- Sparse holes read as zeros instead of failing the whole read (#192), and the
  intermittent boot-time mount failure is gone (#245).
- Path resolution no longer truncates. A request for a long path could resolve
  to a shorter existing name, which for `execve` meant running a file the
  caller never asked for (#284).
- ELF loading rejects files shorter than the header (#241, #224).
- `getcwd` handles undersized buffers with `ERANGE` (#230).

### Changed

- CI used to report success on a kernel panic (#193). It does not any more,
  which is how much of the rest of this release became findable.
- The macOS build works (#124, #268), and the harness no longer hangs on a
  piped `make qemu-test` (#218) or exits 0 when interrupted (#246).

## [0.9.5] - 2026-02-09

### Changed

- Unit-test framework overhaul, 73 tests across all subsystems, and a DMA zone
  allocator with memory-subsystem tests. FHS initialization enabled.

### Fixed

- Invalid syscall numbers return a negative `ENOSYS`. The Release build boots
  again, after correcting the port I/O inline assembly constraints.

## [0.9.4] - 2026-01-28

### Added

- Filesystem Hierarchy Standard directory initialization, `fflush`, and
  mountpoint entries injected into directory listings.

### Changed

- Project layout restructured.

### Fixed

- Shell history navigation, buffer handling, keyboard input and PS/2 scan-code
  translation. A set of ext2 fixes described only as "critical" at the time.

## [0.9.3] - 2025-12-15

### Added

- The kernel testing framework, built on an X-macro pattern, and `tapview`
  grouping of failing tests in its final report.

### Fixed

- CFS `vruntime` updates and scheduler feedback logging; zombie reaping for
  shell background processes and `waitpid` validation; descriptor-table
  robustness; VGA graphics stability; page-table allocation ordering in
  `__mem_pg_entry_alloc`. First macOS toolchain support.

## [0.9.2] - 2025-12-15

### Changed

- Build and documentation only: `CMAKE_LINKER` no longer forced, ARM build
  instructions, and the `master` to `main` rename recorded in the README.

## [0.9.1] - 2025-05-03

### Changed

- Memory-management code split into its own files — pages, process memory,
  virtual-memory mapping and the page directory each moved out of the monolith.

### Fixed

- `scanf` and `fgets` behaviour on stdin; `vsnprintf`; a struct-versus-typedef
  naming mismatch; `list_head` gained a validity check.

[Unreleased]: https://github.com/mentos-team/MentOS/compare/v0.9.7...develop
[0.9.7]: https://github.com/mentos-team/MentOS/compare/v0.9.6...v0.9.7
[0.9.6]: https://github.com/mentos-team/MentOS/compare/v0.9.5...v0.9.6
[0.9.5]: https://github.com/mentos-team/MentOS/compare/v0.9.4...v0.9.5
[0.9.4]: https://github.com/mentos-team/MentOS/compare/v0.9.3...v0.9.4
[0.9.3]: https://github.com/mentos-team/MentOS/compare/v0.9.2...v0.9.3
[0.9.2]: https://github.com/mentos-team/MentOS/compare/v0.9.1...v0.9.2
[0.9.1]: https://github.com/mentos-team/MentOS/compare/v0.9.0...v0.9.1

[#191]: https://github.com/mentos-team/MentOS/issues/191
[#231]: https://github.com/mentos-team/MentOS/issues/231
[#259]: https://github.com/mentos-team/MentOS/issues/259
[#263]: https://github.com/mentos-team/MentOS/issues/263
[#270]: https://github.com/mentos-team/MentOS/issues/270
[#285]: https://github.com/mentos-team/MentOS/issues/285
[#289]: https://github.com/mentos-team/MentOS/issues/289
[#291]: https://github.com/mentos-team/MentOS/issues/291
[#296]: https://github.com/mentos-team/MentOS/issues/296
[#302]: https://github.com/mentos-team/MentOS/issues/302
[#303]: https://github.com/mentos-team/MentOS/issues/303
[#304]: https://github.com/mentos-team/MentOS/issues/304
[#305]: https://github.com/mentos-team/MentOS/issues/305
[#309]: https://github.com/mentos-team/MentOS/issues/309
[#311]: https://github.com/mentos-team/MentOS/issues/311
[#315]: https://github.com/mentos-team/MentOS/issues/315
[#316]: https://github.com/mentos-team/MentOS/pull/316
[#317]: https://github.com/mentos-team/MentOS/issues/317
[#319]: https://github.com/mentos-team/MentOS/issues/319
[#320]: https://github.com/mentos-team/MentOS/issues/320
[#323]: https://github.com/mentos-team/MentOS/issues/323
[#327]: https://github.com/mentos-team/MentOS/issues/327
[#336]: https://github.com/mentos-team/MentOS/issues/336
[#338]: https://github.com/mentos-team/MentOS/issues/338
[#341]: https://github.com/mentos-team/MentOS/issues/341
[#342]: https://github.com/mentos-team/MentOS/issues/342
[#343]: https://github.com/mentos-team/MentOS/issues/343
[#344]: https://github.com/mentos-team/MentOS/issues/344
