// main/app_shell.c —— mode router + switcher overlay + NVS mode persist.
// Rule: OK-long is global (mode switcher) in every mode, no exceptions.
// Game yields its old OK-long (town/resume move to battle 4th row + Leave row).
#include "app_shell.h"
#include "chiptune.h"
#include "bsp_display.h"
#include "hud_battery.h"
#include "demo_deskpet.h"
#include "deskpet_i18n.h"
#include "deskpet_store.h"
#include "mode_clicker.h"
#include "mode_pomo.h"
#include "mode_vocab.h"
#include "mode_settings.h"
#include "mode_standby.h"
#include "sprite.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <stdio.h>

static const char *TAG = "app_shell";

static _Atomic(app_mode_t) s_mode = APP_MODE_GAME;
static app_mode_t s_prev = APP_MODE_GAME;  // standby wake target
static app_mode_t s_sw_return = APP_MODE_GAME;
static lv_obj_t *s_sw_scr;
static lv_obj_t *s_sw_bg;
static lv_obj_t *s_sw_panel;
static lv_obj_t *s_sw_list;
static lv_obj_t *s_sw_oc;
static int s_sw_cursor;

extern const lv_font_t zh_subset;

static void persist_mode(void)
{
    nvs_handle_t h;
    if (nvs_open("deskpet", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "mode", (uint8_t)atomic_load(&s_mode));
    nvs_commit(h);
    nvs_close(h);
}

static void load_persisted(void)
{
    nvs_handle_t h;
    uint8_t m = APP_MODE_GAME;
    if (nvs_open("deskpet", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, "mode", &m);
        nvs_close(h);
    }
    if (m < APP_MODE_COUNT && m != APP_MODE_STANDBY && m != APP_MODE_SETTINGS) s_mode = (app_mode_t)m;
    // standby never persists (always boot to last active mode).
}

static void mode_enter_hw(app_mode_t m)
{
    // full brightness except standby
    bsp_display_backlight(m == APP_MODE_STANDBY ? 15 : 100);
    switch (m) {
    case APP_MODE_GAME: demo_deskpet_enter(); break;
    case APP_MODE_POMO: mode_pomo_enter(); break;
    case APP_MODE_CLICKER: mode_clicker_enter(); break;
    case APP_MODE_VOCAB: mode_vocab_enter(); break;
    case APP_MODE_STANDBY: mode_standby_enter(); break;
    case APP_MODE_SETTINGS: mode_settings_enter(); break;
    default: break;
    }
}

static void mode_exit_hw(app_mode_t m)
{
    switch (m) {
    case APP_MODE_GAME: demo_deskpet_exit(); break;
    case APP_MODE_POMO: mode_pomo_exit(); break;
    case APP_MODE_CLICKER: mode_clicker_exit(); break;
    case APP_MODE_VOCAB: mode_vocab_exit(); break;
    case APP_MODE_STANDBY: mode_standby_exit(); break;
    case APP_MODE_SETTINGS: mode_settings_exit(); break;
    default: break;
    }
}

static const char *mode_name(app_mode_t m)
{
    deskpet_lang_t lang = deskpet_get_lang();
    switch (m) {
    case APP_MODE_GAME: return deskpet_tr(S_MODE_GAME, lang);
    case APP_MODE_POMO: return deskpet_tr(S_MODE_POMO, lang);
    case APP_MODE_CLICKER: return deskpet_tr(S_MODE_CLICKER, lang);
    case APP_MODE_VOCAB: return deskpet_tr(S_MODE_VOCAB, lang);
    case APP_MODE_STANDBY: return deskpet_tr(S_MODE_STANDBY, lang);
    case APP_MODE_SETTINGS: return deskpet_tr(S_SETTINGS, lang);
    default: return "?";
    }
}

static void switcher_refresh(void)
{
    char buf[192];
    snprintf(buf, sizeof(buf), "%s%s\n%s%s\n%s%s\n%s%s\n%s%s\n%s%s",
             s_sw_cursor == 0 ? ">" : " ", mode_name(APP_MODE_GAME),
             s_sw_cursor == 1 ? ">" : " ", mode_name(APP_MODE_POMO),
             s_sw_cursor == 2 ? ">" : " ", mode_name(APP_MODE_CLICKER),
             s_sw_cursor == 3 ? ">" : " ", mode_name(APP_MODE_VOCAB),
             s_sw_cursor == 4 ? ">" : " ", mode_name(APP_MODE_STANDBY),
             s_sw_cursor == 5 ? ">" : " ", mode_name(APP_MODE_SETTINGS));
    lv_label_set_text(s_sw_list, buf);
    const lv_font_t *f = (deskpet_get_lang() == LANG_ZH) ? &zh_subset : &lv_font_montserrat_12;
    lv_obj_set_style_text_font(s_sw_list, f, 0);
    lv_obj_set_style_text_font(s_sw_scr, f, 0);
    static const sprite_id_t POSE[APP_MODE_COUNT] = {
        SPR_FRONT_KNIGHT, SPR_OC_POMO_FOCUS, SPR_OC_CLICKER, SPR_OC_VOCAB_STUDY,
        SPR_OC_STANDBY_SLEEP, SPR_OC_SETTINGS,
    };
    const lv_img_dsc_t *oc = sprite_get(POSE[s_sw_cursor]);
    if (oc) lv_img_set_src(s_sw_oc, oc);
}

static void switcher_open(void)
{
    app_mode_t current = atomic_load(&s_mode);
    s_sw_return = (current == APP_MODE_SWITCHER) ? s_sw_return : current;
    if (current < APP_MODE_COUNT && current != APP_MODE_STANDBY) s_prev = current;
    s_sw_cursor = (int)current < APP_MODE_COUNT ? (int)current : 0;
    if (current < APP_MODE_COUNT) mode_exit_hw(current);
    s_sw_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_sw_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_sw_scr, lv_color_hex(0x0B0B1A), 0);
    lv_obj_set_style_text_color(s_sw_scr, lv_color_hex(0xE8E8F0), 0);
    s_sw_bg = lv_img_create(s_sw_scr);
    const lv_img_dsc_t *bg = sprite_get(SPR_BG_TOWN);
    if (bg) lv_img_set_src(s_sw_bg, bg);
    lv_obj_set_pos(s_sw_bg, 0, 0);
    s_sw_panel = lv_obj_create(s_sw_scr);
    lv_obj_set_size(s_sw_panel, 144, 160);
    lv_obj_set_pos(s_sw_panel, 8, 8);
    lv_obj_remove_flag(s_sw_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_sw_panel, 0, 0);
    lv_obj_set_style_bg_color(s_sw_panel, lv_color_hex(0x101426), 0);
    lv_obj_set_style_bg_opa(s_sw_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_sw_panel, 2, 0);
    lv_obj_set_style_border_color(s_sw_panel, lv_color_hex(0x7AC8D8), 0);
    lv_obj_set_style_text_color(s_sw_panel, lv_color_hex(0xE8E8F0), 0);
    lv_obj_set_style_pad_all(s_sw_panel, 0, 0);
    lv_obj_t *t = lv_label_create(s_sw_panel);
    lv_label_set_text(t, deskpet_tr(S_MODE_SWITCH, deskpet_get_lang()));
    lv_obj_set_pos(t, 8, 8);
    s_sw_list = lv_label_create(s_sw_panel);
    lv_obj_set_width(s_sw_list, 128);
    lv_obj_set_pos(s_sw_list, 8, 38);
    s_sw_oc = lv_img_create(s_sw_scr);
    const lv_img_dsc_t *oc = sprite_get(SPR_FRONT_KNIGHT);
    if (oc) lv_img_set_src(s_sw_oc, oc);
    lv_image_set_antialias(s_sw_oc, false);
    lv_img_set_zoom(s_sw_oc, 512);
    // Menu poses are 64x64 at 2x zoom. Keep the full 128x128 image inside the
    // 240x320 viewport so props and feet are not clipped on the right/bottom.
    lv_obj_set_pos(s_sw_oc, 112, 192);
    s_mode = APP_MODE_SWITCHER;
    lv_screen_load(s_sw_scr);
    switcher_refresh();
    hud_battery_attach(s_sw_scr);
}

