// main/mode_standby.c —— standby shows Beijing wall clock / top agent at
// first (dim), then deep-sleeps after 30s idle when no USB host is attached.
// UP/DOWN browse agent slots (no wake while agents present); OK wakes.
#include "mode_standby.h"
#include "agent_status.h"
#include "app_shell.h"
#include "deskpet_i18n.h"
#include "pet_model.h"
#include "pet_sleep.h"
#include "sprite.h"

#include "lvgl.h"
#include <stdio.h>
#include <time.h>

#define STANDBY_SLEEP_MS 30000  // idle on battery -> deep sleep (any key wakes)

static lv_obj_t *s_scr;
static lv_obj_t *s_clock, *s_face, *s_agent, *s_bar, *s_card;
static lv_timer_t *s_timer;
static uint32_t s_tick;
static int s_browse;  // -1 = follow top priority
static int32_t s_last_activity;

extern const lv_font_t zh_subset;

static deskpet_str_id_t state_zh(agent_state_t st)
{
    switch (st) {
    case AST_WORKING: return S_AG_WORKING;
    case AST_NEEDS_YOU: return S_AG_NEEDS;
    case AST_REVIEW: return S_AG_REVIEW;
    case AST_FAILED: return S_AG_FAILED;
    default: return S_AG_CELEBRATE;
    }
}

static sprite_id_t face_for(agent_state_t st, uint32_t tick)
{
    switch (st) {
    case AST_WORKING: return SPR_AG_WORKING;
    case AST_NEEDS_YOU: return (tick % 2) ? SPR_AG_NEEDS0 : SPR_AG_NEEDS1;
    case AST_REVIEW: return SPR_AG_REVIEW;
    case AST_FAILED: return SPR_AG_FAILED;
    default: return SPR_AG_CELEBRATE;
    }
}

static void refresh(void)
{
    char buf[96];  // 64 triggers -Werror=format-truncation on device
    int show = (s_browse >= 0) ? s_browse : agent_status_top();
    agent_slot_t snapshot;
    bool has_agent = show >= 0 && agent_status_get_copy((uint8_t)show, &snapshot);
    const agent_slot_t *a = has_agent ? &snapshot : NULL;
    if (!a || !a->used || a->state == AST_IDLE) {
        time_t t = time(NULL);
        struct tm bt;
        localtime_r(&t, &bt);  // TZ fixed to Beijing by deskpet_time_init()
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", bt.tm_hour, bt.tm_min, bt.tm_sec);
        lv_label_set_text(s_clock, buf);
        lv_obj_clear_flag(s_clock, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_agent, "");
        const lv_img_dsc_t *dsc = sprite_get(SPR_OC_STANDBY_SLEEP);
        if (dsc) lv_img_set_src(s_face, dsc);
        lv_img_set_zoom(s_face, 512);
        lv_obj_set_pos(s_face, 104, 210);
        lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_border_color(s_card, lv_color_hex(0x4A5878), 0);
    } else {
        lv_obj_add_flag(s_clock, LV_OBJ_FLAG_HIDDEN);
        char head[48];
        if (deskpet_get_lang() == LANG_ZH)
            snprintf(head, sizeof(head), "%s %s", a->name, deskpet_tr(state_zh(a->state), LANG_ZH));
        else
            snprintf(head, sizeof(head), "%s %s", a->name, agent_state_name_en(a->state));
        snprintf(buf, sizeof(buf), "%s\n%s", head, a->text);
        lv_label_set_text(s_agent, buf);
        const lv_img_dsc_t *dsc = sprite_get(face_for(a->state, s_tick));
        if (dsc) lv_img_set_src(s_face, dsc);
        lv_img_set_zoom(s_face, 1024);  // 25x31 faces -> 100x124 medallion
        lv_obj_set_pos(s_face, 108, 210);
        lv_color_t accent = lv_color_hex(0x7AC8D8);
        if (a->state == AST_NEEDS_YOU) accent = lv_color_hex(0xF2C66D);
        else if (a->state == AST_FAILED) accent = lv_color_hex(0xE06C75);
        else if (a->state == AST_REVIEW) accent = lv_color_hex(0xB38BE8);
        else if (a->state == AST_CELEBRATE) accent = lv_color_hex(0x79D6A5);
        lv_obj_set_style_border_color(s_card, accent, 0);
        if (a->progress <= 100) {
            lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(s_bar, a->progress, LV_ANIM_OFF);
        } else {
            lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        }
    }
    const lv_font_t *f = (deskpet_get_lang() == LANG_ZH) ? &zh_subset : &lv_font_montserrat_14;
    lv_obj_t *objs[] = {s_clock, s_agent};
    for (unsigned i = 0; i < 2; i++) lv_obj_set_style_text_font(objs[i], f, 0);
}

