// main/mode_clicker.c —— UP=prev page, DOWN=next page, OK=play/black.
// Transport behind clicker_hid.h; UI shows link state + sent count.
#include "mode_clicker.h"
#include "bsp_display.h"
#include "clicker_hid.h"
#include "deskpet_i18n.h"
#include "sprite.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdio.h>

static const char *TAG = "mode_clicker";

// Clicker dim rule: long talks idle the screen, but NEVER exit the mode.
// Dim to near-black after timeout; any key restores full brightness.
#define CLICKER_DIM_MS 120000u
// Entering clicker mode is the user's "let a new host pair" gesture.
#define CLICKER_PAIR_WINDOW_MS 120000u

static lv_obj_t *s_scr;
static lv_obj_t *s_status, *s_count, *s_hint, *s_oc, *s_title;
static lv_timer_t *s_timer;
static uint32_t s_dropped;
static uint64_t s_last_key_ms;
static bool s_dimmed;

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

extern const lv_font_t zh_subset;

static void refresh(void)
{
    char buf[48];
    deskpet_lang_t lang = deskpet_get_lang();
    bool up = clicker_hid_connected();
    if (lang == LANG_ZH)
        snprintf(buf, sizeof(buf), "%s", up ? "已连接" : "未配对");
    else
        snprintf(buf, sizeof(buf), "%s", up ? "LINKED" : "PAIRING");
    lv_label_set_text(s_status, buf);
    lv_obj_set_style_text_color(s_status,
                                lv_color_hex(up ? 0x79D6A5 : 0xF2C66D), 0);
    snprintf(buf, sizeof(buf), "%u/%u", (unsigned)clicker_hid_sent_total(), (unsigned)s_dropped);
    lv_label_set_text(s_count, buf);
    if (lang == LANG_ZH)
        snprintf(buf, sizeof(buf), "上下翻页 OK放映 双击黑屏");
    else
        snprintf(buf, sizeof(buf), "UP/DN page OK F5 x2 blank");
    lv_label_set_text(s_hint, buf);
    const lv_font_t *f = (lang == LANG_ZH) ? &zh_subset : &lv_font_montserrat_14;
    lv_obj_set_style_text_font(s_status, f, 0);
    lv_obj_set_style_text_font(s_count, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_hint, f, 0);
    lv_obj_set_style_text_font(s_title, f, 0);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    if (!s_dimmed && now_ms() - s_last_key_ms > CLICKER_DIM_MS) {
        s_dimmed = true;
        bsp_display_backlight(5);  // near-black, mode stays armed
    }
    refresh();  // link state may change underneath
}

void mode_clicker_enter(void)
{
    s_dropped = 0;
    s_last_key_ms = now_ms();
    s_dimmed = false;
    bsp_display_backlight(100);
    clicker_hid_init();
    clicker_hid_open_pairing(CLICKER_PAIR_WINDOW_MS);
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x0B0B1A), 0);
    lv_obj_set_style_text_color(s_scr, lv_color_hex(0xE8E8F0), 0);
    lv_obj_t *panel = lv_obj_create(s_scr);
    lv_obj_set_size(panel, 224, 152);
    lv_obj_set_pos(panel, 8, 8);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x101426), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xB38BE8), 0);
    s_title = lv_label_create(s_scr);
    lv_label_set_text(s_title, deskpet_tr(S_MODE_CLICKER, deskpet_get_lang()));
    lv_obj_set_pos(s_title, 16, 16);
    s_status = lv_label_create(s_scr);
    lv_obj_set_pos(s_status, 16, 50);
    s_count = lv_label_create(s_scr);
    lv_obj_set_pos(s_count, 16, 82);
    s_hint = lv_label_create(s_scr);
    lv_obj_set_pos(s_hint, 16, 128);
    s_oc = lv_img_create(s_scr);
    const lv_img_dsc_t *oc = sprite_get(SPR_OC_CLICKER);
    if (oc) lv_img_set_src(s_oc, oc);
    lv_image_set_antialias(s_oc, false);
    lv_img_set_zoom(s_oc, 512);
    lv_obj_set_pos(s_oc, 104, 214);
    s_timer = lv_timer_create(tick, 1000, NULL);
    lv_screen_load(s_scr);
    refresh();
}

void mode_clicker_exit(void)
{
    clicker_hid_deinit();
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_status = s_count = s_hint = s_oc = s_title = NULL;
    }
}

void mode_clicker_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_DOUBLE && btn == BSP_BTN_OK) {
        s_last_key_ms = now_ms();
        if (!clicker_hid_send(CLICK_BLANK)) s_dropped++;
        refresh();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    s_last_key_ms = now_ms();
    if (s_dimmed) {
        s_dimmed = false;
        bsp_display_backlight(100);  // wake bright, stay in mode
    }
    clicker_key_t k;
    if (btn == BSP_BTN_UP) k = CLICK_PREV;
    else if (btn == BSP_BTN_DOWN) k = CLICK_NEXT;
    else if (btn == BSP_BTN_OK) k = CLICK_PLAY;
    else return;
    if (!clicker_hid_send(k)) s_dropped++;
    ESP_LOGI(TAG, "click %d %s", (int)k, clicker_hid_connected() ? "sent" : "dropped");
    refresh();
}
