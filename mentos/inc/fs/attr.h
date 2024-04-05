/// @file attr.h
/// @brief change file attributes
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

int sys_chown(const char* path, uid_t owner, gid_t group);

int sys_lchown(const char* path, uid_t owner, gid_t group);

int sys_fchown(int fd, uid_t owner, gid_t group);

int sys_chmod(const char* path, mode_t mode);

int sys_fchmod(int fd, mode_t mode);
