// main/mode_pomo.c —— OK toggles start/pause/resume; DOWN abandons to idle.
#include "mode_pomo.h"
#include "app_shell.h"
#include "chiptune.h"
#include "deskpet_i18n.h"
#include "deskpet_store.h"
#include "pet_model.h"
#include "dungeon_model.h"
#include "pomo_lite.h"
#include "sprite.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdio.h>

static const char *TAG = "mode_pomo";

// Standby rule for pomo: a RUNNING session never auto-leaves (long dwell).
// Only after a full cycle completes (BREAK_DONE -> IDLE) plus no keys for
// POMO_DONE_IDLE_MS does the mode step aside to standby.
#define POMO_DONE_IDLE_MS 120000u

static lv_obj_t *s_scr;
static lv_obj_t *s_time, *s_state, *s_count, *s_bar, *s_oc, *s_title, *s_hint;
static lv_timer_t *s_timer;
static pomo_t s_pomo;
static uint64_t s_done_ms;      // last full-cycle completion, 0 = none/armed-off
static uint64_t s_last_key_ms;

extern const lv_font_t zh_subset;

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

static const char *state_text(pomo_state_t st, char *tmp, size_t n)
{
    deskpet_lang_t lang = deskpet_get_lang();
    const char *s = "?";
    if (lang == LANG_ZH) {
        switch (st) {
        case POMO_IDLE: s = "空闲"; break;
        case POMO_RUNNING: s = "专注中"; break;
        case POMO_PAUSED: s = "已暂停"; break;
        case POMO_REWARD: s = "奖励"; break;
        case POMO_BREAK: s = "休息"; break;
        }
        snprintf(tmp, n, "%s", s);
    } else {
        switch (st) {
        case POMO_IDLE: s = "IDLE"; break;
        case POMO_RUNNING: s = "FOCUS"; break;
        case POMO_PAUSED: s = "PAUSED"; break;
        case POMO_REWARD: s = "REWARD"; break;
        case POMO_BREAK: s = "BREAK"; break;
        }
        snprintf(tmp, n, "%s", s);
    }
    return tmp;
}

static void persist(void)
{
    // Read-modify-write of the other blobs; missing ones fall back to
    // defaults so a pomo session is never lost just because the game has
    // not saved yet.
    pet_save_t pet;
    dungeon_save_t dun;
    pomo_t old;
    deskpet_lang_t lang = deskpet_get_lang();
    pet_save_defaults(&pet);
    dungeon_new_game(&dun, (uint32_t)now_ms());
    deskpet_store_load(&pet, &dun, &old, &lang);
    deskpet_store_save(&pet, &dun, &s_pomo, lang);
}

