// sim/stubs/win32_compat.h —— Windows / MSVC compatibility stubs
#pragma once
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#endif
