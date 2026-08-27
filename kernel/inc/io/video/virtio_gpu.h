/// @file virtio_gpu.h
/// @brief Entry point for promoting the console onto a virtio-gpu scanout.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Brings up virtio-gpu and hands the console over to it.
/// @return 0 on success, a negative value on failure.
///
/// Called once from kmain(), after memory management and the timer are up. The
/// backend that booted the machine keeps displaying throughout: everything that
/// can fail happens before the scanout is switched, so a machine with no
/// virtio-gpu device, or one where any step of the bring-up fails, simply keeps
/// the console it already had.
///
/// Only compiled into a build whose VIDEO_TYPE asks for it.
int virtio_gpu_promote(void);
