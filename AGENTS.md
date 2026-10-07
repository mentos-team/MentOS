# AGENTS.md — operational index

This file is the repository's single operational guide. Deep subsystem
knowledge lives under `docs/maintainer/` (see the map below).

## Coding style

MentOS is freestanding C suitable for kernel development:

- Use precise types, initialize variables at declaration, prefer designated
  initializers, and preserve const-correctness.
- Manage ownership explicitly with the appropriate kernel allocator; check
  allocation and syscall return values and propagate errors with return codes
  or `errno`.
- Keep functions focused, use descriptive `snake_case` names for functions
  and variables, and avoid unnecessary macros or unsafe casts.
- Document public interfaces and non-obvious invariants with Doxygen comments.
  Keep formatting consistent with the surrounding code and run the relevant
  formatter or build checks when available.

The kernel has only a freestanding subset of the standard library; use the
project's kernel/libc helpers rather than assuming hosted-library behavior.

## Git and release workflow

- Use Conventional Commits: `<type>(scope): short summary` (for example,
  `fix(procfs): preserve stat field positions`).
- Keep commits focused and buildable. Fix/feature PRs target `develop` and are
  squash-merged; release PRs into `main` and the back-merge into `develop` use
  merge commits to preserve ancestry.
- Feature and fix branches start from `develop`. Before a release, update the
  changelog and version, run the relevant build/test suites, merge `develop`
  into `main`, tag `v<version>`, then merge `main` back into `develop`.
- Ask before pushing release tags or branches when the operation has not been
  explicitly requested.

## Repository map

| Path | Contents |
|---|---|
| `kernel/src/fs/` | VFS core (vfs.c), ext2, procfs, pipe, open/close/read/write |
| `kernel/src/process/` | process.c (alloc/fork/execve), scheduler.c (waitpid/exit), wait.c |
| `kernel/src/mem/` | paging, page_fault, mm, slab/buddy/zone allocators |
| `kernel/src/elf/` | ELF loader |
| `kernel/src/io/` | console (video.c + `video/` backends) + `/proc` module generators |
| `kernel/src/system/` | syscall dispatch, signals, panic, printk |
| `lib/` | shared freestanding libc (kernel AND userspace) |
| `userspace/bin/` | shell, init, runtests.c (test driver), utilities |
| `userspace/tests/` | t_*.c; built INTO `filesystem/bin/tests` (build dirties the tree) |
| `filesystem/` | rootfs staging tree → rootfs.img via mke2fs |
| `scripts/` | run-qemu-test, tapview |
| `.github/workflows/` | ubuntu.yml (build + test), macos.yml |

