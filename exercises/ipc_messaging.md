# Exercise: Inter-Process Communication via System Call

## Objective

Create a pair of system calls to enable one process to send a message to another
and receive a message, simulating basic inter-process communication (IPC).

---

## System Calls Specification

### 1. `sys_send_message`

- **Definition**: `int sys_send_message(pid_t to, const char *message, size_t length)`
- **Purpose**: Sends a message to a specified process by its PID.
- **Parameters**:
  - `to`: The target process ID.
  - `message`: Pointer to the message string.
  - `length`: The length of the message.
- **Return Values**:
  - `0` on success.
  - `-1` if the target process does not exist or the message buffer is full.

### 2. `sys_receive_message`

- **Definition**: `int sys_receive_message(char *buffer, size_t size)`
- **Purpose**: Receives a message sent to the calling process.
- **Parameters**:
  - `buffer`: Pointer to a buffer where the message will be stored.
  - `size`: Size of the buffer.
- **Return Values**:
  - Number of bytes received on success.
  - `-1` if there are no messages.

---

## Implementation Steps

### Kernel-Side Implementation

1. **Message Queue Design**:
   - Each process in the kernel gets its own message queue (e.g., a fixed-size
     array or a dynamic linked list).
   - Limit the queue size to prevent memory overuse.

2. **`sys_send_message`**:
   - Verify the target PID exists.
   - Copy the message from user space to kernel space using `copy_from_user`.
   - Add the message to the target process's queue.
   - Handle cases like full queues or invalid PIDs.

3. **`sys_receive_message`**:
   - Check if the calling process has messages in its queue.
   - Copy the oldest message from kernel space to user space using `copy_to_user`.
   - Remove the message from the queue.
   - Handle cases like empty queues or insufficient buffer size.

---

### User-Space Implementation

1. **`send_message`**:
   - Wrap the `sys_send_message` system call.
   - Validate inputs and provide an easy-to-use interface for sending messages.

2. **`receive_message`**:
   - Wrap the `sys_receive_message` system call.
   - Allow users to check the return value for message length or errors.

---

## Exercise Task Instructions

1. **Setup the Kernel**:
   - Define and register the new system calls with unique syscall numbers.
   - Implement the message queue data structure in the kernel.

2. **Write the User-Space Wrapper Functions**:
   - Implement `send_message` and `receive_message` in a user library.

3. **Create a Test Application**:
   - Write two programs:
     - **Sender:** Sends a message to another process.
     - **Receiver:** Continuously receives and prints messages.

4. **Test and Debug**:
   - Test valid and invalid cases, such as:
     - Sending to non-existent PIDs.
     - Receiving with an undersized buffer.
     - Exceeding the message queue size.

---

## Extensions for Advanced Students

- **Priority Messages**
  - Add a priority field to messages and modify the queue to handle
    priority-based delivery.
- **Broadcast Messages**
  - Allow sending messages to all processes or a group of processes.
- **Asynchronous Notifications**
  - Implement a signal or callback mechanism to notify a process when a new
    message arrives.
- **Persistent Queues**
  - Store messages on disk for processes that terminate and restart.

---

## Sample Code Outline

### Files to Add and Modify

**Add**:

- `libc/inc/coms.h`: Place the user-side functions here.
- `libc/src/coms.c`: Implement the user-space system call interface.
- `mentos/inc/ipc/coms.h`: Defines the functions related to the messaging system.
- `mentos/src/ipc/coms.c`: Implement the system call in the kernel.

**Modify**:

- `libc/inc/system/syscall_types.h`: Choose two unique numbers for the system calls.
- `mentos/inc/system/syscall.h`: Add the definitions for the kernel-side functions.
- `mentos/src/system/syscall.c`: Register the kernel-side functions in the system call table.
- **Kernel Task Management Code**:
  - Call `register_message_queue` when a process is created (e.g., in `__alloc_task`).
  - Call `unregister_message_queue` when a process is cleaned up (e.g., in `sys_waitpid` or similar cleanup logic).

---

### Kernel-Side Code (`mentos/inc/ipc/coms.h`)

You define the messaging system's functions here:

```c
/// @brief Register the message queue for a process.
/// @param pid Process ID.
void register_message_queue(pid_t pid);

/// @brief Unregister the message queue for a process.
/// @param pid Process ID.
void unregister_message_queue(pid_t pid);
```

Integration Notes:

- In __alloc_task, call register_message_queue to initialize the message queue
  for a new process.
- In sys_waitpid or the kernel's process cleanup routine, call
  unregister_message_queue to free the resources associated with the terminated
  process.

### Kernel-Side Code (`mentos/src/ipc/coms.c`)

You can place your implementation in `mentos/src/ipc/coms.c`, but remember to
add this file to the kernel build system in `mentos/CMakeLists.txt`.

