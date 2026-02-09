///                MentOS, The Mentoring Operating system project
/// @file smart_sem_user.h
/// @brief Semaphore user-side implementation header code.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.
/// N.B. Header to use only in user space.

#pragma once

/// @brief Semaphore creation.
/// @return The semaphore id or -1 if creation failed.
/// Set the errno code if failed.
int sem_create();

/// @brief Destruction of a created semaphore.
/// @param id Semaphore ID.
/// @return Return -1 and set errno if destroy failed, otherwise 0.
int sem_destroy(int id);

/// @brief Initialization of a created semaphore.
/// @param id Semaphore ID.
/// @return Return -1 and set errno if initialization failed, otherwise 0.
int sem_init(int id);

/// @brief Acquire the semaphore ownership if available, otherwise waits that
/// becomes available.
/// @param id Semaphore ID.
/// @return Return -1 and set errno if creation failed, otherwise 0 if
/// semaphore has been successfully acquired.
int sem_acquire(int id);

/// @brief Release the ownership of a Semaphore.
/// @param id Semaphore ID.
/// @return Return -1 and set errno if release failed, otherwise 0.
int sem_release(int id);
