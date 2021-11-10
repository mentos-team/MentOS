///                MentOS, The Mentoring Operating system project
/// @file signal.h
/// @brief Signals definition.
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "sys/types.h"

/// @brief List of signals.
typedef enum {
    SIGHUP    = 1,  ///< Hang up detected on controlling terminal or death of controlling process.
    SIGINT    = 2,  ///< Issued if the user sends an interrupt signal (Ctrl + C).
    SIGQUIT   = 3,  ///< Issued if the user sends a quit signal (Ctrl + D).
    SIGILL    = 4,  ///< Illegal Instruction.
    SIGTRAP   = 5,  ///< Trace/breakpoint trap.
    SIGABRT   = 6,  ///< Abort signal from abort().
    SIGEMT    = 7,  ///< Emulator trap.
    SIGFPE    = 8,  ///< Floating-point arithmetic exception.
    SIGKILL   = 9,  ///< If a process gets this signal it must quit immediately and will not perform any clean-up operations.
    SIGBUS    = 10, ///< Bus error (bad memory access).
    SIGSEGV   = 11, ///< Invalid memory reference.
    SIGSYS    = 12, ///< Bad system call (SVr4).
    SIGPIPE   = 13, ///< Broken pipe: write to pipe with no readers.
    SIGALRM   = 14, ///< Alarm clock signal (used for timers).
    SIGTERM   = 15, ///< Software termination signal (sent by kill by default).
    SIGUSR1   = 16, ///< User-defined signal 1.
    SIGUSR2   = 17, ///< User-defined signal 2.
    SIGCHLD   = 18, ///< Child stopped or terminated.
    SIGPWR    = 19, ///< Power failure.
    SIGWINCH  = 20, ///< Window resize signal.
    SIGURG    = 21, ///< Urgent condition on socket.
    SIGPOLL   = 22, ///< Pollable event.
    SIGSTOP   = 23, ///< Stop process.
    SIGTSTP   = 24, ///< Stop typed at terminal.
    SIGCONT   = 25, ///< Continue if stopped.
    SIGTTIN   = 26, ///< Terminal input for background process.
    SIGTTOU   = 27, ///< Terminal output for background process.
    SIGVTALRM = 28, ///< Virtual alarm clock.
    SIGPROF   = 29, ///< Profiling timer expired.
    SIGXCPU   = 30, ///< CPU time limit exceeded.
    SIGXFSZ   = 31, ///< File size limit exceeded.
    NSIG
} signal_type_t;