```c

/// @brief Maximum number of messages per process.
#define MAX_MESSAGES 10

/// @brief Define the message structure.
typedef struct message {
    char content[256]; ///< The content of the message (up to 256 characters).
    size_t length;     ///< The actual length of the message.
} message_t;

/// @brief Define the per-process message queue.
typedef struct message_queue {
    list_head list;                        ///< Reference inside the list of message queues.
    struct message messages[MAX_MESSAGES]; ///< The queue of messages.
    int head;                              ///< The index of the next message to be dequeued.
    int tail;                              ///< The index where the next message will be enqueued.
} message_queue_t;

/// @brief List of all current active message queues.
list_head message_queue_list;

void register_message_queue(pid_t pid) { 
    // Implementation goes here
}

void unregister_message_queue(pid_t pid) { 
    // Implementation goes here
}

long sys_send_message(pid_t pid, const char *message, size_t length) { 
    // Implementation goes here
}

long sys_receive_message(char *buffer, size_t size) { 
    // Implementation goes here
}
```

You can take inspiration from the other IPC codes to see how to implement
`register_message_queue` and `unregister_message_queue`.

#### User-Space (`libc/coms.h`)

```c
/// @brief Sends a message to a specific process.
/// @param to Target process ID.
/// @param message The message to send.
/// @return 0 on success, -1 on failure.
int send_message(pid_t to, const char *message);

/// @brief Receives a message sent to the calling process.
/// @param buffer Buffer to store the message.
/// @param size Size of the buffer.
/// @return Number of bytes received, -1 on failure.
int receive_message(char *buffer, size_t size);
```

#### Test Application (`programs/tests/test_coms.c`)

Again, remember to add the source to the list of test codes in
`programs/tests/CMakeLists.txt`.

```c
int main() {
    if (fork() == 0) {
        // Child process (Receiver)
        char buffer[256];
        while (1) {
            int bytes = receive_message(buffer, sizeof(buffer));
            if (bytes > 0) {
                printf("Received: %s\n", buffer);
            }
        }
    } else {
        // Parent process (Sender)
        send_message(getpid() + 1, "Hello from parent!");
    }
    return 0;
}
```

## Notes

This exercise combines practical programming with OS concepts, making it ideal
for understanding kernel-user interactions and the intricacies of system calls.

### Additional Notes

1. **Integration Points in the Kernel**:
   - Ensure `register_message_queue` is called during the process creation phase
     (e.g., in `__alloc_task` or equivalent).
   - Call `unregister_message_queue` during process cleanup (e.g., in
     `sys_waitpid` or the kernel's exit logic). This prevents memory leaks and
     ensures that message queues are properly removed.

2. **Error Handling**:
   - Validate that a message queue exists for a target process
     (`sys_send_message`) and the current process (`sys_receive_message`).
   - Check buffer boundaries during `copy_to_user` and `copy_from_user` to avoid
     kernel crashes or data corruption.
   - Return appropriate error codes (e.g., `-EINVAL`, `-ESRCH`, `-ENOSPC`) to
     help users debug issues.

3. **Concurrency**:
   - Protect shared data structures like `message_queue_list` or individual
     queues using appropriate synchronization primitives (e.g., spinlocks,
     mutexes) to avoid race conditions.
   - Ensure thread safety in `sys_send_message` and `sys_receive_message`.

4. **Message Truncation**:
   - Messages longer than `256` bytes are truncated. Inform users via
     documentation or return a specific error code (e.g., `-EMSGSIZE`) if
     truncation occurs.

5. **Testing Recommendations**:
   - Test with edge cases:
     - Attempt to send messages to nonexistent or terminated processes.
     - Test overflow conditions by filling a queue with `MAX_MESSAGES`.
     - Use undersized buffers in `sys_receive_message`.
   - Test multi-process scenarios where multiple senders and receivers interact
     simultaneously.

6. **Kernel Debugging**:
   - Use kernel logs (`pr_debug`, `pr_info`, etc.) to output diagnostics during
     system call execution, process creation, and cleanup.

7. **User-Space Library**:
   - Provide a clear and well-documented API in `libc/coms.h` to simplify usage
     of `send_message` and `receive_message`.
   - Include sample usage and edge-case handling in the test application.

8. **CMake Integration**:
   - Ensure all new source files (`libc/src/coms.c`, `mentos/src/ipc/coms.c`)
     are added to the appropriate CMake configuration files:
     - Kernel (`mentos/CMakeLists.txt`).
     - Test programs (`programs/tests/CMakeLists.txt`).

9. **Optional Extensions**:
    - Implement **priority messages** where higher-priority messages are
      delivered first. You need to a priority field to the messages.
    - Add **time-to-live (TTL)** functionality for messages, automatically
      discarding expired ones. You need to add a `will_expire_on` field, that
      uses kernel ticks in the future to identify an old message.
    - Include **group messaging** for broadcasting messages to multiple
      processes.