static void switcher_close(void)
{
    hud_battery_detach();
    if (s_sw_scr) {
        lv_obj_delete(s_sw_scr);
        s_sw_scr = NULL;
        s_sw_bg = NULL;
        s_sw_panel = NULL;
        s_sw_list = NULL;
        s_sw_oc = NULL;
    }
}

void app_shell_enter(app_mode_t mode)
{
    if (mode == APP_MODE_SWITCHER) {
        switcher_open();
        return;
    }
    if (mode >= APP_MODE_COUNT) return;
    app_mode_t current = atomic_load(&s_mode);
    if (mode == APP_MODE_STANDBY && current < APP_MODE_COUNT
        && current != APP_MODE_STANDBY)
        s_prev = current;
    if (current == APP_MODE_SWITCHER) switcher_close();
    else mode_exit_hw(current);
    // s_prev (standby wake target) was captured at switcher_open; keep it.
    s_mode = mode;
    if (mode != APP_MODE_STANDBY && mode != APP_MODE_SETTINGS) persist_mode();
    mode_enter_hw(mode);
}

app_mode_t app_shell_mode(void) { return atomic_load(&s_mode); }

void app_shell_sleep_prepare(void)
{
    // persist_mode() would store STANDBY, which load_persisted() skips, so
    // store s_prev directly: after a deep-sleep reboot the pet resumes the
    // mode it was in before standby.
    nvs_handle_t h;
    if (nvs_open("deskpet", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "mode", (uint8_t)s_prev);
    nvs_commit(h);
    nvs_close(h);
}

void app_shell_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    app_mode_t current = atomic_load(&s_mode);
    audio_idle_feed();
    if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        // Global rule: OK-long always opens the switcher.
        if (current == APP_MODE_SWITCHER) {
            audio_se(CT_SE_BACK);
            app_shell_enter(s_sw_return);
        } else {
            audio_se(CT_SE_OK);
            app_shell_enter(APP_MODE_SWITCHER);
        }
        return;
    }
    if (current == APP_MODE_SWITCHER) {
        if (ev != BSP_BTN_CLICK) return;
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            int d = (btn == BSP_BTN_UP) ? -1 : 1;
            s_sw_cursor = (s_sw_cursor + d + APP_MODE_COUNT) % APP_MODE_COUNT;
            audio_se(CT_SE_OK);
            switcher_refresh();
        } else if (btn == BSP_BTN_OK) {
            audio_se(CT_SE_OK);
            app_shell_enter((app_mode_t)s_sw_cursor);
        }
        return;
    }
    if (current == APP_MODE_STANDBY) {
        mode_standby_activity();  // any input defers the deep-sleep timer
        if (mode_standby_browse(btn, ev)) {
            audio_se(CT_SE_OK);
            return;  // browsing agents, stay
        }
        if (ev == BSP_BTN_CLICK) {
            audio_se(CT_SE_OK);
            app_shell_enter(s_prev);  // else wake
        }
        return;
    }
    switch (current) {
    case APP_MODE_GAME: demo_deskpet_key(btn, ev); break;
    case APP_MODE_POMO: mode_pomo_key(btn, ev); break;
    case APP_MODE_CLICKER: mode_clicker_key(btn, ev); break;
    case APP_MODE_VOCAB: mode_vocab_key(btn, ev); break;
    case APP_MODE_SETTINGS: mode_settings_key(btn, ev); break;
    default: break;
    }
}

void app_shell_boot(void)
{
    pet_save_t pet;
    pomo_t pomo;
    dungeon_save_t dun;
    deskpet_lang_t lang = LANG_EN;
    if (deskpet_store_load(&pet, &dun, &pomo, &lang) != ESP_OK) lang = LANG_EN;
    deskpet_set_lang(lang);
    load_persisted();
    app_mode_t current = atomic_load(&s_mode);
    ESP_LOGI(TAG, "boot mode=%d", (int)current);
    mode_enter_hw(current);
}
