// Full 8-frame walk / 7-frame attack decoder for 4-bit packed hero art.
#pragma once

#include "lvgl.h"
#include <stdint.h>

typedef enum {
    SPR_ANIM_WALK = 0,
    SPR_ANIM_ATTACK,
} sprite_anim_action_t;

// Walk: 8 packed frames in order. Attack: a curated 6-step sequence per job
// (windup / strike / recover / back to stance) picked from the 7 packed
// attack frames — the generated frames re-pose the character every frame, so
// playing all seven in order reads as flicker.
uint8_t sprite_anim_frame_count(sprite_anim_action_t action);
const lv_img_dsc_t *sprite_anim_get(uint8_t job, sprite_anim_action_t action,
                                    uint8_t frame);
// Raw access to packed frame 0..14 (tests / tools).
const lv_img_dsc_t *sprite_anim_get_packed(uint8_t job, uint8_t packed_frame);
uint8_t sprite_anim_packed_count(void);
