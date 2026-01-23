/// @file fhs.c
/// @brief Filesystem Hierarchy Standard (FHS) initialization.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.
///
/// @details This module ensures that standard FHS directories are created
/// during system initialization. According to FHS 3.0, certain directories
/// should always exist with specific permissions to maintain compatibility
/// with standard Unix/Linux programs.
///
/// Reference: https://refspecs.linuxfoundation.org/FHS_3.0/fhs-3.0.html

#include "fs/fhs.h"
#include "fs/vfs.h"
#include "io/debug.h"
#include "sys/stat.h"
#include "system/syscall.h"

/// @brief Structure defining a standard FHS directory.
typedef struct {
    const char *path;        ///< Path to the directory
    mode_t mode;             ///< Permissions (including type bits)
    const char *description; ///< Description of the directory's purpose
} fhs_directory_t;

/// @brief Standard FHS directories that should be created on system startup.
/// @details These directories are defined by the Filesystem Hierarchy Standard
/// and are expected to exist on all Linux-like systems.
static const fhs_directory_t fhs_directories[] = {
    // Temporary files (world-writable with sticky bit set)
    {"/tmp",       S_IFDIR | 01777, "Temporary file storage"        },
    // Home directories
    {"/home",      S_IFDIR | 0755,  "User home directories"         },
    {"/root",      S_IFDIR | 0700,  "Root user home directory"      },
    // Variable data
    {"/var",       S_IFDIR | 0755,  "Variable data"                 },
    {"/var/tmp",   S_IFDIR | 01777, "Temporary variable data"       },
    {"/var/log",   S_IFDIR | 0755,  "Log files"                     },
    // User binaries and libraries
    {"/usr",       S_IFDIR | 0755,  "User programs and data"        },
    {"/usr/bin",   S_IFDIR | 0755,  "User executable programs"      },
    {"/usr/lib",   S_IFDIR | 0755,  "User libraries"                },
    {"/usr/share", S_IFDIR | 0755,  "User data"                     },
    // System binaries and libraries
    {"/bin",       S_IFDIR | 0755,  "Essential executable programs" },
    {"/lib",       S_IFDIR | 0755,  "Essential system libraries"    },
    {"/sbin",      S_IFDIR | 0755,  "System administration programs"},
    // Configuration
    {"/etc",       S_IFDIR | 0755,  "System configuration"          },
    // Device files
    {"/dev",       S_IFDIR | 0755,  "Device files"                  },
    // Process information
    {"/proc",      S_IFDIR | 0555,  "Process information"           },
    // Mount points
    {"/mnt",       S_IFDIR | 0755,  "Temporary mount points"        },
    {"/media",     S_IFDIR | 0755,  "Removable media mount points"  },
    // Null terminator
    {NULL,         0,               NULL                            },
};

/// @brief Creates a directory if it doesn't already exist.
/// @param path The path to the directory.
/// @param mode The permissions and type bits for the directory.
/// @return 0 on success, -1 on failure.
static int create_directory_if_not_exists(const char *path, mode_t mode)
{
    // Try to stat the path to check if it already exists
    stat_t stat_buf;
    int stat_result = vfs_stat(path, &stat_buf);

    if (stat_result == 0) {
        // Path exists, check if it's a directory
        if (S_ISDIR(stat_buf.st_mode)) {
            pr_debug("[FHS] Directory already exists: %s\n", path);
            return 0;
        } else {
            // Path exists but is not a directory
            pr_err("[FHS] Path exists but is not a directory: %s\n", path);
            return -1;
        }
    }

    // Directory doesn't exist, create it using vfs_mkdir
    if (vfs_mkdir(path, mode & 0777) != 0) {
        pr_err("[FHS] Failed to create directory: %s\n", path);
        return -1;
    }

    pr_debug("[FHS] Created directory: %s (mode: 0%o)\n", path, mode & 0777);
    return 0;
}

int fhs_initialize(void)
{
    pr_debug("Initializing Filesystem Hierarchy Standard (FHS) directories...\n");

    int failed_count = 0;

    // Iterate through all standard directories and create them
    for (size_t i = 0; fhs_directories[i].path != NULL; i++) {
        const fhs_directory_t *dir = &fhs_directories[i];

        if (create_directory_if_not_exists(dir->path, dir->mode) != 0) {
            pr_warning("[FHS] Warning: Could not ensure existence of %s (%s)\n", dir->path, dir->description);
            failed_count++;
        }
    }

    if (failed_count == 0) {
        pr_debug("[FHS] Successfully initialized all standard directories.\n");
        return 0;
    } else {
        pr_warning("[FHS] %d directory initialization warnings (non-critical).\n", failed_count);
        // We don't fail the entire system if some directories can't be created
        // This allows the system to boot even if the filesystem is incomplete
        return 0;
    }
}