static void tick(lv_timer_t *t)
{
    (void)t;
    s_tick++;
    if (lv_tick_get() - s_last_activity >= STANDBY_SLEEP_MS && !pet_sleep_powered()) {
        // Deep sleep reboots the chip: persist the wake target first, then
        // sleep. The waking button press pulls GPIO0 low in deep sleep, the
        // chip boots, deskpet_time_init() replays sleep-elapsed into the
        // clock, and load_persisted lands on the pre-standby mode.
        app_shell_sleep_prepare();
        pet_sleep_now();
    }
    if (s_browse >= 0) {
        agent_slot_t a;
        if (!agent_status_get_copy((uint8_t)s_browse, &a) ||
            !a.used || a.state == AST_IDLE)
            s_browse = -1;  // slot went idle
    }
    refresh();
}

void mode_standby_activity(void)
{
    s_last_activity = lv_tick_get();
}

void mode_standby_enter(void)
{
    s_tick = 0;
    s_browse = -1;
    s_last_activity = lv_tick_get();
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x050508), 0);
    lv_obj_set_style_text_color(s_scr, lv_color_hex(0x8A8A99), 0);
    s_card = lv_obj_create(s_scr);
    lv_obj_set_size(s_card, 224, 136);
    lv_obj_set_pos(s_card, 8, 8);
    lv_obj_remove_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_card, 0, 0);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(0x101426), 0);
    lv_obj_set_style_border_width(s_card, 2, 0);
    lv_obj_set_style_border_color(s_card, lv_color_hex(0x4A5878), 0);
    s_clock = lv_label_create(s_scr);
    lv_obj_set_pos(s_clock, 66, 56);
    s_agent = lv_label_create(s_scr);
    lv_obj_set_width(s_agent, 230);
    lv_obj_set_pos(s_agent, 16, 28);
    s_bar = lv_bar_create(s_scr);
    lv_obj_set_size(s_bar, 208, 10);
    lv_obj_set_pos(s_bar, 16, 112);
    lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    s_face = lv_img_create(s_scr);
    const lv_img_dsc_t *dsc = sprite_get(SPR_OC_STANDBY_SLEEP);
    if (dsc) lv_img_set_src(s_face, dsc);
    lv_image_set_antialias(s_face, false);
    lv_img_set_zoom(s_face, 512);
    lv_obj_set_pos(s_face, 104, 210);
    s_timer = lv_timer_create(tick, 1000, NULL);
    lv_screen_load(s_scr);
    refresh();
}

void mode_standby_exit(void)
{
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_clock = s_agent = s_face = s_bar = s_card = NULL;
    }
}

void mode_standby_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    (void)btn;
    (void)ev;
    // shell wakes on non-browse clicks; nothing to do here.
}

bool mode_standby_browse(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return false;
    if (btn != BSP_BTN_UP && btn != BSP_BTN_DOWN) return false;
    if (agent_status_top() < 0) return false;  // no agents: wake as usual
    int cur = (s_browse >= 0) ? s_browse : agent_status_top();
    if (btn == BSP_BTN_UP) {
        // step backward: advance 3 times in a 4-slot ring
        cur = agent_status_next(cur);
        cur = agent_status_next(cur);
        cur = agent_status_next(cur);
    } else {
        cur = agent_status_next(cur);
    }
    s_browse = cur;
    refresh();
    return true;
}
