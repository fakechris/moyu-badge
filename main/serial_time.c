// main/serial_time.c —— one-line console protocol for host-side time sync.
// The build-time seed is only as fresh as the last build; reading the real
// time from the host at flash (or any) moment makes wall time exact. Uses the
// default console VFS (non-blocking reads) — no USJ driver, logs untouched.
#ifdef ESP_PLATFORM

#include "serial_time.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "app_shell.h"
#include "pet_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "serial_time";

static void handle_line(char *line)
{
    if (strcmp(line, "ID?") == 0) {
        // Host-side identity handshake: tools/flash.sh only talks to a port
        // that answers this (a random /dev/cu.usbmodem* may be another device).
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        printf("DESKPET-ID %02X%02X%02X%02X%02X%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return;
    }
    if (strcmp(line, "SLEEP") == 0) {
        // Debug/test hook: same path as the standby 30s timer (persist wake
        // target, panel off, deep sleep). Lets a host reproduce sleep/wake
        // without touching the buttons. Never returns if sleep succeeds.
        printf("SLEEP: entering deep sleep\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(100));  // let the console FIFO drain
        app_shell_sleep_prepare();
        pet_sleep_now();
        return;
    }
    if (strncmp(line, "TIME ", 5) != 0) return;
    long long sec = strtoll(line + 5, NULL, 10);
    if (sec < 1700000000LL) {  // sanity floor: 2023-11, rejects host garbage
        ESP_LOGW(TAG, "bad epoch '%s'", line + 5);
        return;
    }
    struct timeval tv = { .tv_sec = (time_t)sec, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    time_t now = time(NULL);
    struct tm bt;
    localtime_r(&now, &bt);
    char buf[24];
    strftime(buf, sizeof(buf), "%F %T", &bt);
    printf("TIME OK %lld %s\n", sec, buf);  // ack line for flash.sh
    ESP_LOGI(TAG, "wall clock set from serial: %s Beijing", buf);
}

static void serial_time_task(void *arg)
{
    (void)arg;
    static char buf[40];
    size_t n = 0;
    for (;;) {
        char c;
        if (read(0, &c, 1) == 1) {
            if (c == '\n' || c == '\r') {
                if (n) {
                    buf[n] = '\0';
                    n = 0;
                    handle_line(buf);
                }
            } else if (n + 1 < sizeof(buf)) {
                buf[n++] = c;
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void serial_time_start(void)
{
    if (xTaskCreate(serial_time_task, "serial_time", 3072, NULL, 5, NULL) != pdPASS)
        ESP_LOGE(TAG, "task create failed");
}

#else  // host sim

void serial_time_start(void) {}

#endif
