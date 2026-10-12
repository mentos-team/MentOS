# IPC lifetime and blocking contracts

This page describes the small System V-like IPC layer implemented by MentOS.
It is intentionally simpler than Linux, but follows the same important rule:
an object can disappear from the namespace while existing users finish using
it.

## Common rules

`semget`, `msgget`, and `shmget` create or find an object by key. `IPC_PRIVATE`
always creates a new object. Permission checks use the owner, group, and other
mode bits in `ipc.c`; creation flags are not themselves access checks.
Each namespace is bounded to `IPC_MAX_OBJECTS` live objects. Exhaustion returns
`ENOSPC` before the candidate is published.

Creation allocates outside the namespace spinlock, then rechecks and publishes
under that lock. If another caller published the same key first, the losing
candidate is discarded and the winner is returned. This keeps allocation out
of the critical section while preserving one namespace object per key.

All blocking operations use a kernel wait queue. The operation is retried in
the kernel after wakeup, while the caller remains in the same syscall:

```c
prepare_to_wait(&queue, &entry, TASK_INTERRUPTIBLE);
while (!condition) {
    schedule();
}
finish_wait(&queue, &entry);
```

The condition must be checked again after every wakeup. A wakeup is only a hint,
not ownership of the resource.

## Lock ordering

Each IPC object has an object lock; each namespace has a list lock. Ordinary
operations hold only one of these at a time. The removal path marks the object
under its object lock, then acquires the namespace lock to unlink it, and never
acquires the object lock again while holding the namespace lock. Wait queues are
not locks: they are prepared while the object lock is held, then the lock is
released before `schedule()`. No lock may be held across user-memory access,
I/O, or a wait-queue sleep.

## Semaphores

`semop` copies the complete operation array into kernel memory, validates every
operation, and evaluates the transaction while holding the set lock. Either all
operations are applied or none are. Negative operations and zero-wait
operations sleep in the kernel; `IPC_NOWAIT` returns `EAGAIN` instead.

`semctl(IPC_RMID)` marks the set removed, unlinks it from the namespace, wakes
waiters, and returns `EIDRM` to blocked operations. References keep the object
alive until the last syscall continuation releases it.

All `semctl` commands hold an object reference for their complete duration.
Metadata and semaphore values are read or changed under the set spinlock. User
arrays are copied to or from kernel buffers outside that lock, because a user
memory access may fault and resume the syscall.

## Message queues

`msgsnd` copies the message before sleeping, so a blocked sender does not retain
a user pointer. It waits for queue capacity and wakes receivers after enqueue.
`msgrcv` selects type zero, an exact positive type, or the lowest type bounded
by a negative selector. It wakes senders after removing a message.

Queue metadata and the linked message list are protected by the queue lock.
`msg_last` is maintained when a non-head message is removed; this matters for a
subsequent enqueue after a selective receive.

`msgctl(IPC_RMID)` uses the same tombstone/reference protocol as semaphores.
`msgctl(IPC_STAT)` takes a locked kernel snapshot before copying metadata to the
user buffer; it never holds the queue lock across that copy.

## Shared memory

Shared segments are backed by normal-zone pages because this 32-bit VM layer
does not yet provide a temporary `kmap` API for HIGHMEM. Every attachment is a
physical VMA with owner callbacks:

- namespace reference: the segment is discoverable by `shmid`/key;
- syscall references: active `shmat`/`shmctl` operations;
- VMA references: one for every attached mapping, including forked mappings.

`IPC_RMID` removes only the namespace reference. Physical pages and metadata are
freed after the final VMA/syscall reference is released. VMAs are page-aligned
and rounded up so a segment ending near a page boundary cannot fault on its
last word.

`shmctl(IPC_STAT)` takes a locked metadata snapshot and copies it to userspace
only after releasing the object lock. The same reference keeps the segment alive
for the entire command.

Allocation failures are handled as normal syscall errors: partially allocated
message queues and semaphore sets release their owned buffers before returning.
During `fork`, a failed VMA clone destroys every previously cloned area, so
SHM acquire callbacks are balanced by release callbacks and cannot leak a
segment reference.

## Deliberate scope

The implementation currently supports the commands needed by MentOS userspace:
`IPC_RMID`, `IPC_STAT` where implemented, semaphore value operations, and the
blocking send/receive paths. Linux-only commands such as `SEM_INFO`, `SEM_STAT`,
and advanced namespace limits are not silently emulated; they return
`ENOSYS`/the documented error until their contract is designed and tested.

When changing an IPC object, update both the relevant `userspace/tests` entry
and `userspace/bin/runtests.c`. `t_ipc_lifetime` covers message-queue tail
maintenance, blocked message receivers, and blocked semaphore operations
released by `IPC_RMID`. Always run the full QEMU suite and inspect the serial
log for panics; a clean QEMU exit alone is not proof of success.