/// @brief Codes that indentify the sender of a signal.
typedef enum {
    SI_NOINFO, ///< Unable to determine complete signal information.

    // Signal         : -
    // Enabled fields : si_pid, si_uid
    SI_USER, ///< Signal sent by kill(), pthread_kill(), raise(), abort() or alarm().

    // Signal         : -
    // Enabled fields : -
    SI_KERNEL, ///< Generic kernel function

    // Signal         : -
    // Enabled fields : si_pid, si_uid, si_value
    SI_QUEUE,   ///< Signal was sent by sigqueue().
    SI_TIMER,   ///< Signal was generated by expiration of a timer set by timer_settimer().
    SI_ASYNCIO, ///< Signal was generated by completion of an asynchronous I/O request.
    SI_MESGQ,   ///< Signal was generated by arrival of a message on an empty message queue.

    // Signal         : SIGILL
    // Enabled fields : si_addr (address of failing instruction)
    ILL_ILLOPC, ///< Illegal opcode.
    ILL_ILLOPN, ///< Illegal operand.
    ILL_ILLADR, ///< Illegal addressing mode.
    ILL_ILLTRP, ///< Illegal trap.
    ILL_PRVOPC, ///< Privileged opcode.
    ILL_PRVREG, ///< Privileged register.
    ILL_COPROC, ///< Coprocessor error.
    ILL_BADSTK, ///< Internal stack error.

    // Signal         : SIGFPE
    // Enabled fields : si_addr (address of failing instruction)
    FPE_INTDIV, ///< Integer divide-by-zero.
    FPE_INTOVF, ///< Integer overflow.
    FPE_FLTDIV, ///< Floating point divide-by-zero.
    FPE_FLTOVF, ///< Floating point overflow.
    FPE_FLTUND, ///< Floating point underflow.
    FPE_FLTRES, ///< Floating point inexact result.
    FPE_FLTINV, ///< Invalid floating point operation.
    FPE_FLTSUB, ///< Subscript out of range.

    // Signal         : SIGSEGV
    // Enabled fields : si_addr (address of faulting memory reference)
    SEGV_MAPERR, ///< Address not mapped.
    SEGV_ACCERR, ///< Invalid permissions.

    // Signal         : SIGBUS
    // Enabled fields : si_addr (address of faulting memory reference)
    BUS_ADRALN, ///< Invalid address alignment.
    BUS_ADRERR, ///< Non-existent physical address.
    BUS_OBJERR, ///< Object-specific hardware error.

    // Signal         : SIGTRAP
    // Enabled fields : -
    TRAP_BRKPT, ///< Process breakpoint.
    TRAP_TRACE, ///< Process trace trap.

    // Signal         : SIGCHLD
    // Enabled fields : si_pid (child process ID)
    //                  si_uid (real user ID of process that sent the signal)
    //                  si_status (exit value or signal)
    CLD_EXITED,    ///< Child has exited.
    CLD_KILLED,    ///< Child has terminated abnormally and did not create a core file.
    CLD_DUMPED,    ///< Child has terminated abnormally and created a core file.
    CLD_TRAPPED,   ///< Traced child has trapped.
    CLD_STOPPED,   ///< Child has stopped.
    CLD_CONTINUED, ///< Stopped child has continued.

    // Signal         : SIGIO/SIGPOLL
    // Enabled fields : si_band
    POLL_IN,  ///< Data input available.
    POLL_OUT, ///< Output buffers available.
    POLL_MSG, ///< Input message available.
    POLL_ERR, ///< I/O error.
    POLL_PRI, ///< High priority input available.
    POLL_HUP, ///< Device disconnected.
} signal_sender_code_t;

/// @brief Defines what to do with the provided signal mask.
typedef enum {
    /// @brief The set of blocked signals is the union of the current set
    ///        and the set argument.
    SIG_BLOCK,
    /// @brief The signals in set are removed from the current set of
    ///        blocked signals. It is permissible to attempt to unblock
    ///        a signal which is not blocked.
    SIG_UNBLOCK,
    /// @brief The set of blocked signals is set to the argument set.
    SIG_SETMASK
} sigmask_how_t;

/// @defgroup SigactionFlags Flags associated with a sigaction.
/// @{

#define SA_NOCLDSTOP 0x00000001U ///< Turn off SIGCHLD when children stop.
#define SA_NOCLDWAIT 0x00000002U ///< Flag on SIGCHLD to inhibit zombies.
#define SA_SIGINFO   0x00000004U ///< sa_sigaction specifies the signal-handling function for signum.
#define SA_ONSTACK   0x08000000U ///< Indicates that a registered stack_t will be used.
#define SA_RESTART   0x10000000U ///< Flag to get restarting signals (which were the default long ago)
#define SA_NODEFER   0x40000000U ///< Prevents the current signal from being masked in the handler.
#define SA_RESETHAND 0x80000000U ///< Clears the handler when the signal is delivered.

/// @}

/// Type of a signal handler.
typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)  ///< Default signal handling.
#define SIG_IGN ((sighandler_t)1)  ///< Ignore signal.
#define SIG_ERR ((sighandler_t)-1) ///< Error return from signal.

/// @brief Structure used to mask and unmask signals.
/// @details
/// Each unsigned long consists of 32 bits, thus, the maximum number of signals
/// that may be declared is 64.
/// Signals are divided into two cathegories, identified by the two unsigned longs:
///     [ 1, 31] corresponds to normal signals;
///     [32, 64] corresponds to real-time signals.
typedef struct sigset_t {
    /// Signals divided into two cathegories.
    unsigned long sig[2];
} sigset_t;

