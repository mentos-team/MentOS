///                MentOS, The Mentoring Operating system project
/// @file   sys.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

// TODO: doxygen comment.
/// @brief
/// @param magic1
/// @param magic2
/// @param cmd
/// @param arg
int sys_reboot(int magic1, int magic2, unsigned int cmd, void *arg);
