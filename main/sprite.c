// main/sprite.c —— registry + platform data source (embedded flash / sim files).
#include "sprite.h"
#include "pet_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *file;  // exact filename under main/sprites/
    uint16_t w, h;
    lv_color_format_t cf;
} spr_def_t;

#define I4(f, w, h) {f ".i4", w, h, LV_COLOR_FORMAT_I4}
#define A888(f, w, h) {f ".argb8888", w, h, LV_COLOR_FORMAT_ARGB8888}

static const spr_def_t DEFS[SPR_COUNT] = {
    [SPR_IDLE0] = A888("idle0", 25, 31), [SPR_IDLE1] = A888("idle1", 25, 31),
    [SPR_HAPPY0] = A888("happy0", 25, 31), [SPR_HAPPY1] = A888("happy1", 25, 31),
    [SPR_HAPPY2] = A888("happy2", 25, 31),
    [SPR_AG_WORKING] = A888("ag_working", 25, 31),
    [SPR_AG_NEEDS0] = A888("ag_needs0", 25, 31),
    [SPR_AG_NEEDS1] = A888("ag_needs1", 25, 31),
    [SPR_AG_REVIEW] = A888("ag_review", 25, 31),
    [SPR_AG_FAILED] = A888("ag_failed", 25, 31),
    [SPR_AG_CELEBRATE] = A888("ag_celebrate", 25, 31),
    [SPR_HERO_JOB_WHITE] = A888("hero_job_white", 64, 64),
    [SPR_HERO_JOB_BLACK] = A888("hero_job_black", 64, 64),
    [SPR_OC_POMO_FOCUS] = A888("oc_pomo_focus", 64, 64),
    [SPR_OC_POMO_REWARD] = A888("oc_pomo_reward", 64, 64),
    [SPR_OC_CLICKER] = A888("oc_clicker", 64, 64),
    [SPR_OC_STANDBY_SLEEP] = A888("oc_standby_sleep", 64, 64),
    [SPR_SIDE_KNIGHT] = A888("side_knight", 64, 64),
    [SPR_SIDE_BLACK] = A888("side_black", 64, 64),
    [SPR_SIDE_WHITE] = A888("side_white", 64, 64),
    [SPR_SIDE_THIEF] = A888("side_thief", 64, 64),
    [SPR_SIDE_DARK] = A888("side_dark", 64, 64),
    [SPR_FRONT_KNIGHT] = A888("front_knight", 64, 64),
    [SPR_FRONT_THIEF] = A888("front_thief", 64, 64),
    [SPR_FRONT_DARK] = A888("front_dark", 64, 64),
    [SPR_MON_SLIME] = A888("mon_slime", 48, 48),
    [SPR_MON_SLIME_HURT] = A888("mon_slime_hurt", 48, 48),
    [SPR_MON_BAT0] = A888("mon_bat0", 48, 48),
    [SPR_MON_BAT1] = A888("mon_bat1", 48, 48),
    [SPR_MON_BAT_HURT] = A888("mon_bat_hurt", 48, 48),
    [SPR_MON_SKEL] = A888("mon_skel", 48, 48),
    [SPR_MON_SKEL_HURT] = A888("mon_skel_hurt", 48, 48),
    [SPR_MON_GOLEM] = A888("mon_golem", 64, 64),
    [SPR_MON_GOLEM_HURT] = A888("mon_golem_hurt", 64, 64),
    [SPR_MON_MIMIC_CLOSED] = A888("mon_mimic_closed", 64, 64),
    [SPR_MON_MIMIC_OPEN] = A888("mon_mimic_open", 64, 64),
    [SPR_MON_ANCIENT] = A888("mon_ancient", 64, 64),
    [SPR_MON_ANCIENT_HURT] = A888("mon_ancient_hurt", 64, 64),
    [SPR_EVENT_CURSED_GATE] = A888("event_cursed_gate", 64, 64),
    [SPR_EVENT_WISHING_WELL] = A888("event_wishing_well", 32, 32),
    [SPR_EVENT_RARE_SPARKLE] = A888("event_rare_sparkle", 16, 16),
    [SPR_EVENT_STARFALL] = A888("event_starfall", 64, 64),
    [SPR_ULT_CASTLE] = A888("ult_castle", 48, 48),
    [SPR_ULT_METEOR] = A888("ult_meteor", 48, 48),
    [SPR_ULT_SANCTUARY] = A888("ult_sanctuary", 48, 48),
    [SPR_ULT_JACKPOT] = A888("ult_jackpot", 48, 48),
    [SPR_ULT_ECLIPSE] = A888("ult_eclipse", 48, 48),
    [SPR_UI_REBIRTH] = A888("ui_rebirth", 32, 32),
    [SPR_UI_ULT_CASTLE] = A888("ui_ult_castle", 16, 16),
    [SPR_UI_ULT_METEOR] = A888("ui_ult_meteor", 16, 16),
    [SPR_UI_ULT_SANCTUARY] = A888("ui_ult_sanctuary", 16, 16),
    [SPR_UI_ULT_JACKPOT] = A888("ui_ult_jackpot", 16, 16),
    [SPR_UI_ULT_ECLIPSE] = A888("ui_ult_eclipse", 16, 16),
    [SPR_UI_AFFIX_LEECH] = A888("ui_affix_leech", 16, 16),
    [SPR_UI_AFFIX_GUARD] = A888("ui_affix_guard", 16, 16),
    [SPR_UI_AFFIX_SWIFT] = A888("ui_affix_swift", 16, 16),
    [SPR_UI_AFFIX_VENOM] = A888("ui_affix_venom", 16, 16),
    [SPR_UI_AFFIX_SLAYER] = A888("ui_affix_slayer", 16, 16),
    [SPR_UI_AFFIX_GREED] = A888("ui_affix_greed", 16, 16),
    [SPR_UI_RESOURCE_ESSENCE] = A888("ui_resource_essence", 16, 16),
    [SPR_UI_ITEM_FEATHER] = A888("ui_item_feather", 16, 16),
    [SPR_UI_RARITY0] = A888("ui_rarity0", 8, 8),
    [SPR_UI_RARITY1] = A888("ui_rarity1", 8, 8),
    [SPR_UI_RARITY2] = A888("ui_rarity2", 8, 8),
    [SPR_UI_RARITY3] = A888("ui_rarity3", 8, 8),
    [SPR_UI_RARITY4] = A888("ui_rarity4", 8, 8),
    [SPR_BG_ROOM] = I4("bg_room", 240, 176),
    [SPR_BG_TOWN] = I4("bg_town", 240, 176),
    [SPR_BG_CAVE] = I4("bg_cave", 240, 176),
    [SPR_BG_FIRE] = I4("bg_fire", 240, 176),
    [SPR_BG_FROST] = I4("bg_frost", 240, 176),
    [SPR_BG_VAULT] = I4("bg_vault", 240, 176),
};

