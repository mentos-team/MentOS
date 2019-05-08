/// @file   initfscp.h
/// @brief

#pragma once

#define MAX_FILENAME_LENGTH 64
#define MAX_FILES 32
#define INITFSCP_VER "0.3.0"

#define FS_FILE 0x01 ///< Identifies a file.
#define FS_DIRECTORY 0x02 ///< Identifies a directory.
#define FS_CHARDEVICE 0x04 ///< Identifies a character devies.
#define FS_BLOCKDEVICE 0x08 ///< Identifies a block devies.
#define FS_PIPE 0x10 ///< Identifies a pipe.
#define FS_SYMLINK 0x20 ///< Identifies a symbolic link.
#define FS_MOUNTPOINT 0x40 ///< Identifies a mount-point.

#define RESET "\033[00m"
#define BLACK "\033[30m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

/// @brief Information concerning a file.
typedef struct initrd_file_t {
	/// Number used as delimiter, it must be set to 0xBF.
	int magic;
	/// The name of the file.
	char fileName[MAX_FILENAME_LENGTH];
	/// The type of the file.
	short int file_type;
	/// The uid of the owner.
	int uid;
	/// Offset of the starting address.
	unsigned int offset;
	/// Dimension of the file.
	unsigned int length;
} initrd_file_t;
