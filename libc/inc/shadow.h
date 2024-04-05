// @file shadow.h
/// @brief Secret password file routines
#pragma once

#include "stddef.h"

#define SHADOW "/etc/shadow"

struct spwd {
    char *sp_namp;             ///< user login name.
    char *sp_pwdp;             ///< encrypted password.
    long int sp_lstchg;        ///< last password change.
    long int sp_min;           ///< days until change allowed.
    long int sp_max;           ///< days before change required.
    long int sp_warn;          ///< days warning for expiration.
    long int sp_inact;         ///< days before account inactive.
    long int sp_expire;        ///< date when account expires.
    unsigned long int sp_flag; ///< reserved for future use.
};

struct spwd *getspnam(const char *);
int getspnam_r(const char *, struct spwd *, char *, size_t, struct spwd **);