#ifdef ESP_PLATFORM
// Symbol per file: _binary_<stem>_<ext>_start, dots -> underscores.
#define DECL(f, e) extern const uint8_t _binary_##f##_##e##_start[];
DECL(idle0, argb8888) DECL(idle1, argb8888)
DECL(happy0, argb8888) DECL(happy1, argb8888) DECL(happy2, argb8888)
DECL(ag_working, argb8888) DECL(ag_needs0, argb8888) DECL(ag_needs1, argb8888)
DECL(ag_review, argb8888) DECL(ag_failed, argb8888) DECL(ag_celebrate, argb8888)
DECL(hero_job_white, argb8888) DECL(hero_job_black, argb8888)
DECL(oc_pomo_focus, argb8888) DECL(oc_pomo_reward, argb8888)
DECL(oc_clicker, argb8888) DECL(oc_standby_sleep, argb8888)
DECL(side_knight, argb8888) DECL(side_black, argb8888) DECL(side_white, argb8888)
DECL(side_thief, argb8888) DECL(side_dark, argb8888)
DECL(front_knight, argb8888) DECL(front_thief, argb8888) DECL(front_dark, argb8888)
DECL(mon_slime, argb8888) DECL(mon_slime_hurt, argb8888)
DECL(mon_bat0, argb8888) DECL(mon_bat1, argb8888)
DECL(mon_bat_hurt, argb8888)
DECL(mon_skel, argb8888) DECL(mon_skel_hurt, argb8888)
DECL(mon_golem, argb8888) DECL(mon_golem_hurt, argb8888)
DECL(mon_mimic_closed, argb8888) DECL(mon_mimic_open, argb8888)
DECL(mon_ancient, argb8888) DECL(mon_ancient_hurt, argb8888)
DECL(event_cursed_gate, argb8888) DECL(event_wishing_well, argb8888)
DECL(event_rare_sparkle, argb8888) DECL(event_starfall, argb8888)
DECL(ult_castle, argb8888) DECL(ult_meteor, argb8888) DECL(ult_sanctuary, argb8888)
DECL(ult_jackpot, argb8888) DECL(ult_eclipse, argb8888)
DECL(ui_rebirth, argb8888)
DECL(ui_ult_castle, argb8888) DECL(ui_ult_meteor, argb8888)
DECL(ui_ult_sanctuary, argb8888) DECL(ui_ult_jackpot, argb8888) DECL(ui_ult_eclipse, argb8888)
DECL(ui_affix_leech, argb8888) DECL(ui_affix_guard, argb8888) DECL(ui_affix_swift, argb8888)
DECL(ui_affix_venom, argb8888) DECL(ui_affix_slayer, argb8888) DECL(ui_affix_greed, argb8888)
DECL(ui_resource_essence, argb8888) DECL(ui_item_feather, argb8888)
DECL(ui_rarity0, argb8888) DECL(ui_rarity1, argb8888) DECL(ui_rarity2, argb8888)
DECL(ui_rarity3, argb8888) DECL(ui_rarity4, argb8888)
DECL(bg_room, i4) DECL(bg_town, i4) DECL(bg_cave, i4)
DECL(bg_fire, i4) DECL(bg_frost, i4) DECL(bg_vault, i4)
#define PTR(f, e) _binary_##f##_##e##_start

