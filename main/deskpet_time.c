// main/deskpet_time.c —— boot wall-clock seed: build epoch + Beijing TZ.
// Sources, best first: (1) serial "TIME" command (tools/flash.sh settime),
// (2) deep-sleep restore below, (3) the build-time epoch stamped by
// tools/gen_build_time.cmake. Power-button off loses everything (no RTC
// battery), so after a hard off the clock resumes at last build/flash time.
#include "deskpet_time.h"

#include <stdint.h>

#if __has_include("build_time.h")
#include "build_time.h"  // generated per build, never committed
#endif
#ifndef DESKPET_BUILD_EPOCH_SEC
#define DESKPET_BUILD_EPOCH_SEC 0ULL  // host builds without the CMake stamp
#endif

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_rtc_time.h"
#include "esp_sleep.h"

// Survives deep sleep in RTC memory (the main RAM does not).
#define SLEEP_STAMP_MAGIC UINT32_C(0x534C4550)  // "SLEP"
RTC_DATA_ATTR static struct {
    uint32_t magic;
    uint64_t rtc_us;
    int64_t epoch_sec;
} s_sleep_stamp;

void deskpet_time_sleep_stamp(void)
{
    s_sleep_stamp.magic = SLEEP_STAMP_MAGIC;
    s_sleep_stamp.rtc_us = esp_rtc_get_time_us();
    s_sleep_stamp.epoch_sec = (int64_t)time(NULL);
}

// Deep-sleep wake: the digital world rebooted, but the RTC counter never
// stopped — replay the elapsed time into the wall clock. RC-oscillator grade
// accuracy (minutes/day drift) until the next host time sync.
static void wake_restore(void)
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_GPIO) return;
    if (s_sleep_stamp.magic != SLEEP_STAMP_MAGIC) return;
    s_sleep_stamp.magic = 0;
    int64_t elapsed_us = (int64_t)(esp_rtc_get_time_us() - s_sleep_stamp.rtc_us);
    if (elapsed_us < 0) return;
    struct timeval tv = {
        .tv_sec = (time_t)(s_sleep_stamp.epoch_sec + elapsed_us / 1000000),
        .tv_usec = (suseconds_t)(elapsed_us % 1000000),
    };
    settimeofday(&tv, NULL);
    ESP_LOGI("deskpet_time", "deep-sleep wake: slept %.1f s",
             (double)elapsed_us / 1000000.0);
}
#else
void deskpet_time_sleep_stamp(void) {}
static void wake_restore(void) {}
#endif

void deskpet_time_init(void)
{
    wake_restore();
    setenv("TZ", "CST-8", 1);  // POSIX sign is inverted: CST-8 == UTC+8
    tzset();
    struct timeval seed = { .tv_sec = (time_t)DESKPET_BUILD_EPOCH_SEC, .tv_usec = 0 };
    struct timeval now;
    gettimeofday(&now, NULL);
    if (now.tv_sec < seed.tv_sec) settimeofday(&seed, NULL);
}
