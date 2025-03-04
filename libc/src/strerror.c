/// @file strerror.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "strerror.h"
#include "string.h"

char *strerror(int errnum)
{
    static char error[1024];

    switch (errnum) {
    case 0:
        strcpy(error, "Success");
        break;
    case EPERM:
        strcpy(error, "Operation not permitted");
        break;
    case ENOENT:
        strcpy(error, "No such file or directory");
        break;
    case ESRCH:
        strcpy(error, "No such process");
        break;
    case EINTR:
        strcpy(error, "Interrupted system call");
        break;
    case EIO:
        strcpy(error, "I/O error");
        break;
    case ENXIO:
        strcpy(error, "No such device or address");
        break;
    case E2BIG:
        strcpy(error, "Argument list too long");
        break;
    case ENOEXEC:
        strcpy(error, "Exec format error");
        break;
    case EBADF:
        strcpy(error, "Bad file descriptor");
        break;
    case ECHILD:
        strcpy(error, "No child processes");
        break;
    case EAGAIN:
        strcpy(error, "Resource temporarily unavailable");
        break;
    case ENOMEM:
        strcpy(error, "Not enough memory");
        break;
    case EACCES:
        strcpy(error, "Permission denied");
        break;
    case EFAULT:
        strcpy(error, "Bad address");
        break;
    case ENOTBLK:
        strcpy(error, "Block device required");
        break;
    case EBUSY:
        strcpy(error, "Device or resource busy");
        break;
    case EEXIST:
        strcpy(error, "File exists");
        break;
    case EXDEV:
        strcpy(error, "Cross-device link");
        break;
    case ENODEV:
        strcpy(error, "No such device");
        break;
    case ENOTDIR:
        strcpy(error, "Not a directory");
        break;
    case EISDIR:
        strcpy(error, "Is a directory");
        break;
    case EINVAL:
        strcpy(error, "Invalid argument");
        break;
    case ENFILE:
        strcpy(error, "Too many open files in system");
        break;
    case EMFILE:
        strcpy(error, "Too many open files");
        break;
    case ENOTTY:
        strcpy(error, "Inappropriate I/O control operation");
        break;
    case ETXTBSY:
        strcpy(error, "Text file busy");
        break;
    case EFBIG:
        strcpy(error, "File too large");
        break;
    case ENOSPC:
        strcpy(error, "No space left on device");
        break;
    case ESPIPE:
        strcpy(error, "Illegal seek");
        break;
    case EROFS:
        strcpy(error, "Read-only file system");
        break;
    case EMLINK:
        strcpy(error, "Too many links");
        break;
    case EPIPE:
        strcpy(error, "Broken pipe");
        break;
    case EDOM:
        strcpy(error, "Mathematics argument out of domain");
        break;
    case ERANGE:
        strcpy(error, "Result out of range");
        break;
    case EDEADLK:
        strcpy(error, "Resource deadlock would occur");
        break;
    case ENAMETOOLONG:
        strcpy(error, "File name too long");
        break;
    case ENOLCK:
        strcpy(error, "No locks available");
        break;
    case ENOSYS:
        strcpy(error, "Function not implemented");
        break;
    case ENOTEMPTY:
        strcpy(error, "Directory not empty");
        break;
    case ELOOP:
        strcpy(error, "Too many symbolic links encountered");
        break;
    case ENOMSG:
        strcpy(error, "No message of desired type");
        break;
    case EIDRM:
        strcpy(error, "Identifier removed");
        break;
    case ECHRNG:
        strcpy(error, "Channel number out of range");
        break;
    case EL2NSYNC:
        strcpy(error, "Level 2 not synchronized");
        break;
    case EL3HLT:
        strcpy(error, "Level 3 halted");
        break;
    case EL3RST:
        strcpy(error, "Level 3 reset");
        break;
    case ELNRNG:
        strcpy(error, "Link number out of range");
        break;
    case EUNATCH:
        strcpy(error, "Protocol driver not attached");
        break;
    case ENOCSI:
        strcpy(error, "No CSI structure available");
        break;
    case EL2HLT:
        strcpy(error, "Level 2 halted");
        break;
    case EBADE:
        strcpy(error, "Invalid exchange");
        break;
    case EBADR:
        strcpy(error, "Invalid request descriptor");
        break;
    case EXFULL:
        strcpy(error, "Exchange full");
        break;
    case ENOANO:
        strcpy(error, "No anode");
        break;
    case EBADRQC:
        strcpy(error, "Invalid request code");
        break;
    case EBADSLT:
        strcpy(error, "Invalid slot");
        break;
    case ENOSTR:
        strcpy(error, "Device not a stream");
        break;
    case ENODATA:
        strcpy(error, "No data available");
        break;
    case ETIME:
        strcpy(error, "Timer expired");
        break;
    case ENOSR:
        strcpy(error, "Out of stream resources");
        break;
    case ENONET:
        strcpy(error, "Machine not on network");
        break;
    case ENOPKG:
        strcpy(error, "Package not installed");
        break;
    case EREMOTE:
        strcpy(error, "Object is remote");
        break;
    case ENOLINK:
        strcpy(error, "Link severed");
        break;
    case EADV:
        strcpy(error, "Advertise error");
        break;
    case ESRMNT:
        strcpy(error, "SR mount error");
        break;
    case ECOMM:
        strcpy(error, "Communication error on send");
        break;
    case EPROTO:
        strcpy(error, "Protocol error");
        break;
    case EMULTIHOP:
        strcpy(error, "Multihop attempted");
        break;
    case EBADMSG:
        strcpy(error, "Not a data message");
        break;
    case EOVERFLOW:
        strcpy(error, "Value too large for data type");
        break;
    case ENOTUNIQ:
        strcpy(error, "Name not unique on network");
        break;
    case EBADFD:
        strcpy(error, "File descriptor in bad state");
        break;
    case EREMCHG:
        strcpy(error, "Remote address changed");
        break;
    case ELIBACC:
        strcpy(error, "Cannot access a needed shared library");
        break;
    case ELIBBAD:
        strcpy(error, "Accessing a corrupted shared library");
        break;
    case ELIBSCN:
        strcpy(error, "Corrupted .lib section in a.out");
        break;
    case ELIBMAX:
        strcpy(error, "Exceeded shared library system limit");
        break;
    case ELIBEXEC:
        strcpy(error, "Cannot execute shared library directly");
        break;
    case EUCLEAN:
        strcpy(error, "Structure needs cleaning");
        break;
    case ENOTNAM:
        strcpy(error, "Not a XENIX named type file");
        break;
    case ENAVAIL:
        strcpy(error, "No XENIX semaphores available");
        break;
    case EISNAM:
        strcpy(error, "Is a named type file");
        break;
    case EREMOTEIO:
        strcpy(error, "Remote I/O error");
        break;
    case EDQUOT:
        strcpy(error, "Quota exceeded");
        break;
    case ENOMEDIUM:
        strcpy(error, "No medium found");
        break;
    case EMEDIUMTYPE:
        strcpy(error, "Wrong medium type");
        break;
    case ENOTSCHEDULABLE:
        strcpy(error, "Process cannot be scheduled");
        break;
    default:
        strcpy(error, "Unknown error");
        break;
    }

    return error;
}
