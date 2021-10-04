
#include "sys/unistd.h"
#include "system/syscall_types.h"
#include "sys/errno.h"

_syscall1(int, alarm, int, seconds)
