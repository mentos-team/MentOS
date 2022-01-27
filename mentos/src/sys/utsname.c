/// @file utsname.c
/// @brief Functions used to provide information about the machine & OS.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "string.h"
#include "sys/utsname.h"
#include "version.h"
#include "io/debug.h"
#include "sys/errno.h"
#include "fcntl.h"
#include "fs/vfs.h"

static inline int __gethostname(char *name, size_t len)
{
    // Check if name is an invalid address.
    if (!name)
        return -EFAULT;
    // Check if len is negative.
    if (len < 0)
        return -EINVAL;
    // Open the file.
    vfs_file_t *file = vfs_open("/etc/hostname", O_RDONLY, 0);
    if (file == NULL) {
        pr_err("Cannot find `/etc/hostname`.\n");
        return -ENOENT;
    }
    // Clear the buffer.
    memset(name, 0, len);
    // Read the content of the file.
    ssize_t ret = vfs_read(file, name, 0UL, len);
    if (ret < 0) {
        pr_err("Failed to read `/etc/hostname`.\n");
        return ret;
    }
    // Close the file.
    vfs_close(file);
    return 0;
}

int sys_uname(utsname_t *buf)
{
    if (buf == NULL) {
        return -EFAULT;
    }
    // Uname code goes here.
    strcpy(buf->sysname, OS_NAME);
    strcpy(buf->version, OS_VERSION);
    strcpy(buf->release, OS_VERSION);
    __gethostname(buf->nodename, SYS_LEN);
    strcpy(buf->machine, "i686");
    return 0;
}
