/*******************************************************************************
 * Size: 16 px
 * Bpp: 4
 * Opts: --font /Library/Fonts/Arial Unicode.ttf --size 16 --bpp 4 --format lvgl --lv-font-name zh_ipa_fix --no-kerning --no-compress -r 0x252,0x25C,0x28C,0x292,0x2CC,0x2D0 -o /Users/chris/workspace/moyu-badge/main/fonts/zh_ipa_fix.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef ZH_IPA_FIX
#define ZH_IPA_FIX 1
#endif

#if ZH_IPA_FIX

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0252 "ɒ" */
    0xe9, 0xaf, 0xe8, 0x0, 0xef, 0xa4, 0x7f, 0x70,
    0xed, 0x0, 0x8, 0xf0, 0xe8, 0x0, 0x3, 0xf3,
    0xe6, 0x0, 0x2, 0xf4, 0xe8, 0x0, 0x3, 0xf3,
    0xec, 0x0, 0x8, 0xf0, 0xef, 0xa4, 0x8f, 0x70,
    0xe9, 0xaf, 0xe7, 0x0,

    /* U+025C "ɜ" */
    0x5, 0xcf, 0xeb, 0x20, 0x1e, 0x83, 0x4c, 0xe0,
    0x0, 0x0, 0x4, 0xf2, 0x0, 0x0, 0x1b, 0xd0,
    0x0, 0x6, 0xfe, 0x20, 0x0, 0x1, 0x4c, 0xd0,
    0x0, 0x0, 0x4, 0xf2, 0x2f, 0x73, 0x4c, 0xe0,
    0x5, 0xdf, 0xea, 0x20,

    /* U+028C "ʌ" */
    0x0, 0xd, 0xd0, 0x0, 0x0, 0x2f, 0xf3, 0x0,
    0x0, 0x8c, 0xc8, 0x0, 0x0, 0xd8, 0x7e, 0x0,
    0x3, 0xf2, 0x2f, 0x30, 0x9, 0xd0, 0xc, 0x90,
    0xe, 0x80, 0x7, 0xe0, 0x4f, 0x30, 0x2, 0xf4,
    0x9d, 0x0, 0x0, 0xca,

    /* U+0292 "ʒ" */
    0x6f, 0xff, 0xff, 0xf6, 0x13, 0x33, 0x3c, 0xe1,
    0x0, 0x0, 0x7f, 0x30, 0x0, 0x4, 0xf5, 0x0,
    0x0, 0x2e, 0x90, 0x0, 0x0, 0xdf, 0xda, 0x20,
    0x0, 0x12, 0x4a, 0xf3, 0x0, 0x0, 0x0, 0xcc,
    0x2, 0x0, 0x0, 0x9d, 0x6e, 0x10, 0x0, 0xcb,
    0x1e, 0xd5, 0x49, 0xf4, 0x1, 0x9e, 0xfc, 0x40,

    /* U+02CC "ˌ" */
    0x80, 0xf0, 0xf0, 0xf0,

    /* U+02D0 "ː" */
    0x6c, 0x31, 0xd0, 0x1, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0xb0, 0x7f, 0x30
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 142, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 124, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 128, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 108, .adv_w = 136, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 156, .adv_w = 49, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -4},
    {.bitmap_index = 160, .adv_w = 77, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xa, 0x3a, 0x40, 0x7a, 0x7e
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 594, .range_length = 127, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 6, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t zh_ipa_fix = {
#else
lv_font_t zh_ipa_fix = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 4,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if ZH_IPA_FIX*/