static void refresh(void)
{
    char buf[32];
    uint32_t total = (s_pomo.state == POMO_BREAK) ? POMO_BREAK_MS : POMO_FOCUS_MS;
    uint32_t left = s_pomo.remaining_ms;
    if (s_pomo.state == POMO_IDLE) left = total;
    snprintf(buf, sizeof(buf), "%02u:%02u",
             (unsigned)(left / 60000), (unsigned)((left / 1000) % 60));
    lv_label_set_text(s_time, buf);
    char st[24];
    lv_label_set_text(s_state, state_text(s_pomo.state, st, sizeof st));
    deskpet_str_id_t action = S_START;
    if (s_pomo.state == POMO_RUNNING) action = S_PAUSE;
    else if (s_pomo.state == POMO_PAUSED) action = S_RESUME;
    else if (s_pomo.state == POMO_REWARD || s_pomo.state == POMO_BREAK) action = S_GIVE_UP;
    snprintf(buf, sizeof(buf), "OK %s  DN %s",
             deskpet_tr(action, deskpet_get_lang()),
             deskpet_tr(S_GIVE_UP, deskpet_get_lang()));
    lv_label_set_text(s_hint, buf);
    snprintf(buf, sizeof(buf), "x%u", (unsigned)s_pomo.completed);
    lv_label_set_text(s_count, buf);
    lv_bar_set_value(s_bar, total ? (int)((total - left) * 100 / total) : 0, LV_ANIM_OFF);
    const lv_img_dsc_t *oc = sprite_get(
        (s_pomo.state == POMO_REWARD || s_pomo.state == POMO_BREAK)
            ? SPR_OC_POMO_REWARD : SPR_OC_POMO_FOCUS);
    if (oc) lv_img_set_src(s_oc, oc);
    const lv_font_t *f = (deskpet_get_lang() == LANG_ZH) ? &zh_subset : &lv_font_montserrat_14;
    lv_obj_set_style_text_font(s_time, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(s_state, f, 0);
    lv_obj_set_style_text_font(s_count, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(s_title, f, 0);
    lv_obj_set_style_text_font(s_hint, f, 0);
}

static void standby_async(void *user)
{
    (void)user;
    app_shell_enter(APP_MODE_STANDBY);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    uint64_t now = now_ms();
    pomo_event_t e = pomo_tick(&s_pomo, now);
    if (e != POMO_EV_NONE) {
        persist();
        ESP_LOGI(TAG, "pomo event %d completed=%u", (int)e, (unsigned)s_pomo.completed);
        if (e == POMO_EV_FOCUS_DONE) audio_se(CT_SE_POMO_FOCUS);
        if (e == POMO_EV_BREAK_DONE) {
            audio_se(CT_SE_POMO_BREAK);
            s_done_ms = now;
        }
    }
    if (s_done_ms && s_pomo.state == POMO_IDLE
        && now - s_done_ms > POMO_DONE_IDLE_MS
        && now - s_last_key_ms > POMO_DONE_IDLE_MS) {
        s_done_ms = 0;  // one-shot
        // Async: mode switch deletes this timer; never do it synchronously
        // inside its own callback.
        lv_async_call(standby_async, NULL);
        return;
    }
    refresh();
}

void mode_pomo_enter(void)
{
    pet_save_t pet;
    dungeon_save_t dun;
    deskpet_lang_t lang;
    pomo_defaults(&s_pomo);
    deskpet_store_load(&pet, &dun, &s_pomo, &lang);
    pomo_rebase(&s_pomo);   // deadlines are boot-relative: resume paused, never mid-air
    s_done_ms = 0;
    s_last_key_ms = now_ms();
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
    lv_obj_set_style_border_color(panel, lv_color_hex(0x7AC8D8), 0);
    s_title = lv_label_create(s_scr);
    lv_label_set_text(s_title, deskpet_tr(S_MODE_POMO, deskpet_get_lang()));
    lv_obj_set_pos(s_title, 16, 16);
    s_time = lv_label_create(s_scr);
    lv_obj_set_pos(s_time, 82, 54);
    s_state = lv_label_create(s_scr);
    lv_obj_set_pos(s_state, 16, 96);
    s_count = lv_label_create(s_scr);
    lv_obj_set_pos(s_count, 190, 16);
    s_bar = lv_bar_create(s_scr);
    lv_obj_set_size(s_bar, 208, 14);
    lv_obj_set_pos(s_bar, 16, 128);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x26304A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0x7AC8D8), LV_PART_INDICATOR);
    s_hint = lv_label_create(s_scr);
    lv_obj_set_pos(s_hint, 16, 168);
    s_oc = lv_img_create(s_scr);
    const lv_img_dsc_t *oc = sprite_get(SPR_OC_POMO_FOCUS);
    if (oc) lv_img_set_src(s_oc, oc);
    lv_image_set_antialias(s_oc, false);
    lv_img_set_zoom(s_oc, 512);
    lv_obj_set_pos(s_oc, 104, 216);
    s_timer = lv_timer_create(tick, 500, NULL);
    lv_screen_load(s_scr);
    refresh();
}

void mode_pomo_exit(void)
{
    persist();
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_time = s_state = s_count = s_bar = s_oc = s_title = s_hint = NULL;
    }
}

void mode_pomo_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    s_last_key_ms = now_ms();
    s_done_ms = 0;  // any key re-arms (user is back)
    uint64_t now = now_ms();
    pomo_state_t before = s_pomo.state;
    if (btn == BSP_BTN_OK) {
        if (s_pomo.state == POMO_IDLE) pomo_start(&s_pomo, now);
        else if (s_pomo.state == POMO_RUNNING) pomo_pause(&s_pomo, now);
        else if (s_pomo.state == POMO_PAUSED) pomo_resume(&s_pomo, now);
        else pomo_abandon(&s_pomo);
        persist();
        audio_se(s_pomo.state != before ? CT_SE_OK : CT_SE_WARN);
    } else if (btn == BSP_BTN_DOWN) {
        pomo_abandon(&s_pomo);
        persist();
        audio_se(before == POMO_IDLE ? CT_SE_WARN : CT_SE_BACK);
    }
    refresh();
    (void)TAG;
}
