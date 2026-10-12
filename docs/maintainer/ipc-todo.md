# IPC hardening roadmap

This checklist tracks work that remains after the IPC lifetime and resumable
blocking overhaul. Checked items are part of the current implementation; open
items are intentionally scoped for follow-up commits or pull requests.

## Completed baseline

- [x] Make semaphore operations atomic and resumable.
- [x] Make message send/receive block in the kernel with wait queues.
- [x] Preserve message-queue tail correctness after selective receives.
- [x] Add object references and tombstones for `IPC_RMID`.
- [x] Track shared-memory namespace, syscall, and VMA references.
- [x] Protect procfs IPC namespace traversal.
- [x] Serialize `msgget`, `semget`, and `shmget` publication by key.
- [x] Add lifetime tests for queue tail handling and `IPC_RMID` wakeups.
- [x] Add a concurrent key-publication stress test for all three namespaces.
- [x] Document lock, reference, and user-copy contracts in `ipc.md`.

## Required hardening

- [x] Add a test for a sender blocked on a full message queue when
      `IPC_RMID` wakes it with `EIDRM`.
- [x] Add a test for `IPC_RMID` while a shared-memory segment is still mapped;
      verify that the mapping remains usable until `shmdt`.
- [ ] Add stress coverage for concurrent `get`, `ctl`, `remove`, send/receive,
      and semaphore operations.
- [x] Audit lock ordering across namespace locks, object locks, and wait queues;
      document the permitted order and assert it where practical.
- [ ] Audit every IPC allocation and wakeup error path, including forked VMA
      acquisition and failed mapping rollback.
- [x] Re-run the full QEMU suite after each IPC change and inspect TAP plus
      serial output for panics.

## API completeness

- [x] Implement and test `shmctl(IPC_STAT)` with a kernel snapshot and bounded
      user copy.
- [ ] Define and test the supported error contract for missing or removed
      objects (`EINVAL`, `ENOENT`, and `EIDRM`).
- [ ] Add explicit resource limits for queue count, message bytes, semaphore
      sets, and shared-memory segments.
- [ ] Verify permission checks for every `get`, `ctl`, attach, send, receive,
      and semaphore operation, including owner/group/other modes.
- [ ] Review message size, semaphore count, and shared-memory size overflow
      handling at every arithmetic boundary.

## Design follow-ups

- [ ] Factor only genuinely shared namespace/reference helpers; keep queue,
      semaphore, and VMA-specific invariants visible.
- [ ] Evaluate a sleepable mutex primitive for longer IPC critical sections;
      never hold a spinlock across user memory, I/O, or `schedule()`.
- [ ] Add optional diagnostics for lock contention, wait-queue transitions,
      reference counts, and `IPC_RMID` wakeups.
- [ ] Revisit HIGHMEM-backed shared memory once a safe temporary `kmap`
      facility exists.
- [ ] Reassess the design for SMP before enabling multiple CPUs.

## Validation policy

Every checked item must have a focused test where practical, a focused
Conventional Commit, and a full `qemu-test` run before the corresponding PR is
considered complete. This file is a roadmap, not a substitute for a GitHub
issue: implementation bugs should still be tracked in the issue tracker.