static const uint8_t *spr_ptr(sprite_id_t id)
{
    switch (id) {
    case SPR_IDLE0: return PTR(idle0, argb8888);
    case SPR_IDLE1: return PTR(idle1, argb8888);
    case SPR_HAPPY0: return PTR(happy0, argb8888);
    case SPR_HAPPY1: return PTR(happy1, argb8888);
    case SPR_HAPPY2: return PTR(happy2, argb8888);
    case SPR_AG_WORKING: return PTR(ag_working, argb8888);
    case SPR_AG_NEEDS0: return PTR(ag_needs0, argb8888);
    case SPR_AG_NEEDS1: return PTR(ag_needs1, argb8888);
    case SPR_AG_REVIEW: return PTR(ag_review, argb8888);
    case SPR_AG_FAILED: return PTR(ag_failed, argb8888);
    case SPR_AG_CELEBRATE: return PTR(ag_celebrate, argb8888);
    case SPR_HERO_JOB_WHITE: return PTR(hero_job_white, argb8888);
    case SPR_HERO_JOB_BLACK: return PTR(hero_job_black, argb8888);
    case SPR_OC_POMO_FOCUS: return PTR(oc_pomo_focus, argb8888);
    case SPR_OC_POMO_REWARD: return PTR(oc_pomo_reward, argb8888);
    case SPR_OC_CLICKER: return PTR(oc_clicker, argb8888);
    case SPR_OC_STANDBY_SLEEP: return PTR(oc_standby_sleep, argb8888);
    case SPR_SIDE_KNIGHT: return PTR(side_knight, argb8888);
    case SPR_SIDE_BLACK: return PTR(side_black, argb8888);
    case SPR_SIDE_WHITE: return PTR(side_white, argb8888);
    case SPR_SIDE_THIEF: return PTR(side_thief, argb8888);
    case SPR_SIDE_DARK: return PTR(side_dark, argb8888);
    case SPR_FRONT_KNIGHT: return PTR(front_knight, argb8888);
    case SPR_FRONT_THIEF: return PTR(front_thief, argb8888);
    case SPR_FRONT_DARK: return PTR(front_dark, argb8888);
    case SPR_MON_SLIME: return PTR(mon_slime, argb8888);
    case SPR_MON_SLIME_HURT: return PTR(mon_slime_hurt, argb8888);
    case SPR_MON_BAT0: return PTR(mon_bat0, argb8888);
    case SPR_MON_BAT1: return PTR(mon_bat1, argb8888);
    case SPR_MON_BAT_HURT: return PTR(mon_bat_hurt, argb8888);
    case SPR_MON_SKEL: return PTR(mon_skel, argb8888);
    case SPR_MON_SKEL_HURT: return PTR(mon_skel_hurt, argb8888);
    case SPR_MON_GOLEM: return PTR(mon_golem, argb8888);
    case SPR_MON_GOLEM_HURT: return PTR(mon_golem_hurt, argb8888);
    case SPR_MON_MIMIC_CLOSED: return PTR(mon_mimic_closed, argb8888);
    case SPR_MON_MIMIC_OPEN: return PTR(mon_mimic_open, argb8888);
    case SPR_MON_ANCIENT: return PTR(mon_ancient, argb8888);
    case SPR_MON_ANCIENT_HURT: return PTR(mon_ancient_hurt, argb8888);
    case SPR_EVENT_CURSED_GATE: return PTR(event_cursed_gate, argb8888);
    case SPR_EVENT_WISHING_WELL: return PTR(event_wishing_well, argb8888);
    case SPR_EVENT_RARE_SPARKLE: return PTR(event_rare_sparkle, argb8888);
    case SPR_EVENT_STARFALL: return PTR(event_starfall, argb8888);
    case SPR_ULT_CASTLE: return PTR(ult_castle, argb8888);
    case SPR_ULT_METEOR: return PTR(ult_meteor, argb8888);
    case SPR_ULT_SANCTUARY: return PTR(ult_sanctuary, argb8888);
    case SPR_ULT_JACKPOT: return PTR(ult_jackpot, argb8888);
    case SPR_ULT_ECLIPSE: return PTR(ult_eclipse, argb8888);
    case SPR_UI_REBIRTH: return PTR(ui_rebirth, argb8888);
    case SPR_UI_ULT_CASTLE: return PTR(ui_ult_castle, argb8888);
    case SPR_UI_ULT_METEOR: return PTR(ui_ult_meteor, argb8888);
    case SPR_UI_ULT_SANCTUARY: return PTR(ui_ult_sanctuary, argb8888);
    case SPR_UI_ULT_JACKPOT: return PTR(ui_ult_jackpot, argb8888);
    case SPR_UI_ULT_ECLIPSE: return PTR(ui_ult_eclipse, argb8888);
    case SPR_UI_AFFIX_LEECH: return PTR(ui_affix_leech, argb8888);
    case SPR_UI_AFFIX_GUARD: return PTR(ui_affix_guard, argb8888);
    case SPR_UI_AFFIX_SWIFT: return PTR(ui_affix_swift, argb8888);
    case SPR_UI_AFFIX_VENOM: return PTR(ui_affix_venom, argb8888);
    case SPR_UI_AFFIX_SLAYER: return PTR(ui_affix_slayer, argb8888);
    case SPR_UI_AFFIX_GREED: return PTR(ui_affix_greed, argb8888);
    case SPR_UI_RESOURCE_ESSENCE: return PTR(ui_resource_essence, argb8888);
    case SPR_UI_ITEM_FEATHER: return PTR(ui_item_feather, argb8888);
    case SPR_UI_RARITY0: return PTR(ui_rarity0, argb8888);
    case SPR_UI_RARITY1: return PTR(ui_rarity1, argb8888);
    case SPR_UI_RARITY2: return PTR(ui_rarity2, argb8888);
    case SPR_UI_RARITY3: return PTR(ui_rarity3, argb8888);
    case SPR_UI_RARITY4: return PTR(ui_rarity4, argb8888);
    case SPR_BG_ROOM: return PTR(bg_room, i4);
    case SPR_BG_TOWN: return PTR(bg_town, i4);
    case SPR_BG_CAVE: return PTR(bg_cave, i4);
    case SPR_BG_FIRE: return PTR(bg_fire, i4);
    case SPR_BG_FROST: return PTR(bg_frost, i4);
    case SPR_BG_VAULT: return PTR(bg_vault, i4);
    default: return PTR(idle0, argb8888);
    }
}
#else
static uint8_t *sim_buf[SPR_COUNT];

