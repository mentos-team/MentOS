# Deadlock Prevention Exercise - Integration Guide

## Quick Start

```bash
cd <mentos-root-dir>
mkdir -p build && cd build
cmake -DENABLE_DEADLOCK_PREVENTION=ON ..
make -j$(nproc)
make qemu
```

---

## File Organization

The deadlock prevention exercise is organized into three layers, mirroring the MentOS architecture:

```bash
exercises/deadlock-prevention/
├── EXERCISE.md              (Student exercise document)
├── INTEGRATION_GUIDE.md     (This file)
├── kernel/
│   ├── inc/                 (Kernel headers)
│   │   ├── deadlock_prevention.h      (Banker's algorithm interface)
│   │   ├── deadlock_simulation.h      (Kernel-side deterministic simulation)
│   │   ├── resource.h                 (Resource management structures)
│   │   └── smart_sem_kernel.h         (Kernel semaphore utilities)
│   ├── src/                 (Kernel implementation)
│   │   ├── deadlock_prevention.c      (TODO: Implement Banker's algorithm)
│   │   ├── deadlock_simulation.c      (Kernel-side deterministic simulation)
│   │   ├── resource.c                 (Resource allocation/tracking)
│   │   └── smart_sem_kernel.c         (Kernel-side semaphore code)
│   └── CMakeLists.txt       (Kernel layer cmake configuration)
├── lib/
│   ├── inc/                 (Library headers)
│   │   ├── arr_math.h                 (Array mathematics for algorithm)
│   │   └── smart_sem_user.h           (User-space semaphore utilities)
│   ├── src/                 (Library implementation)
│   │   ├── arr_math.c                 (TODO: Implement array operations)
│   │   └── smart_sem_user.c           (User-space semaphore code)
│   └── CMakeLists.txt       (Library layer cmake configuration)
└── userspace/
   ├── programs/            (Shell commands and utilities)
   │   ├── CMakeLists.txt   (Build configuration for programs)
   │   └── deadlock.c       (TODO: Shell command deadlock [-i <iter>])
   └── CMakeLists.txt       (Userspace layer cmake configuration)
```

---

## Layer Dependencies

### Layer 1: Library (libc) - Foundation

**Files to implement:**

- `lib/src/arr_math.c`

**Purpose:** Implement array operations needed by the Banker's Algorithm

**Key Functions:**

```c
uint32_t *arr_all(uint32_t *dst, uint32_t value, size_t length);
uint32_t *arr_sub(uint32_t *left, const uint32_t *right, size_t length);
uint32_t *arr_add(uint32_t *left, const uint32_t *right, size_t length);
bool_t arr_g(const uint32_t *left, const uint32_t *right, size_t length);
bool_t arr_ge(const uint32_t *left, const uint32_t *right, size_t length);
// ... and more comparison operations
```

**Used by:** Kernel's `deadlock_prevention.c`

---

### Layer 2: Kernel - Core Algorithm

**Files to implement/understand:**

1. **Resource Management** (`kernel/src/resource.c`, `kernel/inc/resource.h`)
   - Manages system resources
   - Tracks resource allocation to tasks
   - Maintains global resource list

2. **Deadlock Prevention** (`kernel/src/deadlock_prevention.c`, `kernel/inc/deadlock_prevention.h`)
   - **TODO:** Implement Algorithm 1 (Resource Request)
   - **TODO:** Implement Algorithm 2 (Safe State Check)
   - Uses `arr_math` library for array operations
   - References `resource_t` structures

3. **Kernel Semaphore Utilities** (`kernel/src/smart_sem_kernel.c`, `kernel/inc/smart_sem_kernel.h`)
   - Kernel-side support for user-space semaphores
   - Integrates with deadlock prevention

4. **Deterministic Simulation** (`kernel/src/deadlock_simulation.c`, `kernel/inc/deadlock_simulation.h`)
   - Kernel-side deterministic test of the algorithm
   - Prints matrices and decision flow to the debug console

**Key Data Structures:**

```c
typedef struct resource {
   size_t rid;
   list_head_t resources_list;
   list_head_t task_refs;
   size_t n_instances;
   task_struct *assigned_task;
   size_t assigned_instances;
} resource_t;

typedef struct task_resource_ref {
   list_head_t list;
   task_struct *task;
   resource_t *resource;
   bool_t in_use;
} task_resource_ref_t;
```

**Algorithm Flow:**

1. User requests resource (semaphore)
2. Kernel checks current allocation against maximum possible needs
3. Uses Banker's Algorithm to determine if request is safe
4. If safe: allocate and continue
5. If unsafe: deny and let task wait