## Essential commands

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
make -C build -j$(nproc)          # everything (tests land in filesystem/bin/tests)
make -C build kernel.bin          # kernel only
make -C build qemu-test           # guest test run (WARNING: see below)
make -C build filesystem          # rebuild pristine rootfs.img (QEMU writes to it)
```

The kernel unit tests need their own build and their own boot mode. They run at
the end of `kmain` and then signal QEMU, instead of reaching userspace, so an
unattended run ends on its own rather than sitting in a shell.

```
cmake -S . -B build-kt -DCMAKE_BUILD_TYPE=Debug -DENABLE_KERNEL_TESTS=ON
make -C build-kt qemu-kernel-test # kernel suites only, exits on its own
```

## Merge policy

- **Fix/feature PRs into `develop`: squash and merge.** One commit per PR keeps
  `git log` readable and leaves `git bisect` only buildable states to land on;
  a branch's review ping-pong is not history worth shipping.
- **Release PRs into `main`, and the `main` back-merge into `develop`: merge
  commit, never squash.** Squashing either one fabricates a commit and cuts the
  ancestry between the two branches, so every later release replays the same
  diff. This is why merge commits stay enabled on the repository.
- **Rebase and merge** only when a PR's commits are separate units that each
  earn their own line in history. Rare.

## Critical invariants — respect in every change

1. **fd reference invariant**: every `fd_list` slot holds exactly one
   `file->count` reference; removal always goes through `close_f`.
   → `docs/maintainer/vfs-and-fd-lifetime.md`
2. **`close_f` owns count decrements** — callers must never pre-decrement
   (the pre-#189 `vfs_destroy_task` bug).
3. **`do_exit` does NOT close fds** — zombie fds are closed by the reaper
   via `vfs_destroy_task`.
4. **Never trust syscall user pointers or lengths**: no validation layer
   exists (issue #191). No `strcpy`/`strlen`/unbounded walks on user
   memory; bound every copy and force NUL termination
   (`strncpy(dst, src, sizeof(dst)-1); dst[sizeof(dst)-1]=0;`); write at
   most `nbyte` to user buffers; enforce an ARG_MAX with `-E2BIG`.
   → `docs/maintainer/syscall-boundaries.md`, `execve.md`
5. **ext2 block index 0** is a valid sparse hole inside `i_size` (must read
   as zeros) — only invalid for metadata paths (issue #192).
   → `docs/maintainer/ext2.md`

## Test-harness warnings

- **`make qemu-test` exit code 0 does NOT prove the guest tests completed.**
  A kernel panic shuts QEMU down cleanly (exit 0); the CMake target ignores
  exit codes; CI swallows the TAP viewer's failure (issue #193). Currently
  main panics at test 10/39 — t_gid (issue #192). ALWAYS check the serial
  log: `grep -c PANIC` and the last `Running test (n/39)` counter.
- In-guest test output: use `syslog()` (serial); `printf` is VGA-only.
- New tests need BOTH `userspace/tests/CMakeLists.txt` (TEST_LIST) and
  `userspace/bin/runtests.c` (`all_tests[]`).
  → `docs/maintainer/testing-and-ci.md`

## Consult before changing a subsystem

| Changing... | Read first |
|---|---|
| task/fork/exit/waitpid | `docs/maintainer/process-lifecycle.md` |
| execve / ELF / argv | `docs/maintainer/execve.md` |
| fd lists, open/close/dup, file lifetime | `docs/maintainer/vfs-and-fd-lifetime.md` |
| ext2 or the disk image | `docs/maintainer/ext2.md` |
| /proc generation or read handlers | `docs/maintainer/procfs.md` |
| console / video.c / video backends | `docs/maintainer/video-backends.md` |
| pipes | `docs/maintainer/pipes.md` |
| allocators, UAF/double-free analysis | `docs/maintainer/memory-management.md` |
| any syscall | `docs/maintainer/syscall-boundaries.md` |
| CI / tests | `docs/maintainer/testing-and-ci.md` |
| anything security-relevant | `docs/maintainer/security-model.md` |
| reproducing anything | `docs/maintainer/debugging-playbook.md` |

Open bugs live in the GitHub issue tracker and nowhere else. Before filing
one, check `docs/maintainer/false-positives-and-dismissed-findings.md` —
things already investigated and deliberately not filed, plus the subsystems
nobody has ever looked at. Do not re-report what is in there without new
evidence.

<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **MentOS** (8161 symbols, 14698 relationships, 227 execution flows).

> Index stale? Run `node .gitnexus/run.cjs analyze --index-only` from the project root — it auto-selects an available runner. No `.gitnexus/run.cjs` yet? Bootstrap with `npx`, `bunx`, or `pnpm dlx` — e.g. `bunx gitnexus@latest analyze` (npm 11 npx crash; #1939).

## Always Do

- **MUST run impact before editing.** Use `impact({target: "symbolName", direction: "upstream"})` or `node .gitnexus/run.cjs impact "symbolName" --direction upstream --repo .`; report callers, processes, and risk. Never substitute grep for graph analysis.
- **MUST analyze graph changes before committing.** Use `detect_changes({scope: "all"})` (MCP) or `node .gitnexus/run.cjs detect-changes --scope all --repo .` (CLI fallback). `partial: true` or `truncated: true` is not a clean check — a zero means unseen, not unaffected; re-run it. For regression review: `detect_changes({scope: "compare", base_ref: "main"})` or `node .gitnexus/run.cjs detect-changes --scope compare --base-ref "main" --repo .`.
- MUST warn on HIGH/CRITICAL `risk` pre-edit; never use `riskSharedAxes` to waive a HIGH/CRITICAL `risk` warning. Compare File/symbol: MCP File omits axes; Graph-RAG expands File.
- **MUST treat `risk: UNKNOWN` as unresolved, not as low.** An empty caller set is not evidence the symbol is unused — it can also mean the callers are not resolvable by the index (plain-object property access, dynamic dispatch, cross-language calls). `impact` pairs `UNKNOWN` with a `riskNote` saying so. Confirm with a text search before treating the symbol as safe to change or delete; do not proceed on the strength of a zero.
- **MUST use `query({search_query: "concept"})` for concepts/flows, `context({name: "symbolName"})` for a named symbol, or `impact` for blast radius, on read-only callers, dependencies, imports, or execution flow.** Graph first; text search only for empty/`UNKNOWN`/literals.
- For security review, `explain({target: "fileOrSymbol"})` lists taint findings (source→sink flows; needs `analyze --pdg`).

## Never Do

- NEVER edit a function, class, or method before MCP/CLI impact analysis.
- NEVER ignore HIGH or CRITICAL risk warnings from impact analysis, and never read `UNKNOWN` as an all-clear — it means the walk could not answer, which is the one verdict that requires confirming by other means.
- NEVER rename symbols with find-and-replace — use `rename` which understands the call graph.
- NEVER commit before MCP/CLI graph change analysis.

## Resources

| Resource | Use for |
| --- | --- |
| `gitnexus://repo/MentOS/context` | Codebase overview, check index freshness |
| `gitnexus://repo/MentOS/clusters` | All functional areas |
| `gitnexus://repo/MentOS/processes` | All execution flows |
| `gitnexus://repo/MentOS/process/{name}` | Step-by-step execution trace |

## CLI

| Task | Read this skill file |
| --- | --- |
| Understand architecture / "How does X work?" | `.claude/skills/gitnexus-exploring/SKILL.md` |
| Blast radius / "What breaks if I change X?" | `.claude/skills/gitnexus-impact-analysis/SKILL.md` |
| Trace bugs / "Why is X failing?" | `.claude/skills/gitnexus-debugging/SKILL.md` |
| Rename / extract / split / refactor | `.claude/skills/gitnexus-refactoring/SKILL.md` |
| Tools, resources, schema reference | `.claude/skills/gitnexus-guide/SKILL.md` |
| Index, status, clean, wiki CLI commands | `.claude/skills/gitnexus-cli/SKILL.md` |

<!-- gitnexus:end -->
