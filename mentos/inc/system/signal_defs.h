///                MentOS, The Mentoring Operating system project
/// @file signal_defs.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Signal names (from the Unix specification on signals)

/// Hangup.
#define SIGHUP      1

/// Interupt.
#define SIGINT      2

/// Quit.
#define SIGQUIT     3

/// Illegal instruction.
#define SIGILL      4

/// A breakpoint or trace instruction has been reached.
#define SIGTRAP     5

/// Another process has requested that you abort.
#define SIGABRT     6

/// Emulation trap XXX.
#define SIGEMT      7

/// Floating-point arithmetic exception.
#define SIGFPE      8

/// You have been stabbed repeated with a large knife.
#define SIGKILL     9

/// Bus error (device error).
#define SIGBUS      10

/// Segmentation fault.
#define SIGSEGV     11

/// Bad system call.
#define SIGSYS      12

/// Attempted to read or write from a broken pipe.
#define SIGPIPE     13

/// This is your wakeup call.
#define SIGALRM     14

/// You have been Schwarzenegger'd.
#define SIGTERM     15

/// User Defined Signal #1.
#define SIGUSR1     16

/// User Defined Signal #2.
#define SIGUSR2     17

/// Child status report.
#define SIGCHLD     18

/// We need moar powah!.
#define SIGPWR      19

/// Your containing terminal has changed size.
#define SIGWINCH    20

/// An URGENT! event (On a socket).
#define SIGURG      21

/// XXX OBSOLETE; socket i/o possible.
#define SIGPOLL     22

/// Stopped (signal).
#define SIGSTOP     23

/// ^Z (suspend).
#define SIGTSTP     24

/// Unsuspended (please, continue).
#define SIGCONT     25

/// TTY input has stopped.
#define SIGTTIN     26

/// TTY output has stopped.
#define SIGTTOUT    27

/// Virtual timer has expired.
#define SIGVTALRM   28

/// Profiling timer expired.
#define SIGPROF     29

/// CPU time limit exceeded.
#define SIGXCPU     30

/// File size limit exceeded.
#define SIGXFSZ     31

/// Herp.
#define SIGWAITING  32

/// Die in a fire.
#define SIGDIAF     33

/// The sending process does not like you.
#define SIGHATE     34

/// Window server event.
#define SIGWINEVENT 35

/// Everybody loves cats.
#define SIGCAT      36

#define SIGTTOU     37

#define NUMSIGNALS  38

#define NSIG        NUMSIGNALS