---

### Layer 3: Userspace - Testing & Diagnosis

**Files:**

1. **Shell Command** (`userspace/programs/deadlock.c`)
   - **TODO:** Interactive shell command: `deadlock [-i <iterations>]`
   - Creates multiple processes that contend for resources
   - Tests deadlock prevention under real conditions

---

## Building the Exercise

### Step 1: Clone/Verify Files

All files are in `exercises/deadlock-prevention/`

```bash
ls -la exercises/deadlock-prevention/
```

### Step 2: Configure CMake

```bash
cd build
cmake -DENABLE_DEADLOCK_PREVENTION=ON ..
```

This enables:

- `ENABLE_DEADLOCK_PREVENTION` preprocessor define in kernel and libc
- Conditional compilation of exercise code
- Build of the kernel-side deterministic simulation

### Step 3: Build

```bash
make -j$(nproc)
```

Watch for compilation errors in:

- `arr_math.c` (missing implementations)
- `deadlock_prevention.c` (missing algorithm implementations)

### Step 4: Run

```bash
make qemu
```

In QEMU:

- Boot normally
- Watch kernel debug output from the deterministic simulation
- Use `deadlock -i 2` to exercise the runtime path

---

## Implementation Checklist

### Student Implementation Tasks

- [ ] **Phase 1: Array Mathematics**
  - [ ] Implement `arr_all()` - array initialization
  - [ ] Implement `arr_sub()` - element-wise subtraction
  - [ ] Implement `arr_add()` - element-wise addition
  - [ ] Implement `arr_g()` - all-element greater-than comparison
  - [ ] Implement `arr_ge()` - all-element greater-or-equal comparison
  - [ ] Implement `arr_l()`, `arr_le()`, `arr_ne()`, `arr_e()`
  - [ ] Verify with simple test cases

- [ ] **Phase 2: Banker's Algorithm - Resource Request**
  - [ ] Study Algorithm 1 in EXERCISE.md
  - [ ] Implement request validation (check against need)
  - [ ] Implement availability check
  - [ ] Implement tentative allocation
  - [ ] Integrate Safe State Check (see Phase 3)

- [ ] **Phase 3: Banker's Algorithm - Safe State Check**
  - [ ] Study Algorithm 2 in EXERCISE.md
  - [ ] Initialize work array and finish array
  - [ ] Implement main loop
  - [ ] Implement finish checking and task selection
  - [ ] Return true if safe, false if unsafe

- [ ] **Phase 4: Testing**
   - [ ] Compile without errors
   - [ ] Observe kernel debug output from the deterministic simulation
   - [ ] Run with multiple iterations: `deadlock -i 3`
   - [ ] Verify safe sequence detection

---

## Common Issues & Solutions

### Issue: Compilation errors in arr_math.c

**Cause:** Array operation implementations are missing

**Solution:**

1. Review the function signatures in `lib/inc/arr_math.h`
2. Implement each operation following the specifications
3. Remember to handle edge cases (zero-length arrays, NULL pointers)

### Issue: Kernel deadlock/hang during boot

**Cause:** Infinite loop in safe state checking or resource allocation

**Solution:**

1. Add debug logging to `deadlock_prevention.c`
2. Check boundary conditions in algorithm loops
3. Verify resource initialization

### Issue: Shell command `deadlock` not found

**Cause:** Program not compiled or not in filesystem

**Solution:**

1. Check if `userspace/programs/deadlock.c` exists
2. Verify CMake configuration included it
3. Run `make qemu` to regenerate filesystem

---

## Debugging Tips

### Enable Kernel Debug Output

In kernel code, use:

```c
printk(LOG_DEBUG, "Deadlock check for resource %d\n", rid);
```

Monitor in QEMU console or dmesg.

### Use GDB

```bash
make qemu-gdb
# In another terminal:
gdb
(gdb) target remote localhost:1234
(gdb) symbol-file build/kernel/kernel
(gdb) break deadlock_check
(gdb) continue
```

### Unit Test in Isolation

Before integrating, test `arr_math` functions independently:

```c
uint32_t test_arr[3];
arr_all(test_arr, 5, 3);
// test_arr should be [5, 5, 5]
```

---

## References

- See `EXERCISE.md` for the full exercise description
- Banker's Algorithm (Dijkstra, 1965) - foundational CS concept
- MentOS Resource Management - kernel headers
- Array Mathematics Library - `lib/inc/arr_math.h`

---

**Questions?** Refer to the exercise document and kernel source code comments.
