/// @file shadow.c
/// @brief
/// @copyright (c) 2005-2020 Rich Felker, et al.
/// This file is based on the code from libmusl.
#include <shadow.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <ctype.h>

static long xatol(char **s)
{
	long x;
	if (**s == ':' || **s == '\n') return -1;
	for (x=0; **s-'0'<10U; ++*s) x=10*x+(**s-'0');
	return x;
}

int __parsespent(char *s, struct spwd *sp)
{
	sp->sp_namp = s;
	if (!(s = strchr(s, ':'))) return -1;
	*s = 0;

	sp->sp_pwdp = ++s;
	if (!(s = strchr(s, ':'))) return -1;
	*s = 0;

	s++; sp->sp_lstchg = xatol(&s);
	if (*s != ':') return -1;

	s++; sp->sp_min = xatol(&s);
	if (*s != ':') return -1;

	s++; sp->sp_max = xatol(&s);
	if (*s != ':') return -1;

	s++; sp->sp_warn = xatol(&s);
	if (*s != ':') return -1;

	s++; sp->sp_inact = xatol(&s);
	if (*s != ':') return -1;

	s++; sp->sp_expire = xatol(&s);
	if (*s != ':') return -1;

	s++; sp->sp_flag = xatol(&s);
	if (*s != '\n') return -1;
	return 0;
}

int getspnam_r(const char *name, struct spwd *sp, char *buf, size_t size, struct spwd **res)
{
	char path[20+NAME_MAX];
	int rv = 0;
	int fd;
	size_t k, l = strlen(name);
	int skip = 0;
	int cs;
	int orig_errno = errno;

	*res = 0;

	/* Disallow potentially-malicious user names */
	if (*name=='.' || strchr(name, '/') || !l)
		return errno = EINVAL;

	/* Buffer size must at least be able to hold name, plus some.. */
	if (size < l+100)
		return errno = ERANGE;

	fd = open(SHADOW, O_RDONLY, 0);
	if (fd < 0) {
		return errno;
	}

	while (fgets(buf, size, fd) && (k=strlen(buf))>0) {
		if (skip || strncmp(name, buf, l) || buf[l]!=':') {
			skip = buf[k-1] != '\n';
			continue;
		}
		if (buf[k-1] != '\n') {
			rv = ERANGE;
			break;
		}

		if (__parsespent(buf, sp) < 0) continue;
		*res = sp;
		break;
	}
	errno = rv ? rv : orig_errno;
	return rv;
}

#define LINE_LIM 256

struct spwd *getspnam(const char *name)
{
	static struct spwd sp;
	static char *line;
	struct spwd *res;
	int e;
	int orig_errno = errno;

	if (!line) line = malloc(LINE_LIM);
	if (!line) return 0;
	e = getspnam_r(name, &sp, line, LINE_LIM, &res);
	errno = e ? e : orig_errno;
	return res;
}