/// @brief Holds the information on how to handle a specific signal.
typedef struct sigaction_t {
    /// This field specifies the type of action to be performed; its value can be a pointer
    /// to the signal handler, SIG_DFL (that is, the value 0) to specify that the default
    /// action is performed, or SIG_IGN (that is, the value 1) to specify that the signal is
    /// ignored.
    sighandler_t sa_handler;
    /// This sigset_t variable specifies the signals to be masked when running the signal handler
    sigset_t sa_mask;
    /// This set of flags specifies how the signal must be handled;
    unsigned int sa_flags;
} sigaction_t;

/// @brief Data passed with signal info.
typedef union sigval {
    int sival_int;   ///< Integer value.
    void *sival_ptr; ///< Pointer value.
} sigval_t;

/// @brief Stores information about an occurrence of a specific signal.
typedef struct siginfo_t {
    /// The signal number.
    int si_signo;
    /// A code identifying who raised the signal (see signal_sender_code_t).
    int si_code;
    /// Signal value.
    sigval_t si_value;
    /// The error code of the instruction that caused the signal to be raised, or 0 if there was no error.
    int si_errno;
    /// Process ID of sending process.
    pid_t si_pid;
    /// Real user ID of sending process.
    uid_t si_uid;
    /// Address at which fault occurred.
    void *si_addr;
    /// Exit value or signal for process termination.
    int si_status;
    /// Band event for SIGPOLL/SIGIO.
    int si_band;
} siginfo_t;

/// @brief Send signal to a process.
/// @param pid The pid of the process to which we send the signal.
/// @param sig The type of signal to send.
/// @return On success 0, on error -1 and errno is set appropriately.
int kill(pid_t pid, int sig);

/// @brief Sets the disposition of the signal signum to handler.
/// @param signum  The signal number.
/// @param handler The handler for the signal.
/// @return The previous value of the signal handler, or SIG_ERR on error.
sighandler_t signal(int signum, sighandler_t handler);

/// @brief Examine and change a signal action.
/// @param signum Specifies the signal and can be any valid signal except SIGKILL and SIGSTOP.
/// @param act    If non-NULL, the new action for signal signum is installed from act.
/// @param oldact If non-NULL, the previous action is saved in oldact.
/// @return returns 0 on success; on error, -1 is returned, and errno is set to indicate the error.
int sigaction(int signum, const sigaction_t *act, sigaction_t *oldact);

/// @brief Examine and change blocked signals.
/// @param how    Determines the behavior of the call.
/// @param set    The set of signals to manage by the function.
/// @param oldset If non-NULL, the previous value of the signal mask is stored here.
/// @return returns 0 on success, and -1 on error (errno is set to indicate the cause).
/// @details
/// If set is NULL, then the signal mask is unchanged (i.e., how is
///  ignored), but the current value of the signal mask is
///  nevertheless returned in oldset (if it is not NULL).
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

/// @brief Returns the string describing the given signal.
/// @param sig The signal to inquire.
/// @return String representing the signal.
const char *strsignal(int sig);

/// @brief Prepare an empty set.
/// @param set The set to manipulate.
/// @return 0 on success and -1 on error.
int sigemptyset(sigset_t *set);

/// @brief Prepare a full set.
/// @param set The set to manipulate.
/// @return 0 on success and -1 on error.
int sigfillset(sigset_t *set);

/// @brief Adds the given signal to the correct set.
/// @param set    The set to manipulate.
/// @param signum The signalt to handle.
/// @return 0 on success and -1 on error.
int sigaddset(sigset_t *set, int signum);

/// @brief Removes the given signal to the correct set.
/// @param set    The set to manipulate.
/// @param signum The signalt to handle.
/// @return 0 on success and -1 on error.
int sigdelset(sigset_t *set, int signum);

/// @brief Checks if the given signal is part of the set.
/// @param set    The set to manipulate.
/// @param signum The signalt to handle.
/// @return 1 if signum is a member of set,
///         0 if signum is not a member, and -1 on error.
int sigismember(sigset_t *set, int signum);