# Resumable Kernel Contexts — Implementation Plan, revision 2

**Status:** implementation-ready design; implementation is in progress on
`feature/resumable-kernel-contexts`. Runtime validation remains incomplete
until QEMU can run the focused continuation tests and the full regression
matrix.
**Updated:** 2026-10-05. **Target:** MentOS i386, uniprocessor, non-preemptive kernel.
**Source baseline:** `develop`, `2f2224cb41cf9093e4f5c4faa36411da176bdde8`, clean worktree when investigated.
**Replaces:** the original 2026-10-05 plan, SHA-256 `7283bf9a78737dca6056f501bd36142a79bb0b980db57e1e136937ddfb0465b7`.
**Tracking:** [#204](https://github.com/mentos-team/MentOS/issues/204), [#433](https://github.com/mentos-team/MentOS/issues/433), and [the architecture discussion](https://github.com/mentos-team/MentOS/issues/204#issuecomment-5931520033).

This is a plan for implementation, not permission to merge or publish changes. Follow the repository's AGENTS.md and `.github/copilot-instructions.md`. No particular agent framework or unavailable sub-skill is required. All new symbol/file names below are **proposed**, unless identified as existing. Recheck the baseline before executing; preserve unrelated changes and re-verify affected contracts if HEAD has moved.

## 1. Outcome and boundaries

Give each schedulable task its own kernel stack and preserve a kernel continuation across a voluntary `schedule()`. A blocking pipe syscall stays in the kernel, resumes after its scheduling call, rechecks its condition, and returns its actual result once. A wakeup makes a task eligible; it does not guarantee immediate execution or reserve data for it.

Deliverables:

- A single integer stack-switch primitive, coherent trap return and first-task/fork entry paths, per-task TSS stack selection, and safe stack lifetime.
- A scheduler core shared by trap-boundary scheduling and voluntary suspension, including a no-runnable-task path.
- Explicit user-frame, IRQ, FPU, signal, and wait-entry ownership contracts.
- Truly blocking anonymous pipe read and write, EOF, broken pipe, nonblocking behavior, and preserved partial progress.
- Kernel and guest tests that distinguish resumption from a userspace retry workaround.

Non-goals: SMP, kernel preemption, general kernel threads, shared `mm`/fd tables, `CLONE_VM`, Linux scheduling classes, complete POSIX syscall restart, new named-FIFO open/rendezvous semantics, or a global locking rewrite. Keep the existing one-task-per-process model. Leave terminal reads, `waitpid()` retry behavior, and `nanosleep()` semantics as compatible legacy users, with explicit follow-up work.

## 2. Linux comparison: contracts to adopt, machinery to omit

References were inspected on 2026-10-05. Linux **v2.6.12** is the historical i386 reference; **v6.12** is a pinned modern reference, not a claim about the latest release. Use source links and named functions rather than floating `master` citations. This plan contains original MentOS design instructions, not copied Linux implementation code.

| Topic | Primary reference and observed behavior | Decision for MentOS |
|---|---|---|
| Fork return | [v2.6.12 process.c](https://github.com/torvalds/linux/blob/v2.6.12/arch/i386/kernel/process.c), `copy_thread`: copies `pt_regs`, sets child EAX to zero, prepares kernel SP and `ret_from_fork`. [Historical entry.S](https://github.com/torvalds/linux/blob/v2.6.12/arch/i386/kernel/entry.S), `ret_from_fork`: joins syscall exit. | Build a complete child return frame and a dedicated trampoline. Do not enter a fork child with only EIP and user ESP. |
| Inactive context | [v6.12 entry_32.S](https://github.com/torvalds/linux/blob/v6.12/arch/x86/entry/entry_32.S), `__switch_to_asm`, and [process.c](https://github.com/torvalds/linux/blob/v6.12/arch/x86/kernel/process.c), `copy_thread`: inactive switch frame and user registers have distinct roles. | Keep the integer switch small; specify its exact stack layout and construct a matching synthetic frame. Linux's flags/mitigation/paravirtual machinery is not a template to copy. |
| Scheduling versus architecture | [v6.12 sched/core.c](https://github.com/torvalds/linux/blob/v6.12/kernel/sched/core.c), `context_switch`, `finish_task_switch`, `schedule_tail`: address-space transition, low-level switch and post-switch lifetime work are distinct. | One private scheduling core and one architecture activation helper. Preserve MentOS's reap-owned stack destruction; do not import Linux task/mm reference machinery. |
| Return to userspace | [v6.12 entry/common.c](https://github.com/torvalds/linux/blob/v6.12/kernel/entry/common.c), `exit_to_user_mode_loop`: rescheduling and signal work are resolved against the returning task's frame. | Deliver signals at a common user-return boundary, after a suspended syscall has cleaned up. Never deliver a handler by redirecting an arbitrary suspended kernel continuation. |
| Wait ownership | [v6.12 sched/wait.c](https://github.com/torvalds/linux/blob/v6.12/kernel/sched/wait.c), `prepare_to_wait_event`, `finish_wait`, `autoremove_wake_function`; [wait.h](https://github.com/torvalds/linux/blob/v6.12/include/linux/wait.h), wait-event loops. | Caller-owned entries for new resumable waits, condition recheck, no free by the waker. Keep legacy heap-owned entries explicitly distinguishable during migration. |
| Pipe waits | [v6.12 fs/pipe.c](https://github.com/torvalds/linux/blob/v6.12/fs/pipe.c), `pipe_read`, `pipe_write`, `pipe_writable`: lock release around waits, partial-result handling, and peer disappearance participate in completion. | Preserve progress, wake the opposite endpoint before sleeping after a partial transfer, and implement both EOF and no-readers completion. Use EINTR without Linux's internal restart codes. |
| Interrupt completion | [v6.12 irq/chip.c](https://github.com/torvalds/linux/blob/v6.12/kernel/irq/chip.c) and [i8259.c](https://github.com/torvalds/linux/blob/v6.12/arch/x86/kernel/i8259.c), `mask_and_ack_8259A`: acknowledgment belongs to IRQ-controller/flow handling. | Finish MentOS's PIC EOI before suspending the IRQ return path. Do not claim Linux universally sends EOI after a device handler: timing varies by controller/flow. |
| Floating point | [Linux floating-point API documentation](https://docs.kernel.org/core-api/floating-point.html): ordinary kernel FP use is restricted; explicit FP sections have constraints. | MentOS already uses FP in kernel code. Adopt the need for explicit ownership, but preserve it with an eager mechanism below rather than assuming Linux's no-FP compiler contract. |

These comparisons support the design; they do not prove the new MentOS implementation works. The acceptance tests below are the proof obligations.

## 3. Verified MentOS evidence and dependencies

All findings here are **source-derived**, not graph-derived. No MentOS GitNexus index was available. Maintainer documents provide invariants but contain historical snapshots; current executable code is authoritative.

| Existing location | Load-bearing fact / required adaptation |
|---|---|
| `kernel/src/process/scheduler.c:199`, `:308`, `:314` | Boundary scheduler snapshots `*f`, then replaces that same frame with the next task's snapshot. Replace this model atomically with own-stack continuation switching. |
| `kernel/src/process/process.c:236`, `:266`, `:545` | `__alloc_task` copies `thread_struct`; `sys_fork` calls it. Allocate one child stack after copying inheritable state, not a second stack in fork. |
| `kernel/src/process/process.c:723` | Successful exec writes `thread.regs` and calls `scheduler_restore_context(current, f)`. Migrate exec to write the live frame before retiring that API. |
| `kernel/src/process/user.S:36` | `enter_userspace` constructs only an initial user entry; it is not a complete fork-context restore. |
| `kernel/src/kernel.c:523`, `:551`, `:589` | Init exists before kernel unit tests; tests still run on the boot stack. The actual first user transition is later. Do not switch production runqueues during raw pre-user switch tests. |
| `kernel/src/descriptor_tables/interrupt.c:109`, `kernel/src/hardware/timer.c:97` | Timer schedules before EOI; both timer and common handler issue EOI. Several other IRQ handlers also send EOI. Centralize completion before stack switching. |
| `kernel/src/descriptor_tables/exception.S`, `interrupt.S`, `kernel/inc/kernel.h:24` | NASM entry stubs save segments, PUSHA, vector/error and hardware return frame. Ring-0 interrupts lack hardware user ESP/SS. |
| `kernel/src/system/syscall.c:142`, `kernel/src/system/signal.c:397` | Generic syscall return assignment overwrites EAX even when sigreturn has restored the frame. Special-case successful sigreturn as a frame-restoring operation. |
| `kernel/src/system/signal.c:457`, `:476` | Signal stop recursively calls scheduler_run. Replace recursion with stop-and-return to common exit work. Fatal actions must release signal locks before switching away. |
| `kernel/src/devices/fpu.c`, `syscall.c`, `timer.c` | FPU snapshots currently occur in syscall/timer handlers, with a shared aligned scratch buffer and lazy-owner bookkeeping. New entry and suspension paths cannot bypass or ambiguously reuse those saves. |
| `kernel/inc/klib/irqflags.h:31` | Returning `flags & 0x200` as uint8_t truncates IF. Both irq_disable and is_irq_enabled need normalization. |
| `kernel/src/process/wait.c:128` | Existing wake removes and frees an entry. New finish_wait cannot use that freed entry. |
| `kernel/src/hardware/timer.c:580`, `:613`, `:680` | Legacy nanosleep stores a heap wait entry in timer data; signal cancellation and timeout own cleanup. Preserve this ownership during migration. |
| `kernel/src/fs/pipe.c:487`, `:520`, `:821`, `:890`, `:980` | Pipe callbacks assume uninterruptible sleep; last-reader close does not wake writers; transfer loops can overwrite partial results. These are part of the conversion. |
| `kernel/inc/fs/pipe.h:15` | Current capacity is five 64-byte buffers, 320 bytes total. Do not assume a Linux-sized pipe or an existing PIPE_BUF contract. |
| `kernel/src/klib/mutex.c` | Mutex acquisition spins. It is not a sleeping mutex. No pipe holder may voluntarily schedule. |
| `kernel/src/process/process.c:150`, `:598` | Nested exec/shebang frames contain separate PATH_MAX=4096 arrays. Two-page task stacks are not justified. |
| `kernel/src/mem/paging.c:69`, `:171`; `kernel/src/mem/mm/mm.c:74`, `:139` | Kernel PDEs are prepopulated before process creation and copied directories share the tables. Verify late-allocated stacks across existing address spaces; do not reimplement #271. |
| `kernel/src/mem/page_fault.c:269` | Stack diagnostics use boot_info bounds. Teach them per-task bounds after activation, while retaining boot fallback. |
| `kernel/src/io/proc_running.c:256` | /proc consumes thread.regs snapshots. Retain those as diagnostics, never a second authoritative return context. |
| `CMakeLists.txt:92`; `kernel/CMakeLists.txt` | NASM is the assembler; kernel sources are globbed. `kernel/src/CMakeLists.txt` does not exist. |
| `kernel/src/tests/runner.c`; `userspace/bin/runtests.c:25` | Kernel suites need runner registration; guest all_tests entries are strings, not name/path pairs. |

Issue status checked on 2026-10-05: #271, #423, #219, #389, #421 and #422 are closed; #204 and #433 are open. In particular, the prerequisite milestone is not a list of work to blindly repeat. Source confirms the kernel PDE population and kernel failure counter at this baseline. Re-run their relevant regression tests as gates.

Read before implementation: `docs/maintainer/{process-lifecycle,execve,vfs-and-fd-lifetime,pipes,memory-management,syscall-boundaries,security-model,testing-and-ci,debugging-playbook}.md`; for /proc changes also `procfs.md`.

## 4. Architecture decisions and invariants

### 4.1 Context representations

1. **Canonical user frame:** the outer ring-3 entry's `pt_regs_t` on the task's own kernel stack. A proposed `thread.user_regs` points to it while in kernel, or to the prepared synthetic frame before first entry. Clear the pointer immediately before actual user return; never dereference a stale pointer from user mode. Nested ring-0 traps do not replace it and must not copy absent useresp/ss fields.
2. **Kernel continuation:** saved ESP pointing to the integer switch frame on the task stack. Its return address resumes the suspended call chain.
3. **Diagnostic snapshot:** retain `thread.regs` initially for /proc and early init construction. Refresh it at outer entry, after exec/frame changes, and before user exit. No scheduler writes it over another task's live frame.
4. **Signal snapshot:** retain the existing signal_regs convention for this series. Preserve the entire restored GPR frame on sigreturn. Nested signal-frame redesign is deferred.

Proposed thread additions: stack allocation base/top, saved kernel ESP, live user_regs pointer, and kernel FPU context storage. Ownership fields are never inherited by memcpy. `kernel_stack_top` is one-past the usable stack range; TSS esp0 is set to this address. Stack grows downward; the outer user frame is near the high end, not the low-address bottom.

### 4.2 Stack allocation and lifetime

Use paired `task_kernel_stack_alloc/free` helpers in process.c. Start with **32 KiB**, eight pages (`KERNEL_STACK_ORDER = 3` for 4096-byte pages), via existing `alloc_pages_lowmem(GFP_KERNEL, order)` and `free_pages_lowmem(base)`. This avoids kmalloc size-class ambiguity and provides page alignment. This is a conservative starting budget, not a proof that every call chain fits.

- Reserve a small low-address red zone/canary; poison unused bytes for debug high-water measurement.
- Record the allocation extent separately from the current ESP. Verify 16-byte stack alignment where calling compiled C, including trampolines.
- Both old and new stacks, task objects, and switch code must remain mapped at the same virtual addresses across CR3 changes. All stack pages must be supervisor-only.
- Allocate the stack before publishing the task or linking it to its parent. Fail stack allocation with `-ENOMEM`, without kernel panic or inherited ownership leaks.
- Free failed-construction stacks on rollback; free successful tasks' stacks only in the reaper, from a different task's stack. Assert victim != current and no linked caller-owned wait entry.
- do_exit still does **not** close fd slots. close_f remains the only owner of count decrements. mm destruction and kernel-stack lifetime are independent.
- Retain the boot stack for early boot/unit tests. A proposed one-way `scheduler_start_first_task()` switches from it to init's prepared context; store the discarded boot ESP in a dedicated variable, never in init's saved ESP. Set task-context-active only at this handoff.
- Real unmapped guard pages are deferred. Do not label a canary or the first byte of an allocation a guard page. Update page-fault diagnostics to report the active task bounds and use boot_info only before handoff.

Measure Debug and Release call chains with compiler `-fstack-usage` and runtime watermark checks. Move the large exec filename/shebang scratch buffers to explicitly owned heap storage, preserving rollback and bounded user copies. Require at least 25% observed headroom in covered stress paths, record the worst path, and increase the budget or reduce usage if the gate fails. Report the static-analysis coverage limits; a watermark alone is not a universal bound.

### 4.3 Integer switch and first return

Keep `void switch_to(uint32_t *prev_esp, uint32_t next_esp)` implemented in **NASM**, matching existing `.S` files. Caller has IF=0 and DF clear. Save EBX, ESI, EDI, EBP in that push order; store ESP; load next ESP; pop EBP, EDI, ESI, EBX; ret. Caller-saved GPRs need not be preserved by this primitive. IRQ restoration belongs to the resumed caller, not the assembly's ret.

The layout seen at saved ESP is:

| Offset | Value |
|---|---|
| +0 | EBP |
| +4 | EDI |
| +8 | ESI |
| +12 | EBX |
| +16 | return address (normal caller or proposed ret_from_fork trampoline) |
| +20 | caller stack, or synthetic pt_regs_t for first user entry |

For new user tasks, place a complete pt_regs_t above that switch frame. ret_from_fork is an assembly trampoline, not a C function entered with a fake caller frame. It records the frame pointer, aligns the stack for its C helper, runs first-return work, restores ESP to the frame, and uses the common complete GPR/segment/iret epilogue. Add compile-time offset/size checks against `kernel.h` and assembly constants. Distinct register patterns, not zeros alone, test the layout.

Fork copies the parent's live user frame, sets child EAX=0, and copies the saved **user** FPU state. It never copies a live kernel call chain. Init gets initialized user selectors (`CS=0x1b`, data/SS=0x23), EIP/ESP/EBP and valid EFLAGS, including fixed bit 1 and IF. Preserve the intended existing user I/O privilege policy explicitly; do not accidentally grant new EFLAGS privilege bits or break the test-exit mechanism by inheriting arbitrary kernel flags.

Exec preserves the active kernel stack and user_regs pointer, commits the new mm, writes the new user return registers into `f`, activates the new pgd even if current remains selected, and refreshes the snapshot. Failed exec leaves both old mm and old live frame intact. Remove/replace scheduler_restore_context only after these callers are migrated.

### 4.4 IRQ and return-to-user boundary

The required MentOS ordering is device handlers -> common PIC EOI -> leave hard-IRQ context -> optional scheduling/user-return work -> register restore/iret.

- timer_handler updates time, callbacks and a per-task need_resched request; it does not switch stacks or send its own EOI.
- irq_handler owns one EOI per IRQ dispatch, with existing slave/master behavior. Remove duplicate EOI calls from timer, keyboard, mouse and ATA IRQ handlers after verifying they are dispatched through this path. Device-specific acknowledgments remain device-specific.
- Add hard-IRQ-depth tracking. Increment/decrement only around actual IRQ work; it must be zero before switching. A ring-0 IRQ may wake tasks and request rescheduling but returns to the interrupted kernel code. No arbitrary kernel preemption is introduced.
- Centralize outer user-return work for IRQs, syscall exceptions and user faults. Exception handlers report/queue signals; remove their direct scheduler_run calls after routing return through the common boundary.
- Keep syscall execution IRQ behavior unchanged initially: entry disables IRQs; explicit scheduling/idle provides opportunities for other tasks and IRQ wakeups. Do not globally enable IRQs throughout kernel C as part of this series.
- The syscall dispatcher captures the original syscall number before invocation. Successful sigreturn must not have restored EAX overwritten with the C return value; ordinary syscalls still assign their result to f->eax.
- Common exit checks current state, reschedules if necessary, then handles signal work using this task's frame. Recheck state after stop/exit. Return to user only with TASK_RUNNING, current CR3 and TSS correct. Recheck pending work after an actual suspension, without endlessly re-delivering a newly installed handler.
- __do_signal_stop sets state and returns to exit work; remove recursive scheduler_run. Fatal signal branches release sighand locks and leave do_signal promptly after do_exit. Never inspect user frames/mm after destroying that mm.

All accesses to next/current after a switch must respect continuation semantics: when switch_to returns to A, current is A, and a cached pointer to the task A once switched to may already have been reaped. Do not add post-switch cleanup that dereferences such a stale next pointer.

### 4.5 Scheduler and idle

Implement schedule() in scheduler.c, declare it in scheduler.h. Keep runqueue internals private to scheduling code; do not create a second picker/activation path. Existing scheduler_pick_next_task remains the policy function.

- Public schedule() saves normalized IF, disables IRQs, asserts task context active and hard_irq_depth=0, calls a private core, then restores its caller's IF when that caller resumes. It may be called with IF initially zero or one.
- The core handles runnable selection, zombie dequeue and architecture activation for both boundary and voluntary calls. If prev is sleeping, it may return only after prev becomes runnable; selecting prev while runnable is legal, selecting no task is not a reason to return.
- Choose a **scheduler-local idle loop** for the first series, not a new kernel task type. While no candidate exists, execute contiguous `sti; hlt; cli` with compiler memory clobber, then retry the picker with IF=0. Hold no locks across it. Ring-0 IRQ return does not schedule recursively.
- It is valid to idle on the blocked/outgoing stack: UP plus no intervening task execution prevents another task from reaping it. The loop cannot return a zombie to user mode. If the outgoing mm has been destroyed, execution uses the main kernel pgd until another task is activated.
- One proposed `arch_switch_tasks(prev,next)` updates current, TSS and CR3 with IRQs off, preserves kernel FP context as below, then calls switch_to. No locks cross the switch. No fresh task with null mm is accepted in the production runqueue.
- A per-task need_resched bit is cleared when that task's scheduling request is serviced, not by blindly clearing another task's request after a switch. State-driven blocking takes precedence over periodic-policy early returns.
- Keep accounting in the existing picker coherent. Do not bill idle ticks as runtime after resumption; update exec_start when a task actually resumes. Check configured policies, but do not fill unrelated pedagogical algorithm placeholders.

Normalize irq_disable()/is_irq_enabled() to return `(flags & (1U << 9)) != 0`, preserving the existing uint8_t API. Verify nested disable/restore and every error path, including __sleep_on_state's no-current branch.

### 4.6 Floating-point contract: eager isolation for MentOS

Do not assume that kernel C is FP-free: scheduler accounting and the freestanding library already use FP. Preserve two distinct task states: saved user FP image (existing fpu_register) and saved live kernel FP image for a suspended continuation. Use valid initialized FXSAVE images, including MXCSR; zero-filled bytes are not an initialized FP context.

Chosen mechanism:

1. After FPU initialization, low-level IRQ/exception entry saves the interrupted hardware FP state into a 16-byte-aligned, entry-local 512-byte area **before calling any C code**. This scratch lies below the pt_regs frame, never changes its layout, and is not shared across nested entries. Entry loads a known clean kernel FP image before C runs. Early boot entries skip this via an explicitly initialized readiness flag.
2. On an outer user entry, copy that interrupted image into the task's user FP image. On a nested ring-0 entry, keep it private to that entry. The current task's saved user FP image must never be overwritten with interrupted kernel FP values.
3. Immediately around an actual task switch, arch_switch_tasks saves outgoing live kernel FP to its kernel image and restores incoming kernel FP. Use aligned storage or the existing aligned scratch technique under IF=0; enforce no FP-generating C/logging between final restore and integer switch, verified in disassembly. Fresh kernel continuations start with the clean image.
4. Ring-0 trap exit restores its entry-local image; user exit restores the task's user image in an assembly tail after all C exit work. Fresh fork return follows the same user restoration contract without pretending an entry-local image exists.
5. Remove the old unconditional switch_fpu/unswitch_fpu calls from syscall/timer handlers in the same activation commit. Retire conflicting lazy-owner behavior or make DEV_NOT_AVL explicitly consistent with eager ownership; do not leave thread_using_fpu able to point at a reaped task or restore the wrong snapshot. Keep actual FP exception/invalid-op signal handling.

This costs more saves than a Linux-style restricted kernel-FP API, but preserves MentOS behavior without a project-wide FP rewrite. Include its stack overhead in the budget. Test x87 values/control state and SSE/MXCSR where the configured CPU supports them, including first fork entry, IRQ during idle, and alternating user/blocked-kernel continuations. FP save/restore routines must not instrument themselves with FP-using logging. Full nested signal FP ABI redesign is not part of this series; no regression of existing signal behavior is acceptable.

### 4.7 Lifecycle and user memory

do_exit may retain its current return-to-boundary shape in this series, but after EXIT_ZOMBIE it must not resume normal user work. It destroys the user mm, sets the pointer null after destruction, and reaches the scheduling core without dereferencing it. Make affected diagnostics null-safe where this new explicit state is observed; do not broadly refactor /proc.

The reaper releases the victim's stack only after it has switched away. Failed fork construction has a separate teardown path; do not apply reap logic to partially initialized objects. Acquire fallible stack/mm resources before fd duplication where practical. Check existing task/PID/fd allocation results; if partially duplicated fd references must be undone, balance only acquired references through close_f and track pipe-count/proc publication stages explicitly. Never use vfs_destroy_task on an incompletely constructed fd table without proving its preconditions. A bounded VFS construction rollback fix is allowed if needed by these failure paths, not an unrelated VFS redesign.

No other task shares the blocked task's mm/fd table; signals are delivered only after the continuation unwinds. Therefore voluntary switching alone does not introduce another thread that can unmap this caller's buffer or close its fd. Preserve existing uaccess validation, restore the right CR3 before resuming a copy, and update the obsolete single-stack explanation in uaccess.h. Shared-mm fault-recovering copy semantics remain a prerequisite for future threads, not for this UP feature. Do not replace generic VFS kernel-buffer support with unconditional user-only checks inside pipe buffer helpers.

## 5. Wait protocol and transition compatibility

### 5.1 Ownership

Add explicit ownership metadata to wait_queue_entry_t: either caller-owned or legacy heap-owned. Do not overload WQ_FLAG_EXCLUSIVE. wait_queue_entry_init initializes caller ownership; legacy __sleep_on_state sets heap ownership after initialization. Audit every allocation/initialization caller so initialization does not accidentally erase ownership.

Proposed new APIs:

```c
void prepare_to_wait(wait_queue_head_t *head,
                     wait_queue_entry_t *entry, long state);
void finish_wait(wait_queue_head_t *head,
                 wait_queue_entry_t *entry);
```

The caller initializes task/callback/private/list before publication. prepare atomically links if needed, sets state and waiting_on with IRQs disabled and the queue lock held. It does not allocate or schedule. finish removes if still linked, restores runnable state and clears matching waiting_on; it never frees caller storage and is safe after an autoremove wake.

The wake layer unlinks with list_head_remove (which reinitializes links), makes the task runnable, and frees **only legacy heap-owned** entries. A caller-owned entry survives until its blocked function calls finish and exits. Old sleep_on/sleep_on_interruptible remain mark-and-enqueue operations; changing them into synchronous sleeps would break keyboard/nanosleep/old pipe paths.

### 5.2 Locking and signals

- All wait-list mutations and traversal occur under the queue lock with IRQs disabled. Refactor internal locked helpers to avoid recursively calling public remove_wait_queue while already holding the lock.
- Pipe condition lock precedes wait-queue lock. A wake predicate cannot acquire the pipe mutex, signal lock, or call schedule. New pipe waiters use the generic state-based predicate and recheck their condition in process context; remove the old callbacks when unused.
- Signal sending currently holds sighand then takes a wait-queue path. Keep that order; wait preparation's pending-signal check must not take sighand while holding the queue lock. Under this UP/IF=0 model, provide a non-mutating pending-deliverable-signal helper with a documented IRQ-off precondition.
- A signal must wake an interruptible pipe waiter even when there is no data/space. Do not let a pipe-specific predicate veto it. Honor masks and existing ignored-signal rules; SIGKILL remains deliverable. finish_wait runs before signal delivery/termination at user exit.
- Preserve legacy nanosleep timer cancellation: cancel timer before wake can free its heap entry; timer callback must never retain a pointer to an entry freed by the signal path. Regress timeout-versus-signal behavior.

The pipe condition and waiter publication use this ordering (schematic, proposed APIs):

```text
lock pipe
check transfer/EOF/broken-pipe/nonblock conditions
if pending deliverable signal: unlock and return EINTR or partial progress
prepare_to_wait(queue, caller_entry, TASK_INTERRUPTIBLE)
unlock pipe
schedule()
finish_wait(queue, caller_entry)
repeat from lock pipe and condition check
```

No producer can change this pipe's condition between the check and publication: kernel process context is non-preemptive, the condition mutex is held, and prepare protects queue/state against IRQ wakeups. Once unlocked, an early wake changes the task to RUNNING; schedule must not overwrite that state. Wakeups are notifications, not ownership of bytes. Test early wake, spurious wake and a competing consumer that consumes the condition first.

## 6. Pipe behavior to implement

Convert read first, then write in a separate tested step. Keep the existing buffer representation for this series. Fix traversal and capacity accounting sufficiently to handle partially consumed buffers and wraparound: global has_data/has_space must agree with what the next transfer can actually reach. Test the case where a partially consumed 64-byte buffer has len < 64 but offset+len == 64. Compact/reuse buffer space only in a way that preserves FIFO ordering; never skip ahead and reorder the stream.

**Read:** after validating the file, zero length returns zero; consume up to the requested count from currently available data and return a positive short count rather than discarding it when the next buffer is empty. Empty plus no writers returns EOF. Empty plus writers in O_NONBLOCK returns -EAGAIN. Otherwise prepare/unlock/schedule/finish/recheck. Wake writers after freeing capacity. Kernel-buffer callers remain supported.

**Write:** zero length returns zero. No readers queues SIGPIPE and returns -EPIPE if no bytes were written, otherwise preserves the positive partial result. Maintain `done` and buffer offset across each sleep. Fill available capacity without losing earlier progress. Wake readers **before** blocking for further space; delaying wake until the whole large write completes can deadlock. Nonblocking full pipe returns -EAGAIN only if done=0, otherwise the partial count. A signal similarly returns -EINTR only with no progress. A large blocking write loops until complete or interrupted/peerless.

**Close:** under the pipe condition protocol, last-writer close wakes readers; last-reader close wakes writers regardless of space. Keep existing fd reference counting: close_f decrements count, not its caller. The waiter still owns an open endpoint, so the pipe object cannot be freed while its syscall remains in progress under the one-task-per-process assumption. Assert/drain no linked waiters before final deallocation. Do not add scheduler calls while holding the pipe mutex.

**Atomicity scope:** this series does not introduce a public POSIX PIPE_BUF promise: current total capacity is 320 bytes and no such constant was found in lib/inc/limits.h. A transfer performed entirely under one mutex hold remains indivisible relative to other writers; a write that sleeps after partial progress may interleave. Document this limitation and test byte conservation/per-writer order rather than falsely claiming POSIX small-write atomicity. A capacity/PIPE_BUF redesign is a separate follow-up.

Keep the zombie-fd policy explicit in tests: exit does not close endpoints until reap. Close peer endpoints explicitly for EOF/EPIPE tests, and separately test the existing reap behavior. Do not silently change do_exit to close fds just to make a Linux-oriented test pass.

## 7. Implementation sequence and commit gates

Branch from develop; use repository commit types (`feature`, `fix`, `test`, `documentation`, etc.), subjects <=72 characters, and reference #204. Fix/feature PRs target develop and squash on merge. No command in this plan authorizes pushing or merging. The activation milestone below must not be split into separately mergeable, half-working return-path changes.

### M0 — Baseline and prerequisites (independently reviewable)

- [ ] Record HEAD and dirty paths; verify source anchors and prerequisite issue fixes.
- [ ] Run existing Debug guest/kernel suites once and record baseline results, including existing failures. Do not interpret a pre-existing failure as a new regression or silently mark a failing baseline green.
- [ ] Fix uint8_t IF normalization and unmatched restore error paths; add meaningful IRQ-state tests that restore the original state before returning.
- [ ] Collect stack-usage data; heap-allocate large exec scratch with complete cleanup. Pick the initial 32 KiB budget and record static/runtime margins.
- [ ] Add paired stack allocation/free helpers and failure handling without activating per-task TSS/return paths yet. Prefer adding stack acquisition before fd/proc/parent publication to minimize rollback.
- [ ] Gate: build, relevant allocation/exec/IRQ tests, no changed user scheduling behavior.

### M1 — Integer switch primitive and raw-context tests

- [ ] Add `kernel/src/process/switch.S` and `kernel/inc/process/switch.h`, NASM syntax, exact frame contract and non-executable-stack note.
- [ ] Add `kernel/src/tests/unit/test_kernel_context.c` and a test-only NASM helper beside it; register suite in `kernel/src/tests/runner.c`. CONFIGURE_DEPENDS discovers sources; tests directory remains excluded in normal builds.
- [ ] Use a small raw test-context struct with stack+saved ESP, not fake production tasks with null mm. Save boot caller ESP in the harness; B explicitly switches back and never falls through to a dummy return address.
- [ ] Test distinct callee-saved registers with an assembly probe, nested C locals/canaries, 1000 A/B round trips, first trampoline entry, proper C stack alignment and IF=0 throughout the primitive. C volatile variables alone do not prove register preservation.
- [ ] Restore all test global/IRQ state and free test stacks after returning to the harness stack. Run before first init entry without changing runqueue.curr/TSS.
- [ ] Gate: kernel suite completes and reports actual assertions, plus objdump inspection of call convention. No production scheduler migration yet.

### M2 — Atomic activation of per-task contexts and return paths

- [ ] Add canonical user_regs pointer and diagnostic snapshot rules; build fork/init synthetic frames only after their mm/registers are ready.
- [ ] Implement first-task boot handoff and complete ret_from_fork/restore-frame path; verify user selectors/EFLAGS and child EAX.
- [ ] Implement common trap entry/exit FP handling and per-task kernel FP save/restore, including pre-FPU boot bypass.
- [ ] Refactor IRQ handler completion/EOI ownership and hard-IRQ tracking; timer sets reschedule request, no inner scheduling.
- [ ] Introduce common user-return work for IRQ/syscall/fault paths; migrate page-fault/GP/signal-stop direct scheduler calls and preserve sigreturn EAX.
- [ ] Implement private scheduler core, architecture activation, no-runnable idle and public schedule(); use that core from boundary exit.
- [ ] Update exec to modify f and activate its committed mm; retain failed-exec rollback. Remove old cross-task frame replacement after every caller is accounted for.
- [ ] Wire exit/reap stack lifetime, clear destroyed mm pointer, eliminate stale FPU owner pointers, and update stack diagnostics with boot fallback.
- [ ] Gate: Debug and Release guest suites; fork GPR/FP probe; exec ELF/script/failure; signals/stop/continue/sigreturn; user page fault; CPU-bound timer alternation; all-blocked timer wake; repeated fork/exit/reap.

M2 may consist of several local development commits, but merge it only as a complete, working transition. If bringing it up experimentally, keep the experiment off by default until this gate passes; do not make half-converted TSS/frame semantics the default.

### M3 — Resumable wait API with legacy compatibility

- [ ] Add explicit wait-entry ownership and prepare/finish helpers. Convert wake internals to one IRQ-safe lock protocol with locked/unlocked helper separation.
- [ ] Preserve legacy __sleep_on_state ownership and asynchronous return semantics for keyboard, nanosleep and waitpid.
- [ ] Add pending-deliverable-signal query and signal-wake compatibility without reversing sighand/queue lock order.
- [ ] Test caller entry survives wake and finish, legacy entry frees exactly once, repeated/early wake, signal wake without pipe readiness, and no-current error restoration.
- [ ] Keep pre-user kernel tests limited to raw contexts and queue/state mechanics, restoring any temporarily substituted current task. Perform real scheduler integration through M4/M5 guest tasks with valid mm objects and the trace hooks specified below. Do not call the production schedule() from a fake pre-handoff task or introduce a general kernel-thread API merely for testing.
- [ ] Gate: unit suites plus legacy nanosleep, waitpid, keyboard/terminal smoke and signal regressions. No pipe calls schedule while holding its mutex.

### M4 — Blocking pipe read

- [ ] Implement short reads/zero length/EOF/nonblock and the caller-owned wait loop, initially leaving write's legacy empty/full scheduling behavior compatible.
- [ ] Replace new-reader wake predicate with generic wake-and-recheck semantics; ensure waiting pointer and entry are clear before syscall return.
- [ ] Add `userspace/tests/t_pipe_block.c`; register `t_pipe_block` in TEST_LIST and the string `"t_pipe_block"` in all_tests[]. Include errno.h, sys/wait.h, syslog.h, and other used API headers.
- [ ] Test one read with a delayed producer, data smaller than requested buffer, EOF after buffered data, two competing readers, nonblocking errno, zero-length read and signal interruption. Check fork/write/close/wait status and explicitly close inherited unused endpoints.
- [ ] Remove acceptance of EAGAIN from existing blocking-pipe tests where it masks the old workaround.
- [ ] Gate: targeted tests and normal guest suite, with instrumentation proving a read crossed multiple task selections before returning.

### M5 — Blocking pipe write and close symmetry

- [ ] Preserve write progress across waits; wake readers before sleeping; implement last-reader wake, SIGPIPE/EPIPE and signal interruption.
- [ ] Fix buffer traversal/capacity disagreement and wraparound without changing FIFO ordering or claiming PIPE_BUF semantics.
- [ ] Extend guest tests with full-pipe writer, transfer larger than capacity, partial nonblock, reader disappearance, competing writers and checksummed byte conservation. Test SIGPIPE caught/ignored/default dispositions using the supported signal API.
- [ ] Remove pipe_put_process_to_sleep and obsolete callbacks when no callers remain; do not remove legacy sleep_on globally.
- [ ] Gate: repeated multi-process pipe tests, complete guest/kernel suites in Debug and Release, no stack/wait leaks. Only now claim #433 addressed and #204's pipe demonstration complete.

### M6 — Documentation and final integration review

- [ ] Update process-lifecycle.md, pipes.md, testing-and-ci.md and uaccess.h to state actual continuation/ownership/IRQ contracts and retained limitations.
- [ ] Search for stale single-stack and cannot-resume comments and all thread.regs/scheduler_restore_context/sleep_on callers. Classify every remaining legacy wait explicitly.
- [ ] Record validation evidence, stack budget, transient memory cost, no-preemption assertions and unresolved pre-existing test failures. Do not close tracking issues solely because a smoke test boots.
- [ ] Review the final diff for one stack allocation per task, exactly-once frees, full user-frame restoration, EOI before switching, no caller-owned entry freed by wake, and no sleeping with the pipe mutex held.

## 8. Test design and acceptance matrix

All names here except existing suites are proposed. Use existing registration conventions; do not assume sched_yield is implemented (only its syscall number was found). Use timer/nanosleep for guest delays, and test-only hooks for deterministic scheduling-window assertions. Do not expose a new production syscall just for a test.

| ID | Scenario | Required observation |
|---|---|---|
| K1 | Raw A/B contexts, 1000 switches | Same local values, exact nonzero EBX/ESI/EDI/EBP patterns, intact red zones, correct ABI alignment. |
| K2 | IRQ flags enabled/disabled/nested | Saved state is normalized and restored correctly on success/error; test restores harness state. |
| K3 | Allocate stacks after two mm objects exist | Every page accessible with each CR3, supervisor-only PTEs, old/new stacks remain valid across switch. |
| K4 | Forced stack allocation failure; repeated successful creation/reap | Parent state intact; no PID/fd/proc/stack leaks; stack never freed while executing on it. Use a narrow allocator test hook, not uncontrolled global exhaustion. |
| K5 | Prepare -> force wake -> schedule -> finish | No sleep-state overwrite, UAF, double-free or lost wake. Entry self-linked and waiting_on null afterward. |
| K6 | Wake two readers, first consumes data | Second rechecks and waits again, no false success or lost later wake. |
| K7 | No runnable task; timer wakes a waiter | Tick counter advances during idle, no recursive scheduling of ring-0 IRQ, no busy loop with IF=0. |
| U1 | Assembly fork probe | Child EAX=0; other user GPR/stack/segment values survive; parent receives child PID. Probe saves registers immediately after the actual syscall instruction, before libc can alter them. |
| U2 | Exec ELF, shebang, long valid argv, invalid executable | New image runs on same kernel stack; failure preserves old image; canary/headroom valid. |
| U3 | Two CPU-bound processes over many ticks | Both progress without cooperative syscall scheduling; catches delayed EOI. |
| U4 | Distinct x87/SSE/control state across fork, ticks and blocking | User states do not cross-contaminate; nested IRQ does not replace saved user state with kernel state. |
| U5 | Signal handler return, stop/continue, fatal signal | Restored EAX and frame survive dispatcher; stopped task resumes safely; dead task never irets. |
| P1 | Empty blocking read, delayed write, third runnable process | Exactly one read invocation returns actual bytes after several scheduling decisions; no userspace EAGAIN retry. |
| P2 | Read 16 with only 5 available; buffered EOF; zero read | Returns 5, drains before EOF, zero request does not wait. |
| P3 | Empty/full O_NONBLOCK | -1 and errno=EAGAIN only with no progress; valid positive partial write when some space exists. |
| P4 | Blocking write >320 bytes with delayed reader | Writer sleeps and resumes; readers are woken before writer waits; exact bytes/order/checksum. |
| P5 | Last writer or reader explicitly closes | Reader gets EOF; blocked writer wakes and gets defined SIGPIPE/EPIPE or partial result. |
| P6 | Deliverable/masked signal during wait, including SIGKILL | Deliverable signal unblocks, cleanup precedes handler/exit; masked signal does not cause erroneous EINTR. |
| P7 | Competing readers/writers, buffer partial consumption and wrap | No duplicate/lost bytes, per-writer order, no starvation caused by sleeping with mutex held. No global PIPE_BUF promise. |
| L1 | Existing waitpid/nanosleep/terminal and timer cancellation | Legacy queue ownership and observable behavior remain compatible. |

Deterministic evidence for P1/K5: add proposed CMake option `ENABLE_KCTX_TEST_HOOKS` in kernel/CMakeLists.txt, default OFF, enabling a kernel-only compile definition. A test-only bounded event buffer records PID, scheduling reason, blocked/runnable transition, prepare, wake and return. Add a controlled one-shot early-wake hook after the new pipe wait's publication/unlock; it makes the waiter runnable before schedule, then the loop must correctly wait again if the pipe is still empty. Queue-only kernel tests verify its mechanics; instrumented guest tests verify the actual schedule continuation. The hook does not fabricate pipe data or change syscall results. Flush bounded trace records to serial only after leaving locks/critical windows. Validate that a single syscall-entry ID encloses prepare, multiple switches, wake, resume and one return; check the early-wake sequence separately. A guest nanosleep delay alone is useful coverage but does not prove that the reader actually slept. Run instrumented builds for this evidence and normal builds for the final regression gate; add no production syscall or permanent /proc control endpoint for testing.

New kernel suites register in runner.c and report through its failure counter. They are not TAP tests by default: require the suite completion summary, zero failed assertions and the debug-exit result, not a nonexistent `ok t_kctx_switch` line. Guest tests use syslog, not VGA-only printf. A timeout is failure, never an acceptable substitute for a missing result.

## 9. Build and verification procedure

Do not run Debug and Release guest builds concurrently: userspace test binaries are staged into the shared `filesystem/bin/tests` source tree. Record git status before/after and preserve user changes. Separate build directories do not isolate that staging tree. Rebuild the matching filesystem/ISO for each configuration; never reuse a stale image.

```sh
# Debug userspace integration
cmake -S . -B build-kctx-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-kctx-debug -j 4
cmake --build build-kctx-debug --target qemu-test

# Debug kernel suites (includes raw switch tests)
cmake -S . -B build-kctx-kt -DCMAKE_BUILD_TYPE=Debug -DENABLE_KERNEL_TESTS=ON
cmake --build build-kctx-kt --target qemu-kernel-test -j 4

# Release userspace integration, after the Debug run is complete
cmake -S . -B build-kctx-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-kctx-release -j 4
cmake --build build-kctx-release --target qemu-test

# Release kernel suites
cmake -S . -B build-kctx-kt-release -DCMAKE_BUILD_TYPE=Release -DENABLE_KERNEL_TESTS=ON
cmake --build build-kctx-kt-release --target qemu-kernel-test -j 4
```

For stack profiling add compiler-supported `-fstack-usage` to a dedicated kernel target option during M0; top-level CMAKE_C_FLAGS is reset by current configuration, so merely passing a cache variable is not a reliable injection method. Retain `.su` artifacts and inspect call-chain sums, recursion and indirect-call uncertainties.

For the M4/M5 trace gate, reconfigure the relevant Debug/Release guest build with `-DENABLE_KCTX_TEST_HOOKS=ON`, rebuild and run qemu-test sequentially, and inspect the bounded trace contract above. Then reconfigure with `-DENABLE_KCTX_TEST_HOOKS=OFF`, rebuild and run the final normal suite. A generic option left unused by CMake is not evidence: verify its kernel compile definition and the expected trace records.

Existing CMake QEMU targets call timeout wrappers and build matching ISOs. Userspace wrapper validates normal debug-exit completion plus TAP; inspect `test.log` and `serial.log`, including the last test counter versus the actual plan. Kernel wrapper uses `kernel-test-serial.log`; require completed suites and zero recorded failures. Read current script exit semantics rather than stale historical “QEMU exit 0 is success” assumptions.

Do not pipe live build/QEMU commands through head/grep. If tee is used, preserve the real pipeline status. Keep full logs, configuration, HEAD, pass counts, timeout/panic state, raw register probe results and stack watermarks as review evidence. Inspect kernel.bin disassembly for switch/trampoline/FP tails and all new C-call alignment. Re-run broad suites after substantive changes, not repeatedly without new evidence.

## 10. Implementation context pack / affected surface

New proposed files:

- `kernel/src/process/switch.S`, `kernel/inc/process/switch.h`.
- `kernel/src/tests/unit/test_kernel_context.c` and a test-only NASM probe under the same tests directory.
- `userspace/tests/t_pipe_block.c`; focused guest register/FP tests may use separate `t_kctx_*` files with both guest registrations.

Primary edits:

- process.h/process.c: stack ownership, canonical frame, construction/rollback, fork/exec.
- scheduler.h/scheduler.c/scheduler_algorithm.c: one core, idle, state/accounting, start handoff, reap.
- interrupt.S/exception.S/user.S and interrupt.c/exception.c: entry/return ABI, EOI and IRQ depth.
- syscall.c/signal.c/page_fault.c: common exit, sigreturn result, signal stop/exit, diagnostics.
- fpu.c/fpu.h: eager user/kernel/nested-entry ownership and valid bootstrap images.
- wait.c/wait.h and irqflags.h: ownership, locking, prepare/finish, IF fix.
- pipe.c: transfer loops, waits, peer closure and wake symmetry.
- timer.c plus keyboard.c/mouse.c/ata.c IRQ handlers: remove duplicate PIC completion; retain legacy timer-wait ownership.
- kernel.c: explicit first-task handoff after boot tests.
- tests/runner.c and both guest registries; kernel/CMakeLists.txt only for narrowly needed test/profiling flags, not a hand-written source list.
- proc_running.c only as needed for valid snapshots/null-mm diagnostics; uaccess.h and maintainer documentation for changed assumptions.
- vfs.c only if bounded construction rollback changes are necessary; preserve fd counts and pipe endpoint counts.

Direct-dependent accounting:

| Changed contract | Consumers that must be migrated or explicitly retained |
|---|---|
| scheduler_run / frame restore | syscall handler, timer, GP fault, page-fault signal path, signal stop, successful exec, init handoff |
| thread.regs authority | fork/init/exec writers, scheduler snapshots, /proc readers; signal_regs remains separate |
| wait wake ownership | pipe wake iteration, wake_up_all, targeted signal wake, nanosleep timeout/cancel, waitpid, keyboard |
| FPU boundary ownership | syscall, timer, all IRQ/exception entry/exit, fork, initial init state, kernel FP tests, invalid-op/lazy-owner handlers |
| stack destruction | failed construction, waitpid reap, do_exit mm destruction, signal termination and /proc diagnostics |

State at plan publication: all implementation checkboxes are unchecked; no production edits or builds were performed during this revision. This plan is source-verified at the stated commit, with no graph/PDG completeness claim and no measured stack maximum. The runtime gates are deliberately executable work for the implementation stage.

## 11. Risks, bounded decisions and deferred work

Resolved choices: one stack per task, full user frame, one scheduler core, UP non-preemptive, scheduler-local idle, caller-owned new wait entries, explicit legacy ownership, eager FP preservation, 32 KiB initial page allocation, signals interrupt new pipe waits, no automatic restart.

Implementation gates that may change a parameter, not the architecture:

- Stack profiling may require a larger size or further local buffer reduction; do not shrink to 8 KiB without evidence.
- Verify exact NASM/C frame offsets and compiler call alignment in both builds before activation.
- Verify the newly allocated lowmem stack pages across pre-existing pgds, despite #271 being fixed.
- Match existing user EFLAGS/I/O privilege behavior while constructing init's full synthetic frame.
- If OOM injection exposes incomplete VFS construction accounting, fix only the acquisition/rollback contract needed here and add its test before declaring failure handling complete.
- Instrumentation must demonstrably exercise suspension; do not substitute timing guesses or a test that passes without blocking.

Deferred explicitly: general kernel threads/idle task object, true blocking waitpid and terminal reads, POSIX restart/SA_RESTART, full nested signal/FPU ABI redesign, PIPE_BUF/capacity redesign, guard-page virtual stack allocator, shared-mm threads and recoverable uaccess faults, unrelated scheduler algorithm exercises. Existing user-fault or signal defects discovered beyond the changed contract need separate evidence and tracking; they must not be silently attributed to this feature.

The absence of hardware stack guards remains a disclosed limitation. Debug canaries and watermarks detect only covered cases; they do not prevent arbitrary corruption. Kernel IRQ nesting beyond the supported UP entry model is not enabled by this change.

## 12. Definition of done

- [ ] One allocated kernel stack per task, no parent alias, correct rollback and reap-only release.
- [ ] Actual frame/continuation restore for fork, syscall, IRQ, exec and sigreturn; no cross-task `*f` replacement remains.
- [ ] EOI precedes stack switching; CPU-bound tasks continue to receive timer-driven userspace scheduling.
- [ ] Kernel mode is not preempted by IRQs; voluntary scheduling and idle work with correct IF state.
- [ ] User, kernel and nested-entry FP contexts have distinct ownership and passing preservation tests.
- [ ] New wait entries are never freed by the waker; legacy timer/keyboard/waitpid users remain compatible.
- [ ] Blocking read/write pass single-invocation tests, including competing tasks, partial progress, EOF, broken pipe and signals; O_NONBLOCK still reports EAGAIN correctly.
- [ ] Full Debug/Release guest and kernel runs complete with logs and explained baseline differences; test hooks prove the intended scheduling windows.
- [ ] Measured stack headroom and lack of hardware guards are recorded; diagnostics identify the active task stack.
- [ ] Repository fd/mm invariants are preserved; no undocumented expansion into threading, preemption or unrelated syscall semantics.
- [ ] Documentation and final diff review agree with the implemented design. Issue closure is justified by this evidence, not just successful compilation.
