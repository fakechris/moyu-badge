// Decode one full-color frame from a compact shared-palette SP4 animation pack.
#include "sprite_anim.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JOBS 5u
#define WALK_FRAMES 8u
#define ATTACK_FRAMES 7u
#define TOTAL_FRAMES (WALK_FRAMES + ATTACK_FRAMES)
#define ATTACK_STEPS 6u
#define W 64u
#define H 64u

// Curated attack sequences (packed frame indices; 0 = walk frame 0 which is
// the stance). The current full-sheet families share the same authored phases:
// ready, anticipation, connected smear, contact, recovery, then stance.
static const uint8_t ATTACK_SEQ[JOBS][ATTACK_STEPS] = {
    {8, 9, 11, 12, 14, 0},    // knight: windup, slash, contact, recovery
    {8, 9, 11, 12, 14, 0},    // black: charge, cast ribbon, release, recovery
    {8, 9, 11, 12, 14, 0},    // white: gather, halo, contact, recovery
    {8, 9, 11, 12, 14, 0},    // thief: coil, crossing smear, cut, recovery
    {8, 9, 11, 12, 14, 0},    // dark: raise, scythe smear, sweep, recovery
};
#define HEADER_BYTES 8u
#define PALETTE_BYTES 64u
#define FRAME_BYTES (W * H / 2u)
#define PACK_BYTES (HEADER_BYTES + PALETTE_BYTES + TOTAL_FRAMES * FRAME_BYTES)

#ifdef ESP_PLATFORM
#define DECL_PACK(name) \
    extern const uint8_t _binary_anim_##name##_spr4_start[];
DECL_PACK(knight)
DECL_PACK(black)
DECL_PACK(white)
DECL_PACK(thief)
DECL_PACK(dark)

static const uint8_t *pack_ptr(uint8_t job)
{
    static const uint8_t *const packs[JOBS] = {
        _binary_anim_knight_spr4_start,
        _binary_anim_black_spr4_start,
        _binary_anim_white_spr4_start,
        _binary_anim_thief_spr4_start,
        _binary_anim_dark_spr4_start,
    };
    return job < JOBS ? packs[job] : packs[0];
}
#else
static uint8_t *s_packs[JOBS];

static const char *sim_flavor(void)
{
    const char *flavor = getenv("DESKPET_ART_FLAVOR");
    return (flavor && flavor[0]) ? flavor : "generic";
}

static const uint8_t *pack_ptr(uint8_t job)
{
    if (job >= JOBS) job = 0;
    if (s_packs[job]) return s_packs[job];
    static const char *const names[JOBS] = {
        "anim_knight.spr4", "anim_black.spr4", "anim_white.spr4",
        "anim_thief.spr4", "anim_dark.spr4",
    };
    const char *root = getenv("DESKPET_ASSETS");
    char path[256];
    if (!root) root = "main/sprites";
    snprintf(path, sizeof(path), "%s/%s/%s", root, sim_flavor(), names[job]);
    FILE *stream = fopen(path, "rb");
    if (!stream) {
        snprintf(path, sizeof(path), "%s/%s", root, names[job]);
        stream = fopen(path, "rb");
    }
    if (!stream) return NULL;
    uint8_t *data = malloc(PACK_BYTES);
    size_t count = data ? fread(data, 1, PACK_BYTES, stream) : 0;
    fclose(stream);
    if (count != PACK_BYTES) {
        free(data);
        return NULL;
    }
    s_packs[job] = data;
    return data;
}
#endif

static uint8_t s_pixels[W * H * 4u];
static lv_img_dsc_t s_dsc;
static int8_t s_last_job = -1;
static int8_t s_last_frame = -1;

uint8_t sprite_anim_frame_count(sprite_anim_action_t action)
{
    return action == SPR_ANIM_ATTACK ? ATTACK_STEPS : WALK_FRAMES;
}

uint8_t sprite_anim_packed_count(void) { return TOTAL_FRAMES; }

const lv_img_dsc_t *sprite_anim_get(uint8_t job, sprite_anim_action_t action,
                                    uint8_t frame)
{
    if (job >= JOBS) job = 0;
    uint8_t count = sprite_anim_frame_count(action);
    if (frame >= count) frame %= count;
    uint8_t packed_frame = action == SPR_ANIM_ATTACK ? ATTACK_SEQ[job][frame] : frame;
    return sprite_anim_get_packed(job, packed_frame);
}

const lv_img_dsc_t *sprite_anim_get_packed(uint8_t job, uint8_t packed_frame)
{
    if (job >= JOBS) job = 0;
    if (packed_frame >= TOTAL_FRAMES) packed_frame = 0;
    if (s_last_job == (int8_t)job && s_last_frame == (int8_t)packed_frame) return &s_dsc;

    const uint8_t *pack = pack_ptr(job);
    if (!pack || memcmp(pack, "SP4\x01", 4) != 0 || pack[4] != W || pack[5] != H
        || pack[6] != TOTAL_FRAMES || pack[7] != 16) return NULL;
    const uint8_t *palette = pack + HEADER_BYTES;
    const uint8_t *indices = palette + PALETTE_BYTES + packed_frame * FRAME_BYTES;
    for (uint32_t i = 0; i < W * H; i++) {
        uint8_t pair = indices[i >> 1];
        uint8_t index = (i & 1u) ? (uint8_t)(pair >> 4) : (uint8_t)(pair & 0x0f);
        // SP4 stores conventional RGBA; LVGL ARGB8888 is BGRA in memory.
        s_pixels[i * 4u] = palette[index * 4u + 2u];
        s_pixels[i * 4u + 1u] = palette[index * 4u + 1u];
        s_pixels[i * 4u + 2u] = palette[index * 4u];
        s_pixels[i * 4u + 3u] = palette[index * 4u + 3u];
    }
    s_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    s_dsc.header.w = W;
    s_dsc.header.h = H;
    s_dsc.header.stride = W * 4u;
    s_dsc.data_size = sizeof(s_pixels);
    s_dsc.data = s_pixels;
    s_last_job = (int8_t)job;
    s_last_frame = (int8_t)packed_frame;
    return &s_dsc;
}
