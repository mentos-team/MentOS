# Mentoring Operating System (MentOS) - Exercise 1: Deadlock Prevention

**Created by:**

- Mirco De Marchi
- Enrico Fraccaroli
- <enrico.fraccaroli@gmail.com>

---

## Table of Contents

1. [Setup](#setup)
2. [Deadlock: Theoretical Aspects](#deadlock-theoretical-aspects)
   - [Definitions](#definitions)
   - [Banker's Algorithm](#bankers-algorithm)
3. [MentOS: Deadlock Prevention](#mentos-deadlock-prevention)
   - [How to Implement](#how-to-implement)
   - [Library arr_math](#library-arr_math)
   - [Exercise Tasks](#exercise-tasks)

---

## Setup

### Building the Exercise

1. Save your work (e.g., any uncommitted changes)

   ```bash
   cd <mentos-root-dir>
   git add .
   git commit -m "WIP: save changes before deadlock exercise"
   ```

2. Build MentOS with deadlock prevention enabled:

   ```bash
   mkdir -p build && cd build
   cmake -DENABLE_DEADLOCK_PREVENTION=ON ..
   make -j$(nproc)
   ```

3. Run the system:

   ```bash
   make qemu
   ```

---

## Deadlock: Theoretical Aspects

### Definitions

#### Deadlock

**State of a concurrent system with shared resources between tasks, in which at least a single task is waiting for a resource acquisition that can be released by another task without resolution.**

To avoid deadlock, you must prevent at least one of the following conditions from holding:

- **Mutual Exclusion** - Only one task can use a resource at a time
- **Hold and Wait** - A task holding a resource can request other resources
- **No Preemption** - Resources cannot be forcibly taken away
- **Circular Waiting** - A cycle of tasks waiting for each other's resources

#### Safe State

**The system state is safe if you can find a sequence of resource allocations that satisfy all tasks' resource requirements; otherwise, it is unsafe.**

**Note:** You need to know tasks' resource requirements in advance—this is not trivial.

**Methodologies using the concept of unsafe state:**

- **Dynamic Prevention**: Check if each allocation request leads to an unsafe state (e.g., Banker's Algorithm)
- **Detection**: Only detect when deadlock occurs

### Banker's Algorithm

**Main Idea:** "I will satisfy your request only if I am sure to satisfy the requests that others can ask."

**Note:** This approach is conservative because it considers the upper bound of resource requests, which can lead to task starvation.

**Alternative Methodologies:**

- **Static Prevention**: Design constraints to avoid deadlock conditions
- **Detect and Recovery**: Rollback or restart the system
- **Not Handled**: Programmers must write correct code (e.g., Linux)

#### Banker's Algorithm: Notations

- **n**: Current number of tasks in the system
- **m**: Current number of resource types in the system
- **req_task**: Process that performs the resource request
- **req_vec[m]**: Resource instances requested by req_task
- **available[m]**: Number of resource instances available for each resource type
- **max[n][m]**: Maximum number of resource instances that each task may require
- **alloc[n][m]**: Current resource instances allocation for each task
- **need[n][m]**: Current resource instances need for each task
  - **Formula:** `need[i][j] = max[i][j] - alloc[i][j]`

#### Algorithm 1: Resource Request

```pseudocode
Require: req_task, req_vec[m], available[m], max[n][m], alloc[n][m], need[n][m]

1: if req_vec > need[req_task] then
2:     error()
3: end if
4: if req_vec > available then
5:     wait()
6: end if
7: available = available - req_vec
8: alloc[req_task] = alloc[req_task] + req_vec
9: need[req_task] = need[req_task] - req_vec
10: if !safe_state() then
11:     available = available + req_vec
12:     alloc[req_task] = alloc[req_task] - req_vec
13:     need[req_task] = need[req_task] + req_vec
14:     wait()
15: end if
```

#### Algorithm 2: Check if State is Safe

```pseudocode
Require: available[m], max[n][m], alloc[n][m], need[n][m]

1: work[m] = available
2: finish[n] = (0, ..., 0)
3: while finish[] != (1, ..., 1) do
4:     for i = 0 to n do
5:         if !finish[i] and work >= need[i] then
6:             break
7:         end if
8:     end for
9:     if i == n then
10:         return false  // UNSAFE
11:     else
12:         work = work + alloc[i]
13:         finish[i] = 1
14:     end if
15: end while
16: return true  // SAFE
```

---

## MentOS: Deadlock Prevention

### How to Implement

Implementing deadlock prevention in MentOS is challenging because we need to know task resource requirements in advance. To collect the necessary data:

**Need to maintain:**

- **available**: A list of created resources
- **max**: For each task, the resources it is interested in
- **alloc**: Which resources are assigned to each process
- **need**: A library to manage arrays (for both data and the algorithm)

**Assumptions Made:**

- Each semaphore created belongs to an existing resource
- Each resource can be used by the process that created it and by its child processes

**What Has Been Implemented:**

1. Definition of `resource_t` with task reference that owns it
2. Creation of global created resources list
3. List of resources that tasks are interested in, stored in `task_struct`
4. Copy of this list in child `task_struct` during `fork()` syscall
5. Resource creation during semaphore creation in kernel-side syscall
6. Implementation of `arr_math` library

#### Resource Definition and task_struct Improvements

```c
typedef struct resource {
    /// Resource index. The resources indexes must be continuous: 0, 1, ... M.
    size_t rid;
    /// List head for resources list.
    list_head resources_list;
    /// Number of instances of this resource. For now, always 1.
    size_t n_instances;
    /// If the resource has been assigned, it points to the task assigned,
    /// otherwise NULL.
    task_struct *assigned_task;
    /// Number of instances assigned to assigned task.
    size_t assigned_instances;
} resource_t;

typedef struct task_struct {
    ...
    /// Array of resource pointers that task needs.
    struct resource *resources[TASK_RESOURCE_MAX_AMOUNT];
    ...
} task_struct;
```

### Library arr_math

The implementation of Banker's Algorithm requires managing matrices and arrays. You can find the `arr_math` definition in `exercises/deadlock-prevention/lib/inc/arr_math.h`.

**Summary of Array Operations:**

- `uint32_t *arr_all(uint32_t *dst, uint32_t value, size_t length)`
  - Initialize the destination array with a value

- `uint32_t *arr_sub(uint32_t *left, const uint32_t *right, size_t length)`
  - Array element-wise subtraction, result saved in left pointer

- `uint32_t *arr_add(uint32_t *left, const uint32_t *right, size_t length)`
  - Array element-wise addition, result saved in left pointer

**Comparison Operations:**

- `bool_t arr_g_any(const uint32_t *left, const uint32_t *right, size_t length)`
  - Check that at least one array element is greater than the respective other
  - Example: `[1, 1, 6] > [1, 2, 3]` = true

- `bool_t arr_g(const uint32_t *left, const uint32_t *right, size_t length)`
  - Check that all array elements are greater than the respective other
  - Example: `[2, 3, 4] > [1, 2, 3]` = true

- `arr_ge_any`, `arr_ge`: Greater or equal (at least one, or all)
- `arr_l_any`, `arr_l`: Less than (at least one, or all)
- `arr_le_any`, `arr_le`: Less or equal (at least one, or all)
- `arr_e`, `arr_ne`: Equal or not equal (all elements)

### Exercise Tasks

#### Task 1: Understand the Foundation

1. Study the `arr_math` library in `exercises/deadlock-prevention/lib/inc/arr_math.h`
2. Understand the data structures in `exercises/deadlock-prevention/kernel/inc/resource.h`

#### Task 2: Implement Array Mathematics

1. Complete `exercises/deadlock-prevention/lib/src/arr_math.c`
2. Implement all comparison and arithmetic operations

#### Task 3: Implement Banker's Algorithm

1. Study the theoretical algorithms provided above
2. Implement Algorithm 1 (Resource Request) in kernel-side code
3. Implement Algorithm 2 (Safe State Check) in kernel-side code
4. Reference: `exercises/deadlock-prevention/kernel/src/deadlock_prevention.c`

#### Task 4: Test Your Implementation

1. Build the project with deadlock prevention enabled:

   ```bash
   cmake -DENABLE_DEADLOCK_PREVENTION=ON ..
   make
   ```

2. Run QEMU and observe the debug console for deadlock prevention simulation:

   ```bash
   make qemu
   ```

3. Test the shell command:

   ```bash
   deadlock [-i <iterations>]
   ```

   - The optional parameter `-i <iterations>` specifies the number of iterations for created tasks
   - Default: 1 iteration

---

## Debugging

- Enable debug logging to see resource allocation and deadlock prevention decisions
- Check kernel debug output in the QEMU console
- Use GDB to step through the Banker's Algorithm implementation

## Resources

- See `INTEGRATION_GUIDE.md` for detailed file organization and dependencies
- See `ARCHITECTURE.md` for a complete system overview

---

**Good luck with the exercise!**
