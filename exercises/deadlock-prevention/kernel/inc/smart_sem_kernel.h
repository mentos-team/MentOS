///                MentOS, The Mentoring Operating system project
/// @file smart_sem_kernel.h
/// @brief Smart semaphore semaphore kernel-side implementation header code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// N.B. Header to use only in kernel space.

#pragma once

#include "stdatomic.h"

/// @brief Smart semaphore creation.
/// @return The semaphore id or -1 if creation failed.
int sys_sem_create();

/// @brief Destruction of a created smart semaphore.
/// @param id Smart semaphore ID.
/// @return Return -1 if destroy failed, otherwise 0.
int sys_sem_destroy(int id);

/// @brief Initialization of a created smart semaphore.
/// @param id Smart semaphore ID.
/// @return Return -1 if initialization failed, otherwise 0.
int sys_sem_init(int id);

/// @brief Tries a safety acquisition of a smart semaphore identified by
/// an ID and, if available, takes the ownership.
/// @param id Smart semaphore ID.
/// @return Return -1 if creation failed, return 0 if semaphore busy or
/// the acquisition is unsafe, otherwise return 1 if semaphore has been
/// acquired.
int sys_sem_try_acquire(int id);

/// @brief Release the ownership of a smart semaphore.
/// @param id Smart semaphore ID.
/// @return Return -1 if release failed, otherwise 0.
int sys_sem_release(int id);
