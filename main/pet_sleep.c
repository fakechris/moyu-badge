// main/pet_sleep.c —— deep sleep is the real "soft off": panel and MCU power
// down while the battery keeps the RTC counter alive at tens of uA, so wall
// time keeps advancing. Wake = any of the three buttons: they share GPIO0
// through the resistor ladder (bsp_pins.h ADC1_CH0); unpressed sits at 3.3V
// and any press pulls the node to <=0.6V, under the wake-low threshold.
#include "pet_sleep.h"

#ifdef ESP_PLATFORM

#include "bsp_display.h"
#include "deskpet_time.h"
#include "esp_lcd_panel_ops.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"

bool pet_sleep_powered(void)
{
    return usb_serial_jtag_is_connected();
}

void pet_sleep_now(void)
{
    bsp_display_backlight(0);
    esp_lcd_panel_handle_t panel = bsp_display_panel();
    if (panel) esp_lcd_panel_disp_on_off(panel, false);
    deskpet_time_sleep_stamp();
    // 按键节点平时由 button 组件当 ADC 用;入睡显式把它配回数字输入并加内部
    // 上拉(与外部 10k 并联),消除"深睡里引脚浮空 → 虚假 LOW 唤醒 → 直接重启
    // 进游戏"这类幽灵唤醒。
    const gpio_config_t wake_pin = {
        .pin_bit_mask = UINT64_C(1) << GPIO_NUM_0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&wake_pin);
    esp_deep_sleep_enable_gpio_wakeup(UINT64_C(1) << GPIO_NUM_0,
                                      ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
}

#else  // host sim: never sleeps

bool pet_sleep_powered(void) { return true; }
void pet_sleep_now(void) {}

#endif
