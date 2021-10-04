///                MentOS, The Mentoring Operating system project
/// @file ioctls.h
/// @brief Definitions of tty ioctl numbers. 0x54 is just a magic number to make these
///        relatively unique ('T')

#pragma once

#define TCGETS 0x5401U ///< Get the current serial port settings.
#define TCSETS 0x5402U ///< Set the current serial port settings.
