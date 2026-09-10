// main/mode_vocab.c —— vocabulary flashcard mode: FSRS scheduling + two-key
// rating (UP=forgot, OK=knew). Modes never auto-switch: the done screen
// stays put until the user opens the global OK-long switcher.
#include "mode_vocab.h"
#include "deskpet_i18n.h"
#include "vocab_ui.h"
#include "vocab_data.h"

#include "bsp_display.h"
#include "lvgl.h"

static lv_obj_t *s_scr;

void mode_vocab_enter(void)
{
    bsp_display_backlight(100);
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x1A1A2E), 0);
    vocab_ui_create(s_scr, s_vocab_data, VOCAB_DATA_COUNT);
    lv_screen_load(s_scr);
}

void mode_vocab_exit(void)
{
    vocab_ui_destroy();
    if (s_scr) { lv_obj_del(s_scr); s_scr = NULL; }
}

void mode_vocab_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    // Mode rule: never auto-leave. The done screen just sits there; the only
    // way out is the global OK-long switcher. ONCE IN VOCAB, ALWAYS IN VOCAB.
    if (ev != BSP_BTN_CLICK) return;
    // bsp_btn_t order is UP/DOWN/OK -> 0=忘记, 1=flip/模糊, 2=认识.
    // All three are live ratings (Maimemo-style three-tier).
    vocab_ui_button(btn == BSP_BTN_UP ? 0 : (btn == BSP_BTN_OK ? 1 : 2));
}
