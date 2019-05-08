///                MentOS, The Mentoring Operating system project
/// @file strerror.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
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
#ifdef ENOENT
	case ENOENT:
		strcpy(error, "No such file or directory");
		break;
#endif
#ifdef ESRCH
	case ESRCH:
		strcpy(error, "No such process");
		break;
#endif
#ifdef EINTR
	case EINTR:
		strcpy(error, "Interrupted system call");
		break;
#endif
#ifdef EIO
	case EIO:
		strcpy(error, "I/O error");
		break;
#endif
#if defined(ENXIO) && (!defined(ENODEV) || (ENXIO != ENODEV))
	case ENXIO:
		strcpy(error, "No such device or address");
		break;
#endif
#ifdef E2BIG
	case E2BIG:
		strcpy(error, "Arg list too long");
		break;
#endif
#ifdef ENOEXEC
	case ENOEXEC:
		strcpy(error, "Exec format error");
		break;
#endif
#ifdef EALREADY
	case EALREADY:
		strcpy(error, "Socket already connected");
		break;
#endif
#ifdef EBADF
	case EBADF:
		strcpy(error, "Bad file number");
		break;
#endif
#ifdef ECHILD
	case ECHILD:
		strcpy(error, "No children");
		break;
#endif
#ifdef EDESTADDRREQ
	case EDESTADDRREQ:
		strcpy(error, "Destination address required");
		break;
#endif
#ifdef EAGAIN
	case EAGAIN:
		strcpy(error, "No more processes");
		break;
#endif
#ifdef ENOMEM
	case ENOMEM:
		strcpy(error, "Not enough space");
		break;
#endif
#ifdef EACCES
	case EACCES:
		strcpy(error, "Permission denied");
		break;
#endif
#ifdef EFAULT
	case EFAULT:
		strcpy(error, "Bad address");
		break;
#endif
#ifdef ENOTBLK
	case ENOTBLK:
		strcpy(error, "Block device required");
		break;
#endif
#ifdef EBUSY
	case EBUSY:
		strcpy(error, "Device or resource busy");
		break;
#endif
#ifdef EEXIST
	case EEXIST:
		strcpy(error, "File exists");
		break;
#endif
#ifdef EXDEV
	case EXDEV:
		strcpy(error, "Cross-device link");
		break;
#endif
#ifdef ENODEV
	case ENODEV:
		strcpy(error, "No such device");
		break;
#endif
#ifdef ENOTDIR
	case ENOTDIR:
		strcpy(error, "Not a directory");
		break;
#endif
#ifdef EHOSTDOWN
	case EHOSTDOWN:
		strcpy(error, "Host is down");
		break;
#endif
#ifdef EINPROGRESS
	case EINPROGRESS:
		strcpy(error, "Connection already in progress");
		break;
#endif
#ifdef EISDIR
	case EISDIR:
		strcpy(error, "Is a directory");
		break;
#endif
#ifdef EINVAL
	case EINVAL:
		strcpy(error, "Invalid argument");
		break;
#endif
#ifdef ENETDOWN
	case ENETDOWN:
		strcpy(error, "Network interface is not configured");
		break;
#endif
#ifdef ENFILE
	case ENFILE:
		strcpy(error, "Too many open files in system");
		break;
#endif
#ifdef EMFILE
	case EMFILE:
		strcpy(error, "Too many open files");
		break;
#endif
#ifdef ENOTTY
	case ENOTTY:
		strcpy(error, "Not a character device");
		break;
#endif
#ifdef ETXTBSY
	case ETXTBSY:
		strcpy(error, "Text file busy");
		break;
#endif
#ifdef EFBIG
	case EFBIG:
		strcpy(error, "File too large");
		break;
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH:
		strcpy(error, "Host is unreachable");
		break;
#endif
#ifdef ENOSPC
	case ENOSPC:
		strcpy(error, "No space left on device");
		break;
#endif
#ifdef ENOTSUP
	case ENOTSUP:
		strcpy(error, "Not supported");
		break;
#endif
#ifdef ESPIPE
	case ESPIPE:
		strcpy(error, "Illegal seek");
		break;
#endif
#ifdef EROFS
	case EROFS:
		strcpy(error, "Read-only file system");
		break;
#endif
#ifdef EMLINK
	case EMLINK:
		strcpy(error, "Too many links");
		break;
#endif
#ifdef EPIPE
	case EPIPE:
		strcpy(error, "Broken pipe");
		break;
#endif
#ifdef EDOM
	case EDOM:
		strcpy(error, "Math argument");
		break;
#endif
#ifdef ERANGE
	case ERANGE:
		strcpy(error, "Result too large");
		break;
#endif
#ifdef ENOMSG
	case ENOMSG:
		strcpy(error, "No message of desired type");
		break;
#endif
#ifdef EIDRM
	case EIDRM:
		strcpy(error, "Identifier removed");
		break;
#endif
#ifdef EDEADLK
	case EDEADLK:
		strcpy(error, "Deadlock");
		break;
#endif
#ifdef ENETUNREACH
	case ENETUNREACH:
		strcpy(error, "Network is unreachable");
		break;
#endif
#ifdef ENOLCK
	case ENOLCK:
		strcpy(error, "No lock");
		break;
#endif
#ifdef ENOSTR
	case ENOSTR:
		strcpy(error, "Not a stream");
		break;
#endif
#ifdef ETIME
	case ETIME:
		strcpy(error, "Stream ioctl timeout");
		break;
