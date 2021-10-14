# Signal

## Legenda

The **first column** of the table represent the implementation status,
specifically:

1. Implemented
2. Tested

If the column is empty it means that it's not implemented yet.

## Implementation Status

```
 S | Number | Signal    | Standard | Action | Comment
---+--------+-----------+----------+--------+------------------------------
   |    1   | SIGHUP    | P1990    | Term   | Hangup detected on controlling terminal or death of controlling process
   |    2   | SIGINT    | P1990    | Term   | Interrupt from keyboard
   |    3   | SIGQUIT   | P1990    | Core   | Quit from keyboard
   |    4   | SIGILL    | P1990    | Core   | Illegal Instruction
   |    5   | SIGTRAP   | P2001    | Core   | Trace/breakpoint trap
 2 |    6   | SIGABRT   | P1990    | Core   | Abort signal from abort(3)
   |    7   | SIGEMT    |   -      | Term   | Emulator trap
 2 |    8   | SIGFPE    | P1990    | Core   | Floating-point exception
 2 |    9   | SIGKILL   | P1990    | Term   | Kill signal
   |   10   | SIGBUS    | P2001    | Core   | Bus error (bad memory access)
   |   11   | SIGSEGV   | P1990    | Core   | Invalid memory reference
   |   12   | SIGSYS    | P2001    | Core   | Bad system call (SVr4); see also seccomp(2)
   |   13   | SIGPIPE   | P1990    | Term   | Broken pipe: write to pipe with no readers; see pipe(7)
 2 |   14   | SIGALRM   | P1990    | Term   | Timer signal from alarm(2)
 2 |   15   | SIGTERM   | P1990    | Term   | Termination signal
 2 |   16   | SIGUSR1   | P1990    | Term   | User-defined signal 1
 2 |   17   | SIGUSR2   | P1990    | Term   | User-defined signal 2
 1 |   18   | SIGCHLD   | P1990    | Ign    | Child stopped or terminated
   |   19   | SIGPWR    |   -      | Term   | Power failure (System V)
   |   20   | SIGWINCH  |   -      | Ign    | Window resize signal (4.3BSD, Sun)
   |   21   | SIGURG    | P2001    | Ign    | Urgent condition on socket (4.2BSD)
   |   22   | SIGPOLL   | P2001    | Term   | Pollable event (Sys V); synonym for SIGIO
 2 |   23   | SIGSTOP   | P1990    | Stop   | Stop process
   |   24   | SIGTSTP   | P1990    | Stop   | Stop typed at terminal
 2 |   25   | SIGCONT   | P1990    | Cont   | Continue if stopped
   |   26   | SIGTTIN   | P1990    | Stop   | Terminal input for background process
   |   27   | SIGTTOU   | P1990    | Stop   | Terminal output for background process
   |   28   | SIGVTALRM | P2001    | Term   | Virtual alarm clock (4.2BSD)
 2 |   29   | SIGPROF   | P2001    | Term   | Profiling timer expired
   |   30   | SIGXCPU   | P2001    | Core   | CPU time limit exceeded (4.2BSD); see setrlimit(2)
   |   31   | SIGXFSZ   | P2001    | Core   | File size limit exceeded (4.2BSD); see setrlimit(2)
```