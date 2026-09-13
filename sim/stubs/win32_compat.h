// sim/stubs/win32_compat.h —— Windows / MSVC compatibility stubs
#pragma once
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

#ifndef localtime_r
static inline struct tm *localtime_r(const time_t *timep, struct tm *result)
{
    if (!timep || !result) return NULL;
    return (localtime_s(result, timep) == 0) ? result : NULL;
}
#endif

#ifndef strdup
#define strdup _strdup
#endif

#ifndef setenv
static inline int setenv(const char *name, const char *value, int overwrite)
{
    if (!name || !value) return -1;
    if (!overwrite) {
        size_t needed = 0;
        getenv_s(&needed, NULL, 0, name);
        if (needed > 0) return 0;
    }
    return _putenv_s(name, value);
}
#endif

#endif
