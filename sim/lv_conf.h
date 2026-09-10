// sim/lv_conf.h —— LVGL 9.5 PC config (SDL backend). Build with
// -DLV_CONF_INCLUDE_SIMPLE and -I sim/ so lvgl picks this file up.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_SDL 1
#define LV_SDL_BUF_COUNT 2
// Software rendering also works with SDL_VIDEODRIVER=dummy, making scripted
// screenshot acceptance runnable on CI and headless agent sessions.
#define LV_SDL_ACCELERATED 0

#define LV_COLOR_DEPTH 16
#define LV_MEM_SIZE (128U * 1024U)

#define LV_DEF_REFR_PERIOD 33
#define LV_USE_OS LV_OS_NONE

#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_DEFAULT &lv_font_montserrat_12

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#define LV_USE_LABEL 1
#define LV_USE_TIMER 1
#define LV_USE_OBJ 1
#define LV_USE_SNAPSHOT 1

#endif
