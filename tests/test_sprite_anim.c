#include "sprite_anim.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JOBS 5
#define WIDTH 64
#define HEIGHT 64
#define HEADER 8
#define PALETTE 64
#define FRAME_BYTES (WIDTH * HEIGHT / 2)

int main(void)
{
    static const char *const names[JOBS] = {
        "knight", "black", "white", "thief", "dark",
    };
    const char *flavor = getenv("DESKPET_ART_FLAVOR");
    if (!flavor || !flavor[0]) flavor = "generic";
    uint8_t pack[HEADER + PALETTE + 15 * FRAME_BYTES];
    for (int job = 0; job < JOBS; job++) {
        char path[160];
        snprintf(path, sizeof(path), "main/sprites/%s/anim_%s.spr4",
                 flavor, names[job]);
        FILE *file = fopen(path, "rb");
        if (!file) {
            snprintf(path, sizeof(path), "main/sprites/anim_%s.spr4", names[job]);
            file = fopen(path, "rb");
        }
        assert(file);
        assert(fread(pack, 1, sizeof(pack), file) == sizeof(pack));
        fclose(file);
        for (int packed_frame = 0; packed_frame < 15; packed_frame++) {
            const lv_img_dsc_t *image = sprite_anim_get_packed((uint8_t)job, (uint8_t)packed_frame);
            assert(image && image->header.cf == LV_COLOR_FORMAT_ARGB8888);
            const uint8_t *indices = pack + HEADER + PALETTE + packed_frame * FRAME_BYTES;
            for (int pixel = 0; pixel < WIDTH * HEIGHT; pixel++) {
                uint8_t pair = indices[pixel >> 1];
                uint8_t index = (pixel & 1) ? (uint8_t)(pair >> 4) : (uint8_t)(pair & 15);
                const uint8_t *rgba = pack + HEADER + index * 4;
                const uint8_t *bgra = image->data + pixel * 4;
                assert(bgra[0] == rgba[2]);
                assert(bgra[1] == rgba[1]);
                assert(bgra[2] == rgba[0]);
                assert(bgra[3] == rgba[3]);
            }
        }
    }
    // Curated attack sequence: starts on an attack frame, ends on the stance,
    // walk plays all 8 packed frames in order.
    for (int job = 0; job < JOBS; job++) {
        assert(sprite_anim_frame_count(SPR_ANIM_ATTACK) == 6);
        const lv_img_dsc_t *first = sprite_anim_get((uint8_t)job, SPR_ANIM_ATTACK, 0);
        const lv_img_dsc_t *raw8 = sprite_anim_get_packed((uint8_t)job, 8);
        assert(first == raw8);   // same decode slot: sequence[0] == packed 8
        for (uint8_t f = 0; f < 8; f++)
            assert(sprite_anim_get((uint8_t)job, SPR_ANIM_WALK, f)
                   == sprite_anim_get_packed((uint8_t)job, f));
    }
    puts("ALL SPRITE ANIMATION COLORS PASS");
    return 0;
}
