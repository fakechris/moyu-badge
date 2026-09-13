// sim/stubs/sys/time.h —— MSVC-friendly timeval / gettimeofday for host sim.
#pragma once

#include <time.h>

#ifndef _WIN32
#include_next <sys/time.h>
#else

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    time_t tv_sec;
    long tv_usec;
};
#endif

#ifndef _SUSECONDS_T_DEFINED
#define _SUSECONDS_T_DEFINED
typedef long suseconds_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (!tv) return -1;
    // FILETIME is 100ns ticks since 1601-01-01; Unix epoch starts 1970-01-01.
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    const unsigned long long EPOCH_DIFF = 116444736000000000ULL;
    unsigned long long ticks = uli.QuadPart - EPOCH_DIFF;
    tv->tv_sec = (time_t)(ticks / 10000000ULL);
    tv->tv_usec = (long)((ticks % 10000000ULL) / 10ULL);
    return 0;
}

static inline int settimeofday(const struct timeval *tv, const void *tz)
{
    (void)tv;
    (void)tz;
    // Host sim cannot (and should not) rewrite the OS clock.
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