#endif
#ifdef ENOSR
	case ENOSR:
		strcpy(error, "No stream resources");
		break;
#endif
#ifdef ENONET
	case ENONET:
		strcpy(error, "Machine is not on the network");
		break;
#endif
#ifdef ENOPKG
	case ENOPKG:
		strcpy(error, "No package");
		break;
#endif
#ifdef EREMOTE
	case EREMOTE:
		strcpy(error, "Resource is remote");
		break;
#endif
#ifdef ENOLINK
	case ENOLINK:
		strcpy(error, "Virtual circuit is gone");
		break;
#endif
#ifdef EADV
	case EADV:
		strcpy(error, "Advertise error");
		break;
#endif
#ifdef ESRMNT
	case ESRMNT:
		strcpy(error, "Srmount error");
		break;
#endif
#ifdef ECOMM
	case ECOMM:
		strcpy(error, "Communication error");
		break;
#endif
#ifdef EPROTO
	case EPROTO:
		strcpy(error, "Protocol error");
		break;
#endif
#ifdef EPROTONOSUPPORT
	case EPROTONOSUPPORT:
		strcpy(error, "Unknown protocol");
		break;
#endif
#ifdef EMULTIHOP
	case EMULTIHOP:
		strcpy(error, "Multihop attempted");
		break;
#endif
#ifdef EBADMSG
	case EBADMSG:
		strcpy(error, "Bad message");
		break;
#endif
#ifdef ELIBACC
	case ELIBACC:
		strcpy(error, "Cannot access a needed shared library");
		break;
#endif
#ifdef ELIBBAD
	case ELIBBAD:
		strcpy(error, "Accessing a corrupted shared library");
		break;
#endif
#ifdef ELIBSCN
	case ELIBSCN:
		strcpy(error, ".lib section in a.out corrupted");
		break;
#endif
#ifdef ELIBMAX
	case ELIBMAX:
		strcpy(error,
		       "Attempting to link in more shared libraries than system limit");
		break;
#endif
#ifdef ELIBEXEC
	case ELIBEXEC:
		strcpy(error, "Cannot exec a shared library directly");
		break;
#endif
#ifdef ENOSYS
	case ENOSYS:
		strcpy(error, "Function not implemented");
		break;
#endif
#ifdef ENMFILE
	case ENMFILE:
		strcpy(error, "No more files");
		break;
#endif
#ifdef ENOTEMPTY
	case ENOTEMPTY:
		strcpy(error, "Directory not empty");
		break;
#endif
#ifdef ENAMETOOLONG
	case ENAMETOOLONG:
		strcpy(error, "File or path name too long");
		break;
#endif
#ifdef ELOOP
	case ELOOP:
		strcpy(error, "Too many symbolic links");
		break;
#endif
#ifdef ENOBUFS
	case ENOBUFS:
		strcpy(error, "No buffer space available");
		break;
#endif
#ifdef EAFNOSUPPORT
	case EAFNOSUPPORT:
		strcpy(error,
		       "Address family not supported by protocol family");
		break;
#endif
#ifdef EPROTOTYPE
	case EPROTOTYPE:
		strcpy(error, "Protocol wrong type for socket");
		break;
#endif
#ifdef ENOTSOCK
	case ENOTSOCK:
		strcpy(error, "Socket operation on non-socket");
		break;
#endif
#ifdef ENOPROTOOPT
	case ENOPROTOOPT:
		strcpy(error, "Protocol not available");
		break;
#endif
#ifdef ESHUTDOWN
	case ESHUTDOWN:
		strcpy(error, "Can't send after socket shutdown");
		break;
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED:
		strcpy(error, "Connection refused");
		break;
#endif
#ifdef EADDRINUSE
	case EADDRINUSE:
		strcpy(error, "Address already in use");
		break;
#endif
#ifdef ECONNABORTED
	case ECONNABORTED:
		strcpy(error, "Software caused connection abort");
		break;
#endif
#if (defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN)))
	case EWOULDBLOCK:
		strcpy(error, "Operation would block");
		break;
#endif
#ifdef ENOTCONN
	case ENOTCONN:
		strcpy(error, "Socket is not connected");
		break;
#endif
#ifdef ESOCKTNOSUPPORT
	case ESOCKTNOSUPPORT:
		strcpy(error, "Socket type not supported");
		break;
#endif
#ifdef EISCONN
	case EISCONN:
		strcpy(error, "Socket is already connected");
		break;
#endif
#ifdef ECANCELED
	case ECANCELED:
		strcpy(error, "Operation canceled");
		break;
#endif
#ifdef ENOTRECOVERABLE
	case ENOTRECOVERABLE:
		strcpy(error, "State not recoverable");
		break;
#endif
#ifdef EOWNERDEAD
	case EOWNERDEAD:
		strcpy(error, "Previous owner died");
		break;
#endif
#ifdef ESTRPIPE
	case ESTRPIPE:
		strcpy(error, "Streams pipe error");
		break;
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (ENOTSUP != EOPNOTSUPP))
	case EOPNOTSUPP:
		strcpy(error, "Operation not supported on socket");
		break;
#endif
#ifdef EMSGSIZE
	case EMSGSIZE:
		strcpy(error, "Message too long");
		break;
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT:
		strcpy(error, "Connection timed out");
		break;
#endif
	default:
		strcpy(error, "Unknown error");
		break;
	}

	return error;
}
