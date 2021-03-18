///                MentOS, The Mentoring Operating system project
/// @file smart_lock.h
/// @brief Smart lock semaphore kernel-side implementation header code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// N.B. Header to use only in kernel space.

#pragma once

#include "stdatomic.h"

/// @brief Smart lock semaphore creation.
/// @return The semaphore id or -1 if creation failed.
int lock_create();

/// @brief Destruction of a created smart lock semaphore.
/// @param id Smart lock semaphore ID.
/// @return Return -1 if destroy failed, otherwise 0.
int lock_destroy(int id);

/// @brief Initialization of a created smart lock semaphore.
/// @param id Smart lock semaphore ID.
/// @return Return -1 if initialization failed, otherwise 0.
int lock_init(int id);

/// @brief Tries a safety acquisition of a smart lock semaphore identified by
/// an ID and, if available, takes the ownership.
/// @param id Smart lock semaphore ID.
/// @return Return -1 if creation failed, return 0 if semaphore busy or
/// the acquisition is unsafe, otherwise return 1 if semaphore has been
/// acquired.
int lock_try_acquire(int id);

/// @brief Release the ownership of a smart lock semaphore.
/// @param id Smart lock semaphore ID.
/// @return Return -1 if release failed, otherwise 0.
int lock_release(int id);
