// main/hud_battery.c —— classic battery + percent, mode-switcher only.
// Lives on the switcher screen (not lv_layer_top) so game / pomo / standby
// stay clean. Digits use montserrat_14, same face as the standby clock.
#include "hud_battery.h"
#include "bsp_battery.h"

#include "lvgl.h"
#include <stdio.h>

#define TXT_W 42
#define GAP 4
#define BODY_W 22
#define BODY_H 12
#define NIB_W 2
#define NIB_H 6
#define BAR_N 4
#define BAR_W 3
#define BAR_H 6
#define BAR_GAP 1
#define HUD_W (TXT_W + GAP + BODY_W + NIB_W)
#define HUD_H 16
#define HUD_X (240 - 8 - HUD_W)
#define HUD_Y 10
#define LOW_SOC 20

static lv_obj_t *s_root;
static lv_obj_t *s_txt;
static lv_obj_t *s_body;
static lv_obj_t *s_nib;
static lv_obj_t *s_bar[BAR_N];
static lv_timer_t *s_timer;
static int s_last = -2;

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

static void paint(int soc)
{
    if (soc == s_last) return;
    s_last = soc;

    char buf[8];
    uint32_t ink = 0xE8E8F0;
    uint32_t fill = 0x79D6A5;
    int lit = 0;
    if (soc < 0) {
        snprintf(buf, sizeof(buf), "--");
        fill = 0x4A5878;
    } else {
        if (soc > 100) soc = 100;
        snprintf(buf, sizeof(buf), "%d%%", soc);
        lit = (soc + 24) / 25;
        if (lit > BAR_N) lit = BAR_N;
        if (soc == 0) lit = 0;
        if (soc <= LOW_SOC) {
            ink = 0xE06C75;
            fill = 0xE06C75;
        } else if (soc <= 40) {
            fill = 0xF2C66D;
        }
    }
    lv_label_set_text(s_txt, buf);
    lv_obj_set_style_text_color(s_txt, lv_color_hex(ink), 0);
    lv_obj_set_style_border_color(s_body, lv_color_hex(ink), 0);
    lv_obj_set_style_bg_color(s_nib, lv_color_hex(ink), 0);
    for (int i = 0; i < BAR_N; i++) {
        uint32_t c = (i < lit) ? fill : 0x26304A;
        lv_obj_set_style_bg_color(s_bar[i], lv_color_hex(c), 0);
    }
}

static void tick(lv_timer_t *t)
{
    (void)t;
    if (!s_root) return;
    paint(bsp_battery_soc());
}

void hud_battery_detach(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
    s_txt = s_body = s_nib = NULL;
    for (int i = 0; i < BAR_N; i++) s_bar[i] = NULL;
    s_last = -2;
}

void hud_battery_attach(lv_obj_t *parent)
{
    if (!parent) return;
    if (s_root && lv_obj_get_parent(s_root) == parent) {
        paint(bsp_battery_soc());
        return;
    }
    hud_battery_detach();

    s_root = lv_obj_create(parent);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_root, HUD_W, HUD_H);
    lv_obj_set_pos(s_root, HUD_X, HUD_Y);
    lv_obj_set_style_radius(s_root, 0, 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    s_txt = lv_label_create(s_root);
    lv_obj_set_style_text_font(s_txt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_txt, lv_color_hex(0xE8E8F0), 0);
    lv_obj_set_style_text_align(s_txt, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(s_txt, TXT_W);
    lv_obj_set_pos(s_txt, 0, 0);

    int bx = TXT_W + GAP;
    s_body = lv_obj_create(s_root);
    lv_obj_remove_flag(s_body, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_body, BODY_W, BODY_H);
    lv_obj_set_pos(s_body, bx, (HUD_H - BODY_H) / 2);
    lv_obj_set_style_radius(s_body, 0, 0);
    lv_obj_set_style_bg_color(s_body, lv_color_hex(0x101426), 0);
    lv_obj_set_style_bg_opa(s_body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_body, 1, 0);
    lv_obj_set_style_border_color(s_body, lv_color_hex(0xE8E8F0), 0);
    lv_obj_set_style_pad_all(s_body, 0, 0);

    int inner_x = 2;
    int inner_y = (BODY_H - BAR_H) / 2;
    for (int i = 0; i < BAR_N; i++) {
        s_bar[i] = block(s_body, inner_x + i * (BAR_W + BAR_GAP), inner_y,
                         BAR_W, BAR_H, 0x26304A);
    }
    s_nib = block(s_root, bx + BODY_W, (HUD_H - NIB_H) / 2, NIB_W, NIB_H, 0xE8E8F0);

    s_timer = lv_timer_create(tick, 5000, NULL);
    paint(bsp_battery_soc());
}
