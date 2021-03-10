///                MentOS, The Mentoring Operating system project
/// @file smart_sem.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

int sem_create();

int sem_destroy(int id);

int sem_init(int id);

int sem_acquire(int id);

int sem_release(int id);