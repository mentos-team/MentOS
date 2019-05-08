///                MentOS, The Mentoring Operating system project
/// @file prio.h
/// @brief Defines processes priority value.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#define MAX_NICE    +19

#define MIN_NICE    -20

#define NICE_WIDTH  (MAX_NICE - MIN_NICE + 1)

// Priority of a process goes from 0..MAX_PRIO-1, valid RT
// priority is 0..MAX_RT_PRIO-1, and SCHED_NORMAL/SCHED_BATCH
// tasks are in the range MAX_RT_PRIO..MAX_PRIO-1. Priority
// values are inverted: lower p->prio value means higher priority.
//
// The MAX_USER_RT_PRIO value allows the actual maximum
// RT priority to be separate from the value exported to
// user-space.  This allows kernel threads to set their
// priority to a value higher than any user task.
// Note: MAX_RT_PRIO must not be smaller than MAX_USER_RT_PRIO.

#define MAX_RT_PRIO 100

#define MAX_PRIO     (MAX_RT_PRIO + NICE_WIDTH)

#define DEFAULT_PRIO (MAX_RT_PRIO + NICE_WIDTH / 2)

// Convert user-nice values [ -20 ... 0 ... 19 ]
// to static priority [ MAX_RT_PRIO..MAX_PRIO-1 ],
// and back.
#define NICE_TO_PRIO(nice) ((nice) + DEFAULT_PRIO)

#define PRIO_TO_NICE(prio) ((prio) - DEFAULT_PRIO)

// 'User priority' is the nice value converted to something we
// can work with better when scaling various scheduler parameters,
// it's a [ 0 ... 39 ] range.
#define USER_PRIO(p)  ((p)-MAX_RT_PRIO)

#define TASK_USER_PRIO(p) USER_PRIO((p)->static_prio)

#define MAX_USER_PRIO (USER_PRIO(MAX_PRIO))

static const int prio_to_weight[NICE_WIDTH] = {
    /* 100 */     88761, 71755, 56483, 46273, 36291,
    /* 105 */     29154, 23254, 18705, 14949, 11916,
    /* 110 */      9548, 7620, 6100, 4904, 3906,
    /* 115 */      3121, 2501, 1991, 1586, 1277,
    /* 120 */      1024, 820, 655, 526, 423,
    /* 125 */       335, 272, 215, 172, 137,
    /* 130 */       110, 87, 70, 56, 45,
    /* 135 */        36, 29, 23, 18, 15
};