// Sim builds read art straight from the repo. Flavor stems (heroes, faces)
// live in sprites/<flavor>/, shared world art in sprites/; DESKPET_ART_FLAVOR
// picks the flavor the firmware build would embed (default: generic).
static const char *sim_flavor(void)
{
    const char *flavor = getenv("DESKPET_ART_FLAVOR");
    return (flavor && flavor[0]) ? flavor : "generic";
}

static size_t spr_data_size(const spr_def_t *def)
{
    if (def->cf == LV_COLOR_FORMAT_I4)
        return 16u * 4u + ((def->w + 1u) / 2u) * def->h;
    return (size_t)def->w * def->h * 4u;
}

static const uint8_t *spr_ptr(sprite_id_t id)
{
    if (sim_buf[id]) return sim_buf[id];
    char path[256];
    const char *root = getenv("DESKPET_ASSETS");
    size_t bytes = spr_data_size(&DEFS[id]);
    if (!root) root = "main/sprites";
    snprintf(path, sizeof(path), "%s/%s/%s", root, sim_flavor(), DEFS[id].file);
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "%s/%s", root, DEFS[id].file);
        f = fopen(path, "rb");
    }
    if (!f) return NULL;
    sim_buf[id] = malloc(bytes);
    size_t n = sim_buf[id] ? fread(sim_buf[id], 1, bytes, f) : 0;
    fclose(f);
    if (n != bytes) {
        free(sim_buf[id]);
        sim_buf[id] = NULL;
        return NULL;
    }
    return sim_buf[id];
}
#endif

