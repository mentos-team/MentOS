# Scheduler entry points and kernel contexts

MentOS has two scheduler entry points because it has two different callers.
They are both involved in a process switch, but they do not start from the
same kind of CPU context.

## The two MentOS paths

### `scheduler_reschedule_from_trap(pt_regs_t *frame)`

This path runs at the end of a syscall, timer interrupt, or user exception.
The common entry stub has already built a `pt_regs_t` frame. The function:

1. publishes that frame as the task's userspace return context;
2. handles pending signals and zombie bookkeeping;
3. chooses a runnable task;
4. restores the selected task's saved userspace frame, or resumes a task that
   is already asleep inside a syscall.

It must not resume a blocked task through the current trap frame. That frame
belongs to the task that was interrupted, not to the task being selected.

### `schedule(void)`

This path is called by a live kernel call chain after it has published a wait
condition. `switch_to()` saves the current task's callee-saved registers and
stack pointer in `thread.kernel_esp`. When the task is woken, the same
`switch_to()` returns and execution continues immediately after the call to
`schedule()` — for example, at `finish_wait()` in `pipe_read()`,
`sys_waitpid()`, or `sys_nanosleep()`.

`schedule()` therefore preserves a **kernel continuation**. It does not
restore a userspace trap frame and it does not manufacture a userspace retry.

## Why the names are deliberately different

`scheduler_reschedule_from_trap()` is a trap-boundary dispatcher. `schedule()`
is a voluntary blocking operation. Combining them would hide the ownership of
the two frames and would make it possible to return a blocked syscall through
the wrong userspace context.

The low-level assembly helper is still called `switch_to()`: it only saves and
restores the architecture-specific kernel stack context. It does not decide
whether the selected task should eventually return through `iret` or continue
inside a syscall.

## Correspondence with Linux terminology

Linux uses the same conceptual separation, although its names and layering are
more elaborate:

| MentOS | Closest Linux concept | Responsibility |
| --- | --- | --- |
| `schedule()` | `schedule()` / `__schedule()` | Block or yield from a live kernel call chain and select another task. |
| `scheduler_dispatch_next()` | part of `__schedule()` plus `context_switch()` | Decide what kind of saved context the selected task owns. |
| `switch_to()` | architecture `switch_to()` | Switch the low-level kernel execution context. |
| `scheduler_reschedule_from_trap()` | scheduler exit/reschedule work on syscall/IRQ return | Decide whether to reschedule before returning to userspace. |
| `scheduler_restore_context()` | architecture return-to-user path | Install the selected task's userspace register frame. |

Linux normally keeps a blocked syscall's call chain on the task's kernel stack;
`context_switch()` changes to that stack and the task returns from the switch
at the instruction after `schedule()`. It does not usually copy the syscall's
`pt_regs` into a separate continuation object. MentOS uses the same fundamental
stack-switch idea, while keeping an explicit `kernel_esp`/`user_regs` split
because its trap-return path and scheduler are simpler and more tightly
coupled.

The Linux names are useful vocabulary, not an implementation mandate. The
important invariant is the same: a trap frame is a userspace return context;
the stack saved by `switch_to()` is a live kernel continuation.

## Review rule for new blocking code

Every new blocking operation should follow this sequence:

1. establish the condition and publish a caller-owned wait entry while holding
   the condition's lock;
2. release the lock;
3. call `schedule()`;
4. call `finish_wait()` and recheck the condition in a loop.

It must not call `scheduler_reschedule_from_trap()` directly and must not rely
on userspace retrying the syscall to complete kernel-side cleanup.
