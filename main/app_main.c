// main/app_main.c —— DeskPet standalone: BSP init, then straight into DeskPet.
// No menu: this firmware IS the pet. OK-long-press-back rule does not apply
// (single app); OK behavior is defined by demo_deskpet_key().
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "app_shell.h"
#include "audio_task.h"
#include "clicker_hid.h"
#include "deskpet_time.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "serial_time.h"

#include <time.h>

static const char *TAG = "deskpet";

// Button callbacks run in the button component task: LVGL work needs the lock,
// same rule as upstream main.c on_key().
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    app_shell_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "DeskPet standalone boot");
    // 复位原因排障:1 上电 / 3 软件重启 / 4 panic / 8 深睡唤醒 / 9 掉电;
    // 唤醒原因非 0 即深睡唤醒(GPIO 唤醒 = 6)。若"自己开机"对应 4/9,是崩溃/掉电。
    ESP_LOGI(TAG, "reset_reason=%d wakeup_cause=%d",
             (int)esp_reset_reason(), (int)esp_sleep_get_wakeup_cause());
    deskpet_time_init();
    time_t now = time(NULL);
    struct tm bt;
    localtime_r(&now, &bt);
    char tbuf[20];
    strftime(tbuf, sizeof(tbuf), "%F %T", &bt);
    ESP_LOGI(TAG, "wall clock: %s Beijing", tbuf);
    // Console "TIME <epoch>" listener: hosts set the clock exactly at flash
    // time (tools/flash.sh settime) instead of relying on the build seed.
    serial_time_start();
    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed, pet cannot continue");
        return;
    }
    bsp_display_backlight(100);

    // Soft dependencies: pet runs even if audio/battery/button degrades.
    if (bsp_button_init(on_key, NULL) != ESP_OK)
        ESP_LOGW(TAG, "button init failed; pet idles without keys");
    esp_err_t audio_ready = bsp_audio_init();
    if (audio_ready != ESP_OK) ESP_LOGW(TAG, "audio init failed; silent pet");
    if (bsp_battery_init() != ESP_OK) ESP_LOGW(TAG, "battery init failed; SOC unknown");

    if (bsp_lvgl_lock(1000)) {
        app_shell_boot();
        bsp_lvgl_unlock();
    }
    // Do not start the blocking render loop without a codec. On early BSP
    // failures bsp_audio_write() returns immediately, which would busy-spin.
    if (audio_ready == ESP_OK) audio_task_start();
    // Coding-status sync shares the BLE HID peripheral and must be reachable
    // even when the user has never opened clicker mode.
    clicker_hid_init();
    ESP_LOGI(TAG, "pet ready");
}