static lv_img_dsc_t s_dsc[SPR_COUNT];
static bool s_init;

static uint32_t spr_stride(const spr_def_t *def)
{
    return def->cf == LV_COLOR_FORMAT_I4 ? (def->w + 1u) / 2u : def->w * 4u;
}

static uint32_t spr_size(const spr_def_t *def)
{
    if (def->cf == LV_COLOR_FORMAT_I4)
        return 16u * 4u + spr_stride(def) * def->h;
    return spr_stride(def) * def->h;
}

const lv_img_dsc_t *sprite_get(sprite_id_t id)
{
    if ((int)id < 0 || id >= SPR_COUNT) id = SPR_IDLE0;
    const uint8_t *p = spr_ptr(id);
    if (!p) return NULL;
    if (!s_init) {
        for (int i = 0; i < SPR_COUNT; i++) {
            s_dsc[i].header.magic = LV_IMAGE_HEADER_MAGIC;
            s_dsc[i].header.cf = DEFS[i].cf;
            s_dsc[i].header.w = DEFS[i].w;
            s_dsc[i].header.h = DEFS[i].h;
            s_dsc[i].header.stride = spr_stride(&DEFS[i]);
            s_dsc[i].data_size = spr_size(&DEFS[i]);
        }
        s_init = true;
    }
    s_dsc[id].data = p;
    return &s_dsc[id];
}

sprite_id_t sprite_for_mood(int mood, uint32_t tick_500ms)
{
    // All faces come from the 25x31 badge set (one OC, one scale); the old
    // 64px hero frames that used to sit here were a different character.
    switch (mood) {
    case PET_MOOD_HAPPY: return (sprite_id_t)(SPR_HAPPY0 + (tick_500ms % 3));
    case PET_MOOD_CELEBRATE: return SPR_AG_CELEBRATE;
    case PET_MOOD_SAD: return SPR_AG_FAILED;
    case PET_MOOD_BUSY: return (tick_500ms % 2) ? SPR_AG_WORKING : SPR_IDLE0;
    case PET_MOOD_FOCUS: return SPR_AG_WORKING;
    case PET_MOOD_LOWBAT:
    case PET_MOOD_SLEEP: return SPR_IDLE1;   // eyes shut
    default: return (tick_500ms % 4 == 3) ? SPR_IDLE1 : SPR_IDLE0;  // blink
    }
}
