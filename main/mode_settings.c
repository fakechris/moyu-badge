// main/mode_settings.c —— UP/DOWN pick a row, OK toggles it. OK-long (global)
// leaves. Rows: mute all / game sound / pomodoro sound / language.
#include "mode_settings.h"
#include "audio_task.h"
#include "chiptune.h"
#include "deskpet_i18n.h"
#include "sprite.h"
#include "clicker_hid.h"

#include "lvgl.h"
#include "nvs.h"
#include <stdio.h>

extern const lv_font_t zh_subset;

static lv_obj_t *s_scr, *s_title, *s_list, *s_hint, *s_oc;
static int s_cursor;
static audio_prefs_t s_prefs;

#define ROWS 8
static uint8_t s_vbgm, s_vse;
static uint8_t vol_step(uint8_t v) { return (uint8_t)(v >= 100 ? 0 : (v / 20 + 1) * 20); }

static void persist_lang(deskpet_lang_t lang)
{
    // Same key the game store uses ("deskpet"/"lang"), so both agree.
    nvs_handle_t h;
    if (nvs_open("deskpet", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "lang", (uint8_t)lang);
    nvs_commit(h);
    nvs_close(h);
}

static void refresh(void)
{
    deskpet_lang_t lang = deskpet_get_lang();
    const char *on = deskpet_tr(S_ON, lang), *off = deskpet_tr(S_OFF, lang);
    char buf[260];
    snprintf(buf, sizeof(buf), "%s%s: %s\n%s%s: %s\n%s%s: %s\n%s%s: %s\n%s%s: %u\n%s%s: %u\n%s%s: %s\n%s%s: %s",
             s_cursor == 0 ? ">" : " ", deskpet_tr(S_SET_MUTE_ALL, lang), s_prefs.mute_all ? on : off,
             s_cursor == 1 ? ">" : " ", deskpet_tr(S_SET_POMO_SND, lang), s_prefs.mute_pomo ? on : off,
             s_cursor == 2 ? ">" : " ", deskpet_tr(S_SET_GAME_SND, lang), s_prefs.mute_game ? on : off,
             s_cursor == 3 ? ">" : " ", deskpet_tr(S_SET_OTHER_SND, lang), s_prefs.mute_other ? on : off,
             s_cursor == 4 ? ">" : " ", deskpet_tr(S_SET_BGM_VOL, lang), s_vbgm,
             s_cursor == 5 ? ">" : " ", deskpet_tr(S_SET_SE_VOL, lang), s_vse,
             s_cursor == 6 ? ">" : " ", deskpet_tr(S_LANGUAGE, lang), deskpet_tr(S_LANG_NAME, lang),
             s_cursor == 7 ? ">" : " ", deskpet_tr(S_SET_PAIR, lang),
             clicker_hid_pairing_open() ? deskpet_tr(S_PAIRING_NOW, lang) : "");
    lv_label_set_text(s_list, buf);
    lv_label_set_text(s_title, deskpet_tr(S_SETTINGS, lang));
    lv_label_set_text(s_hint, deskpet_tr(S_SET_HINT, lang));
    // The language row shows both scripts, so the list always uses the CJK
    // subset font (it carries ASCII too).
    lv_obj_set_style_text_font(s_list, &zh_subset, 0);
    const lv_font_t *f = (lang == LANG_ZH) ? &zh_subset : &lv_font_montserrat_14;
    lv_obj_set_style_text_font(s_title, f, 0);
    lv_obj_set_style_text_font(s_hint, f, 0);
}

void mode_settings_enter(void)
{
    audio_prefs_get(&s_prefs);
    audio_get_volumes(&s_vbgm, &s_vse);
    s_cursor = 0;
    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x0B0B1A), 0);
    lv_obj_set_style_text_color(s_scr, lv_color_hex(0xE8E8F0), 0);
    lv_obj_t *panel = lv_obj_create(s_scr);
    lv_obj_set_size(panel, 224, 200);
    lv_obj_set_pos(panel, 8, 8);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x101426), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x7AC8D8), 0);
    s_title = lv_label_create(s_scr);
    lv_obj_set_pos(s_title, 16, 16);
    s_list = lv_label_create(s_scr);
    lv_obj_set_width(s_list, 208);
    lv_obj_set_pos(s_list, 16, 44);
    s_hint = lv_label_create(s_scr);
    lv_obj_set_pos(s_hint, 16, 216);
    s_oc = lv_img_create(s_scr);
    const lv_img_dsc_t *oc = sprite_get(SPR_FRONT_KNIGHT);
    if (oc) lv_img_set_src(s_oc, oc);
    lv_image_set_antialias(s_oc, false);
    lv_img_set_zoom(s_oc, 256);
    lv_obj_set_pos(s_oc, 88, 246);
    lv_screen_load(s_scr);
    refresh();
}

void mode_settings_exit(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_title = s_list = s_hint = s_oc = NULL;
    }
}

void mode_settings_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_cursor = (s_cursor + (btn == BSP_BTN_UP ? -1 : 1) + ROWS) % ROWS;
        audio_se(CT_SE_OK);
    } else if (btn == BSP_BTN_OK) {
        switch (s_cursor) {
        case 0: s_prefs.mute_all = !s_prefs.mute_all; break;
        case 1: s_prefs.mute_pomo = !s_prefs.mute_pomo; break;
        case 2: s_prefs.mute_game = !s_prefs.mute_game; break;
        case 3: s_prefs.mute_other = !s_prefs.mute_other; break;
        case 4: s_vbgm = vol_step(s_vbgm); audio_set_volumes(s_vbgm, s_vse); break;
        case 5: s_vse = vol_step(s_vse); audio_set_volumes(s_vbgm, s_vse); break;
        case 7: clicker_hid_repair(30000); break;   // forget bonds, 30 s window
        default: {
            deskpet_lang_t l = deskpet_get_lang() == LANG_ZH ? LANG_EN : LANG_ZH;
            deskpet_set_lang(l);
            persist_lang(l);
            break;
        }
        }
        if (s_cursor < 4) audio_prefs_set(&s_prefs);
        audio_se(CT_SE_CHOICE);   // audible only if the new state allows it
    }
    refresh();
}
