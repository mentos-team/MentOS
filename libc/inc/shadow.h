// @file shadow.h
/// @brief Secret password file routines
#pragma once

#include "stddef.h"

#define SHADOW "/etc/shadow"

struct spwd {
	char *sp_namp;
	char *sp_pwdp;
	long sp_lstchg;
	long sp_min;
	long sp_max;
	long sp_warn;
	long sp_inact;
	long sp_expire;
	unsigned long sp_flag;
};

struct spwd *getspnam(const char *);
int getspnam_r(const char *, struct spwd *, char *, size_t, struct spwd **);
