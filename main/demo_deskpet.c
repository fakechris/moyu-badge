// main/demo_deskpet.c —— DESIGN v1 screens: S1 town / S2 side battle /
// S3 rest / S4 choice card / S5 offline report. Keymap = DESIGN 2 global table.
#include "demo_deskpet.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "deskpet_i18n.h"
#include "deskpet_store.h"
#include "dungeon_model.h"
#include "chiptune.h"
#include "pet_model.h"
#include "pomo_lite.h"
#include "sprite.h"
#include "sprite_anim.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "demo_deskpet";

static void lbl(lv_obj_t *o, const char *text);
static void show(lv_obj_t *o, bool on);

typedef enum { M_TOWN = 0, M_BATTLE, M_REST, M_CHOICE, M_REPORT } ui_mode_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_bg;
static lv_obj_t *s_gold, *s_stage, *s_hpbar, *s_hptext, *s_enbar, *s_jobtext, *s_meta;
static lv_obj_t *s_player, *s_mon[3], *s_rare[3], *s_face, *s_overlay, *s_ui_icon;
// Battle readability (review follow-up): every HP change gets a cause on
// screen — enemy HP bars, per-target hurt flash, floating damage numbers,
// and the enemies' reply shown half a beat after the OC's swing.
#define UI_ROUND_MS 1400u          // on-screen beat (model rounds stay 800 ms for sim/offline)
#define UI_REPLY_MS 600u           // enemies answer this long after the OC acts
#define DMG_TICKS 9
static lv_obj_t *s_ehp[3];         // enemy HP bars
static lv_obj_t *s_dmg[4];         // floating numbers: 0..2 enemies, 3 = OC
static int8_t s_dmg_ticks[4];
static uint16_t s_ehp_max[3];
static uint8_t s_hurt_mask;        // which enemies flash this beat
static uint8_t s_player_hurt_ticks;
static int s_pending_taken;        // OC damage revealed at the reply beat
static uint64_t s_reply_ms;
static uint16_t s_hp_shown;        // displayed OC HP (lags until the reply beat)
static uint8_t s_energy_prev;
static lv_obj_t *s_ico_ess, *s_ico_fea, *s_ess_txt, *s_fea_txt;
// Milestone banners (floor change / level up / job level / maze clear) and
// per-enemy level tags, so progress is never just a number changing quietly.
#define BANNER_TICKS 14
static lv_obj_t *s_banner, *s_dim, *s_elv[3], *s_entext;
static int8_t s_banner_ticks;
static uint8_t s_prev_floor, s_prev_ng, s_prev_joblv[DUNGEON_JOB_COUNT];
static uint16_t s_prev_xlv;
static uint8_t s_prev_unlocked;

static pet_save_t s_pet;
static dungeon_save_t s_dun;
static pomo_t s_pomo;
static deskpet_lang_t s_lang = LANG_EN;

// UI-side log line (model log is model-owned; this mirrors log_push).
static void log_push_ui(const char *msg)
{
    for (int i = DUNGEON_LOG_LINES - 1; i > 0; i--)
        memcpy(s_dun.log[i], s_dun.log[i - 1], DUNGEON_LOG_LEN);
    snprintf(s_dun.log[0], DUNGEON_LOG_LEN, "%s", msg);
}

static const char *foe_name(uint8_t i)
{
    if (s_dun.hidden_room) return "Ancient";
    if (s_dun.mimic_room) return "Mimic";
    switch (s_dun.enemy_kind[i] & 3) {
    case 3: return "Gate";
    case 1: return "Bat";
    case 2: return "Skel";
    default: return "Slime";
    }
}

static void popup(int slot, int value, uint32_t color)
{
    char buf[12];
    snprintf(buf, sizeof(buf), value < 0 ? "%d" : "+%d", value);
    lbl(s_dmg[slot], buf);
    lv_obj_set_style_text_color(s_dmg[slot], lv_color_hex(color), 0);
    s_dmg_ticks[slot] = DMG_TICKS;
}

static void banner(const char *text, uint32_t color)
{
    lbl(s_banner, text);
    lv_obj_set_style_text_color(s_banner, lv_color_hex(color), 0);
    s_banner_ticks = BANNER_TICKS;
}

// Compare the model against the last frame and announce what changed.
static void check_milestones(void)
{
    char buf[40];
    if (s_dun.ng != s_prev_ng && s_dun.ng > s_prev_ng) {
        snprintf(buf, sizeof(buf), "MAZE CLEAR! badge %u", s_dun.ng);
        banner(buf, 0xFFD25A);
    } else if (s_dun.floor != s_prev_floor && s_dun.floor > 1) {
        snprintf(buf, sizeof(buf), "%s F%u", s_dun.floor > s_prev_floor ? "DOWN" : "", s_dun.floor);
        banner(buf, 0xBFE3FF);
        audio_se(CT_SE_STAIR);
    } else if (s_dun.explore_lv > s_prev_xlv) {
        snprintf(buf, sizeof(buf), "LEVEL UP! %u", s_dun.explore_lv);
        banner(buf, 0x7CE38B);
    } else {
        for (uint8_t j = 0; j < DUNGEON_JOB_COUNT; j++) {
            if (s_dun.job_lv[j] > s_prev_joblv[j]) {
                snprintf(buf, sizeof(buf), "%s LV%u", dungeon_job_name_en((dungeon_job_t)j), s_dun.job_lv[j]);
                banner(buf, 0x7CE38B);
                break;
            }
            if (((s_dun.unlocked >> j) & 1) && !((s_prev_unlocked >> j) & 1)) {
                snprintf(buf, sizeof(buf), "NEW JOB: %s", dungeon_job_name_en((dungeon_job_t)j));
                banner(buf, 0xFFD25A);
                break;
            }
        }
    }
    s_prev_floor = s_dun.floor;
    s_prev_ng = s_dun.ng;
    s_prev_xlv = s_dun.explore_lv;
    s_prev_unlocked = s_dun.unlocked;
    memcpy(s_prev_joblv, s_dun.job_lv, sizeof(s_prev_joblv));
}
static lv_obj_t *s_menu, *s_log;
static lv_timer_t *s_timer;
static ui_mode_t s_mode;
static int s_cursor;   // town row / battle skill hl 0..2
static int s_upsel;    // upgrade target: 0 weapon / 1 armor / 2 smoke / 3 potion
static int s_synsel;   // town synth target: 0 weapon / 1 armor
static int s_sansel;   // town sanctum track cursor
// Autopilot beats: the OC shops / rests / reads its card for a moment so the
// player can see what happened, then moves on by itself. 0 = no auto-leave.
#define TOWN_BEAT_MS 4000u
#define REST_BEAT_MS 3000u
#define REPORT_BEAT_MS 6000u
static uint64_t s_auto_leave_ms;
static uint8_t s_battle_end_hold;   // battle-end banner beat (any end)
static bool s_manual_battle;        // V2 playback latch (§11): set on any
                                    // battle key; reset on fresh battle entry
static uint8_t s_beat_next_choice;  // after the beat: 1 = open choice card
static int s_pairA, s_pairB;
static uint32_t s_tick;
static uint8_t s_flash;
static uint8_t s_attack_frame;
static uint8_t s_fx_ticks;
static uint8_t s_mimic_reveal_ticks;
#define MIMIC_REVEAL_TICKS 12u  // 0.7 s closed, then open before combat starts
#define MIMIC_CLOSED_UNTIL 5u
static bool s_attacking;
static sprite_id_t s_fx_sprite;
static uint64_t s_round_due_ms, s_explore_due_ms, s_last_ms;
// NVS cadence: the save only guards against power loss, so routine progress
// is flushed every 10 min (1 min once the battery is low); milestones and
// user actions flush immediately. Per-round saves wore the 24 KB partition
// in ~40 days of auto-battle and hitched the frame on every flash erase.
#define SAVE_INTERVAL_MS 600000u
#define SAVE_INTERVAL_LOWBAT_MS 60000u
#define SAVE_LOWBAT_SOC 15
static bool s_dirty;
static uint64_t s_last_save_ms;
static _Atomic int s_audio_scene = CT_SCENE_NONE;


extern const lv_font_t zh_subset;

static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

static void save_now(void)
{
    deskpet_store_save(&s_pet, &s_dun, &s_pomo, s_lang);
    s_dirty = false;
    s_last_save_ms = now_ms();
}

static void save_later(void) { s_dirty = true; }

static void save_if_due(uint64_t now)
{
    if (!s_dirty) return;
    uint64_t interval = SAVE_INTERVAL_MS;
    int soc = bsp_battery_soc();
    if (soc >= 0 && soc <= SAVE_LOWBAT_SOC) interval = SAVE_INTERVAL_LOWBAT_MS;
    if (now - s_last_save_ms >= interval) save_now();
}

static const char *T(deskpet_str_id_t id)
{
    return deskpet_tr(id, s_lang);
}


// ---- change-guarded LVGL setters ------------------------------------------
// refresh() runs every 100 ms tick. Setting text/src/style unconditionally
// marks the whole screen dirty every tick; on the C3 a full 240x320 redraw plus
// SPI flush is >100 ms, so the LVGL task never yielded (IDLE starved, task WDT,
// BLE init pushed to 18 s on 604ea5b). Only touch objects when they change.
static void lbl(lv_obj_t *o, const char *text)
{
    if (o && strcmp(lv_label_get_text(o), text) != 0) lv_label_set_text(o, text);
}

static void show(lv_obj_t *o, bool on)
{
    if (!o) return;
    bool hidden = lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
    if (on && hidden) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else if (!on && !hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void img_set(lv_obj_t *img, const lv_img_dsc_t *dsc)
{
    if (dsc && lv_img_get_src(img) != dsc) lv_img_set_src(img, dsc);
}

static void set_pos(lv_obj_t *o, int32_t x, int32_t y)
{
    if (lv_obj_get_x(o) != x || lv_obj_get_y(o) != y) lv_obj_set_pos(o, x, y);
}

static sprite_id_t bg_for(void)
{
    if (s_mode == M_TOWN || s_mode == M_REST || s_mode == M_REPORT) return SPR_BG_TOWN;
    if (s_dun.floor <= 20) return SPR_BG_ROOM;
    if (s_dun.floor <= 40) return SPR_BG_CAVE;
    if (s_dun.floor <= 60) return SPR_BG_FIRE;
    if (s_dun.floor <= 80) return SPR_BG_FROST;
    return SPR_BG_VAULT;
}

static int audio_scene_for_state(void)
{
    switch (s_mode) {
    case M_TOWN: return CT_SCENE_TOWN;
    case M_BATTLE: return s_dun.must_bright ? CT_SCENE_BOSS : CT_SCENE_BATTLE;
    case M_REST: return CT_SCENE_TOWN;
    case M_CHOICE: return CT_SCENE_DUNGEON;
    default: return CT_SCENE_NONE;
    }
}

static sprite_id_t ult_fx_for(dungeon_job_t job)
{
    static const sprite_id_t ids[DUNGEON_JOB_COUNT] = {
        SPR_ULT_CASTLE, SPR_ULT_METEOR, SPR_ULT_SANCTUARY,
        SPR_ULT_JACKPOT, SPR_ULT_ECLIPSE,
    };
    return ids[job < DUNGEON_JOB_COUNT ? job : JOB_KNIGHT];
}

static ct_se_t ult_se_for(dungeon_job_t job)
{
    static const ct_se_t ids[DUNGEON_JOB_COUNT] = {
        CT_SE_ULT_KNIGHT, CT_SE_ULT_BLACK, CT_SE_ULT_WHITE,
        CT_SE_ULT_THIEF, CT_SE_ULT_DARK,
    };
    return ids[job < DUNGEON_JOB_COUNT ? job : JOB_KNIGHT];
}

static sprite_id_t ult_icon_for(dungeon_job_t job)
{
    static const sprite_id_t ids[DUNGEON_JOB_COUNT] = {
        SPR_UI_ULT_CASTLE, SPR_UI_ULT_METEOR, SPR_UI_ULT_SANCTUARY,
        SPR_UI_ULT_JACKPOT, SPR_UI_ULT_ECLIPSE,
    };
    return ids[job < DUNGEON_JOB_COUNT ? job : JOB_KNIGHT];
}

static sprite_id_t affix_icon(uint8_t affix)
{
    static const sprite_id_t ids[AF_COUNT] = {
        SPR_COUNT, SPR_UI_AFFIX_LEECH, SPR_UI_AFFIX_GUARD,
        SPR_UI_AFFIX_SWIFT, SPR_UI_AFFIX_VENOM, SPR_UI_AFFIX_SLAYER,
        SPR_UI_AFFIX_GREED,
    };
    return ids[affix < AF_COUNT ? affix : AF_NONE];
}

static sprite_id_t equipped_affix_icon(uint32_t phase)
{
    const uint8_t affixes[4] = {
        s_dun.weapon_affix & 0x0f, s_dun.weapon_affix >> 4,
        s_dun.armor_affix & 0x0f, s_dun.armor_affix >> 4,
    };
    for (uint8_t offset = 0; offset < 4; offset++) {
        uint8_t affix = affixes[(phase + offset) % 4];
        if (affix != AF_NONE) return affix_icon(affix);
    }
    return SPR_COUNT;
}

static void show_fx(sprite_id_t sprite, uint8_t ticks)
{
    s_fx_sprite = sprite;
    s_fx_ticks = ticks;
}

static void start_attack(void)
{
    s_attacking = true;
    s_attack_frame = 0;
}

static sprite_id_t side_for(dungeon_job_t job)
{
    static const sprite_id_t T[DUNGEON_JOB_COUNT] = {
        [JOB_KNIGHT] = SPR_SIDE_KNIGHT,
        [JOB_BLACK] = SPR_SIDE_BLACK,
        [JOB_WHITE] = SPR_SIDE_WHITE,
        [JOB_THIEF] = SPR_SIDE_THIEF,
        [JOB_DARK] = SPR_SIDE_DARK,
    };
    if (job >= DUNGEON_JOB_COUNT) job = JOB_KNIGHT;
    return T[job];
}

static sprite_id_t front_for(dungeon_job_t job)
{
    switch (job) {
    case JOB_KNIGHT: return SPR_FRONT_KNIGHT;
    case JOB_THIEF: return SPR_FRONT_THIEF;
    case JOB_BLACK: return SPR_HERO_JOB_BLACK;
    case JOB_WHITE: return SPR_HERO_JOB_WHITE;
    case JOB_DARK: return SPR_FRONT_DARK;
    default: return SPR_FRONT_KNIGHT;
    }
}

static sprite_id_t foe_sprite(uint8_t kind, uint8_t hurt)
{
    if (s_dun.hidden_room) return hurt ? SPR_MON_ANCIENT_HURT : SPR_MON_ANCIENT;
    if (s_dun.mimic_room)
        return s_mimic_reveal_ticks > MIMIC_CLOSED_UNTIL
                   ? SPR_MON_MIMIC_CLOSED : SPR_MON_MIMIC_OPEN;
    switch (kind) {
    case 3: return hurt ? SPR_MON_GOLEM_HURT : SPR_MON_GOLEM;
    case 1: return hurt ? SPR_MON_BAT_HURT : (((s_tick / 3) % 2) ? SPR_MON_BAT0 : SPR_MON_BAT1);
    case 2: return hurt ? SPR_MON_SKEL_HURT : SPR_MON_SKEL;
    default: return hurt ? SPR_MON_SLIME_HURT : SPR_MON_SLIME;
    }
}

static deskpet_str_id_t skill_zh(dungeon_skill_t sk)
{
    switch (sk) {
    case SK_SHIELD_BASH: return S_SK_BASH;
    case SK_TAUNT: return S_SK_TAUNT;
    case SK_FIRE: return S_SK_FIRE;
    case SK_FROST: return S_SK_FROST;
    case SK_CURE: return S_SK_CURE;
    case SK_AEGIS: return S_SK_AEGIS;
    case SK_VENOM: return S_SK_VENOM;
    case SK_SMOKEOUT: return S_SK_SMOKE;
    case SK_DRAIN: return S_SK_DRAIN;
    case SK_DOOM: return S_SK_DOOM;
    default: return S_SKILL;
    }
}

static deskpet_str_id_t choice_zh(dungeon_choice_t ch)
{
    switch (ch) {
    case CH_AFFIX: return S_CH_AFFIX;
    case CH_BLESS_ATK: return S_CH_ATK;
    case CH_BLESS_LEARN: return S_CH_LEARN;
    case CH_BLESS_DEF: return S_CH_DEF;
    case CH_TRADE_WOUND: return S_CH_WOUND;
    case CH_TRADE_FEATHER: return S_CH_FEATHER;
    case CH_SHRINE: return S_CH_SHRINE;
    case CH_SHARDS: return S_CH_SHARDS;
    default: return S_CH_CHALLENGE;
    }
}

static const char *slot_name(uint8_t slot, char *tmp, size_t n)
{
    dungeon_job_t job = (dungeon_job_t)(slot == 2 ? s_dun.sub_job : s_dun.main_job);
    uint8_t sub = (slot == 2) ? 0 : slot;
    const dungeon_job_def_t *d = dungeon_job_def(job);
    dungeon_skill_t sk = d->active[sub];
    if (!dungeon_skill_unlocked(&s_dun, job, sub)) {
        snprintf(tmp, n, "--");
        return tmp;
    }
    uint8_t cd = (slot == 2) ? s_dun.skill_cd[2] : s_dun.skill_cd[slot];
    if (s_lang == LANG_ZH) {
        if (cd) snprintf(tmp, n, "%s·%u", T(skill_zh(sk)), cd);
        else snprintf(tmp, n, "%s", T(skill_zh(sk)));
    } else {
        // Montserrat 12 is ASCII + degree + bullet; U+00B7 middle-dot tofus.
        if (cd) snprintf(tmp, n, "%s:%u", dungeon_skill_name_en(sk), cd);
        else snprintf(tmp, n, "%s", dungeon_skill_name_en(sk));
    }
    return tmp;
}

static deskpet_str_id_t choice_desc(dungeon_choice_t ch)
{
    switch (ch) {
    case CH_AFFIX: return S_CHD_AFFIX;
    case CH_BLESS_ATK: return S_CHD_ATK;
    case CH_BLESS_LEARN: return S_CHD_LEARN;
    case CH_BLESS_DEF: return S_CHD_DEF;
    case CH_TRADE_WOUND: return S_CHD_WOUND;
    case CH_TRADE_FEATHER: return S_CHD_FEATHER;
    case CH_SHRINE: return S_CHD_SHRINE;
    case CH_SHARDS: return S_CHD_SHARDS;
    default: return S_CHD_CHALLENGE;
    }
}

static void refresh(void)
{
    char buf[96];
    atomic_store(&s_audio_scene, audio_scene_for_state());
    img_set(s_bg, sprite_get(bg_for()));
    snprintf(buf, sizeof(buf), "%ug", s_dun.gold);
    lbl(s_gold, buf);
    snprintf(buf, sizeof(buf), "F%u", s_dun.floor);
    lbl(s_stage, buf);
    uint16_t mh = dungeon_max_hp(&s_dun);
    if (!s_reply_ms || s_dun.hp_cur > s_hp_shown) s_hp_shown = s_dun.hp_cur;  // heals show at once
    if (s_hp_shown > mh) s_hp_shown = mh;
    lv_bar_set_value(s_hpbar, mh ? (int)(s_hp_shown * 100 / mh) : 0, LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "HP %u/%u", s_hp_shown, mh);
    lbl(s_hptext, buf);
    lv_bar_set_value(s_enbar, s_dun.max_energy ? (int)(s_dun.energy * 100 / s_dun.max_energy) : 0,
                     LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "EN%u", s_dun.energy);
    lbl(s_entext, buf);
    static const char *const JOBCODE[DUNGEON_JOB_COUNT] = {"KNT", "BLM", "WHM", "THF", "DRK"};
    uint8_t main_job = s_dun.main_job < DUNGEON_JOB_COUNT ? s_dun.main_job : JOB_KNIGHT;
    uint8_t sub_job = s_dun.sub_job < DUNGEON_JOB_COUNT ? s_dun.sub_job : JOB_KNIGHT;
    snprintf(buf, sizeof(buf), "%s+%s", JOBCODE[main_job], JOBCODE[sub_job]);
    lbl(s_jobtext, buf);
    // meta at a glance: [essence icon] n  [feather icon] n  S shards L joblv P prestige B badges
    snprintf(buf, sizeof(buf), "%u", s_dun.essence);
    lbl(s_ess_txt, buf);
    snprintf(buf, sizeof(buf), "%u", s_dun.cons[CONS_FEATHER]);
    lbl(s_fea_txt, buf);
    snprintf(buf, sizeof(buf), "S%u L%u P%u B%u", s_dun.shards,
             s_dun.job_lv[main_job], s_dun.prestige, s_dun.ng);
    lbl(s_meta, buf);

    // stage
    if (s_mode == M_BATTLE) {
        const lv_img_dsc_t *player = NULL;
        if (s_attacking) {
            player = sprite_anim_get(s_dun.main_job, SPR_ANIM_ATTACK, s_attack_frame);
        } else if (s_dun.exploring && s_dun.n_enemy == 0) {
            player = sprite_anim_get(s_dun.main_job, SPR_ANIM_WALK, (uint8_t)(s_tick % 8));
        } else {
            player = sprite_get(side_for((dungeon_job_t)s_dun.main_job));
        }
        img_set(s_player, player);
        if (s_attacking || (s_dun.exploring && s_dun.n_enemy == 0))
            lv_obj_invalidate(s_player);  // packed frames share one decode buffer
        show(s_player, true);
        show(s_face, false);
        // OC hurt: shake + red tint for a few ticks
        set_pos(s_player, 18 + (s_player_hurt_ticks ? ((s_player_hurt_ticks & 1) ? 3 : -3) : 0), 124);
        static bool s_tinted;
        if ((s_player_hurt_ticks != 0) != s_tinted) {
            s_tinted = s_player_hurt_ticks != 0;
            lv_obj_set_style_img_recolor_opa(s_player, s_tinted ? LV_OPA_50 : LV_OPA_0, 0);
        }
        for (int i = 0; i < 3; i++) {
            if ((uint8_t)i < s_dun.n_enemy && s_dun.enemy_hp[i]) {
                uint8_t hurt = (uint8_t)(s_flash && ((s_hurt_mask >> i) & 1));
                img_set(s_mon[i], sprite_get(foe_sprite(s_dun.enemy_kind[i], hurt)));
                show(s_mon[i], true);
                if (s_dun.rare_room) show(s_rare[i], true);
                else show(s_rare[i], false);
                uint16_t mx = s_ehp_max[i] ? s_ehp_max[i] : s_dun.enemy_hp[i];
                uint32_t pct = (uint32_t)s_dun.enemy_hp[i] * 100 / mx;
                lv_bar_set_value(s_ehp[i], (int)(pct > 100 ? 100 : pct), LV_ANIM_OFF);
                show(s_ehp[i], true);
                // level tag: floor-driven strength, badge adds +10/maze; elite letter
                static const char *const EA[EA_COUNT] = {"", "A", "V", "S", "T", "G"};
                uint8_t ea = s_dun.enemy_affix[i] < EA_COUNT ? s_dun.enemy_affix[i] : 0;
                snprintf(buf, sizeof(buf), "%sL%u%s", s_dun.enemy_kind[i] == 3 ? "B" : "",
                         (unsigned)(s_dun.floor + 10u * s_dun.ng), EA[ea]);
                lbl(s_elv[i], buf);
                lv_obj_set_style_text_color(s_elv[i], lv_color_hex(ea ? 0xFFD25A : 0xC8D0E0), 0);
                show(s_elv[i], true);
            } else {
                show(s_mon[i], false);
                show(s_rare[i], false);
                show(s_ehp[i], false);
                show(s_elv[i], false);
            }
        }
    } else if (s_mode == M_TOWN || s_mode == M_REST) {
        img_set(s_player, sprite_get(front_for((dungeon_job_t)s_dun.main_job)));
        set_pos(s_player, 18, 124);
        show(s_player, true);
        for (int i = 0; i < 3; i++) { show(s_ehp[i], false); show(s_elv[i], false); }
        pet_triggers_t t = {0};
        t.idle_ms = now_ms() - s_last_ms;
        img_set(s_face, sprite_get(sprite_for_mood(pet_mood_resolve(&t), s_tick)));
        show(s_face, true);
        for (int i = 0; i < 3; i++) {
            show(s_mon[i], false);
            show(s_rare[i], false);
        }
    } else {
        // S4 choice is modal: options + the highlighted card's effect.
        show(s_player, false);
        show(s_face, false);
        for (int i = 0; i < 3; i++) {
            show(s_mon[i], false);
            show(s_rare[i], false);
            show(s_ehp[i], false);
            show(s_elv[i], false);
        }
    }
    show(s_log, true);
    // milestone banner: dim the stage, big text, fades with its ticks
    if (s_banner_ticks > 0) {
        show(s_dim, true);
        lv_obj_set_style_bg_opa(s_dim, (lv_opa_t)(s_banner_ticks * 100 / BANNER_TICKS), 0);
        show(s_banner, true);
        set_pos(s_banner, 0, 84 - (BANNER_TICKS - s_banner_ticks));
    } else {
        show(s_dim, false);
        show(s_banner, false);
    }
    // floating numbers drift up and fade out
    static const int PX[4] = {130, 168, 206, 40};
    static const int PY[4] = {110, 110, 110, 100};   // above the level tags
    for (int k = 0; k < 4; k++) {
        if (s_dmg_ticks[k] > 0 && s_mode == M_BATTLE) {
            set_pos(s_dmg[k], PX[k], PY[k] - (DMG_TICKS - s_dmg_ticks[k]) * 2);
            show(s_dmg[k], true);
        } else {
            show(s_dmg[k], false);
        }
    }

    // menu per mode
    char menu[160];
    if (s_mode == M_BATTLE) {
        char a[24], b[24], c[24];
        snprintf(menu, sizeof(menu), "%s%s %s%s\n%s%s %s%s",
                 s_cursor == 0 ? ">" : " ", slot_name(0, a, sizeof a),
                 s_cursor == 1 ? ">" : " ", slot_name(1, b, sizeof b),
                 s_cursor == 2 ? ">" : " ", slot_name(2, c, sizeof c),
                 s_cursor == 3 ? ">" : " ", T(S_HOME));
    } else if (s_mode == M_TOWN) {
        static const char *const TRK[SN_COUNT] = {"VIT", "FOR", "DEF", "LCK", "STA", "PCK"};
        char au[20], up[20], sy[20], sa[24];
        snprintf(au, sizeof(au), "%s:%s", T(S_AUTO), s_dun.autopilot ? T(S_ON) : T(S_OFF));
        snprintf(up, sizeof(up), "%s:%s", T(S_UPGRADE),
                 (const char *const[]){T(S_UPG_WPN), T(S_UPG_ARM), T(S_UPG_SMOKE), T(S_CONS_POT)}[s_upsel & 3]);
        snprintf(sy, sizeof(sy), "%s:%s", T(S_SYNTH), s_synsel ? "Arm" : "Wpn");
        snprintf(sa, sizeof(sa), "%s:%s%u", T(S_SANCTUM), TRK[s_sansel], s_dun.sanctum[s_sansel]);
        snprintf(menu, sizeof(menu),
                 "%s%s %s%s %s%s\n%s%s %s%s %s%s\n%s%s %s%s %s%s",
                 s_cursor == 0 ? ">" : " ", au,
                 s_cursor == 1 ? ">" : " ", up,
                 s_cursor == 2 ? ">" : " ", sy,
                 s_cursor == 3 ? ">" : " ", sa,
                 s_cursor == 4 ? ">" : " ", T(S_TOWN_INN),
                 s_cursor == 5 ? ">" : " ", T(S_WISH),
                 s_cursor == 6 ? ">" : " ", T(S_REBIRTH),
                 s_cursor == 7 ? ">" : " ", T(S_PAIR),
                 s_cursor == 8 ? ">" : " ", T(S_DOWN));
    } else if (s_mode == M_CHOICE) {
        snprintf(menu, sizeof(menu), "%s%s\n%s%s\n%s%s",
                 s_dun.choice_cursor == 0 ? ">" : " ",
                 T(choice_zh((dungeon_choice_t)s_dun.choice_opts[0])),
                 s_dun.choice_cursor == 1 ? ">" : " ",
                 T(choice_zh((dungeon_choice_t)s_dun.choice_opts[1])),
                 s_dun.choice_cursor == 2 ? ">" : " ",
                 T(choice_zh((dungeon_choice_t)s_dun.choice_opts[2])));
    } else if (s_mode == M_REPORT) {
        snprintf(menu, sizeof(menu), "%s OK", T(S_REPORT));
    } else {
        snprintf(menu, sizeof(menu), "%s OK:%s", T(S_REST), T(S_DOWN));
    }
    char log[220];
    if (s_mode == M_CHOICE && s_dun.choice_pending) {
        uint8_t cur = s_dun.choice_cursor < DUNGEON_CHOICE_OPTS ? s_dun.choice_cursor : 0;
        snprintf(log, sizeof(log), "%s%s", T(choice_desc((dungeon_choice_t)s_dun.choice_opts[cur])),
                 s_dun.autopilot ? " *" : "");
    } else {
        snprintf(log, sizeof(log), "%s\n%s\n%s\n%s", s_dun.log[0], s_dun.log[1],
                 s_dun.log[2], s_dun.log[3]);
    }
    lbl(s_menu, menu);
    lbl(s_log, log);

    sprite_id_t overlay = SPR_COUNT;
    if (s_fx_ticks) overlay = s_fx_sprite;
    else if (s_mode == M_TOWN && s_cursor == 5) overlay = SPR_EVENT_WISHING_WELL;
    else if (s_mode == M_TOWN && s_cursor == 6) overlay = SPR_UI_REBIRTH;
    else if (s_mode == M_CHOICE && s_dun.choice_pending
             && s_dun.choice_opts[s_dun.choice_cursor] == CH_CHALLENGE)
        overlay = SPR_EVENT_CURSED_GATE;
    if (overlay < SPR_COUNT) {
        const lv_img_dsc_t *dsc = sprite_get(overlay);
        img_set(s_overlay, dsc);
        if (dsc) set_pos(s_overlay, (240 - (int)dsc->header.w) / 2, 78);
        show(s_overlay, true);
    } else {
        show(s_overlay, false);
    }

    sprite_id_t icon = SPR_COUNT;
    if (s_mode == M_BATTLE && dungeon_ult_ready(&s_dun))
        icon = ult_icon_for((dungeon_job_t)s_dun.main_job);
    else if (s_mode == M_BATTLE && s_dun.rare_room) icon = SPR_UI_RARITY4;
    else if (s_mode == M_TOWN && (s_cursor == 3 || s_cursor == 5)) icon = SPR_UI_RESOURCE_ESSENCE;
    else if (s_mode == M_TOWN && s_cursor == 6) icon = SPR_UI_REBIRTH;
    else if (s_mode == M_TOWN && s_cursor == 8 && s_dun.cons[CONS_FEATHER]) icon = SPR_UI_ITEM_FEATHER;
    else if (s_mode == M_TOWN &&
             (s_cursor == 2 || (s_cursor == 1 && s_upsel < 2))) {  // no sprite for smoke/pot
        uint8_t rarity = (s_cursor == 1 ? s_upsel : s_synsel) ? s_dun.armor_rar : s_dun.weapon_rar;
        icon = (sprite_id_t)(SPR_UI_RARITY0 + (rarity <= 4 ? rarity : 4));
    } else if (s_mode == M_BATTLE) {
        icon = equipped_affix_icon(s_tick / 10);
    }
    if (icon < SPR_COUNT) {
        const lv_img_dsc_t *dsc = sprite_get(icon);
        img_set(s_ui_icon, dsc);
        if (dsc) set_pos(s_ui_icon, 236 - (int)dsc->header.w, 4);
        show(s_ui_icon, true);
    } else {
        show(s_ui_icon, false);
    }

}

// Fonts, layout heights and static styles: once per enter (and per language),
// never per tick.
static void apply_fonts(void)
{
    const lv_font_t *f = (s_lang == LANG_ZH) ? &zh_subset : &lv_font_montserrat_12;
    const lv_font_t *small = (s_lang == LANG_ZH) ? &zh_subset : &lv_font_montserrat_10;
    lv_obj_t *objs[] = {s_gold, s_stage, s_menu, s_hptext, s_jobtext};
    for (unsigned i = 0; i < sizeof(objs) / sizeof(objs[0]); i++)
        lv_obj_set_style_text_font(objs[i], f, 0);
    lv_obj_set_style_text_font(s_log, small, 0);
    // ASCII-only readouts always use the 10 px face (the CJK face is 16 px and overflows)
    lv_obj_t *ascii[] = {s_meta, s_ess_txt, s_fea_txt, s_entext, s_elv[0], s_elv[1], s_elv[2]};
    for (unsigned i = 0; i < sizeof(ascii) / sizeof(ascii[0]); i++)
        lv_obj_set_style_text_font(ascii[i], &lv_font_montserrat_10, 0);
    // CJK menu rows are 16 px: give the menu 3 full rows and the log the rest
    if (s_lang == LANG_ZH) {
        lv_obj_set_height(s_menu, 54); lv_obj_set_pos(s_log, 4, 284); lv_obj_set_height(s_log, 36);
    } else {
        lv_obj_set_height(s_menu, 44); lv_obj_set_pos(s_log, 4, 274); lv_obj_set_height(s_log, 46);
    }
    for (int k = 0; k < 4; k++) lv_obj_set_style_text_font(s_dmg[k], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(s_banner,
                               (s_lang == LANG_ZH) ? &zh_subset : &lv_font_montserrat_20, 0);
    lv_obj_set_style_img_recolor(s_player, lv_color_hex(0xFF3B2F), 0);
    lv_obj_set_style_img_recolor_opa(s_player, LV_OPA_0, 0);
    lv_img_set_zoom(s_face, 512);
}

static void enter_mode(ui_mode_t m);   // fwd (battle_fast lives beside it)

// V2 playback (§11): fast-forward iff pure auto this battle. Manual touch
// latches for the battle's remaining duration; fresh battles start un-driven.
static bool battle_fast(void) { return s_dun.autopilot && !s_manual_battle; }

static void enter_mode(ui_mode_t m)
{
    s_mode = m;
    s_cursor = 0;
    s_auto_leave_ms = 0;
    uint64_t now = now_ms();
    if (m == M_BATTLE) {
        s_pairA = s_dun.main_job;
        s_pairB = s_dun.sub_job;
        // V2 playback latch (§11): a fresh battle (round_ct == 0) starts
        // un-driven; mid-battle returns (e.g. after a choice card) keep the
        // latch — once you touch a battle, the rest of it is yours.
        if (s_dun.round_ct == 0) {
            s_manual_battle = false;
            if (battle_fast()) log_push_ui(T(S_FAST_PLAY));
        }
    } else if (m == M_TOWN && s_dun.autopilot) {
        // The OC shops on its own; the menu stays for overrides.
        int bought = dungeon_autopilot_town(&s_dun);
        if (bought) { log_push_ui(T(S_SHOPPING)); audio_se(CT_SE_LOOT); }
        s_auto_leave_ms = now + TOWN_BEAT_MS;
        s_cursor = 8;   // highlight "Down": that is what happens next
    } else if (m == M_REST && s_dun.autopilot) {
        s_auto_leave_ms = now + REST_BEAT_MS;
    } else if (m == M_REPORT && s_dun.autopilot) {
        s_auto_leave_ms = now + REPORT_BEAT_MS;
    } else if (m == M_CHOICE && s_dun.autopilot && s_dun.choice_pending) {
        s_dun.choice_cursor = dungeon_choice_auto(&s_dun);   // show the intent
    }
    refresh();
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t now = now_ms();
    // S5: gap = offline. 120s without tick (light sleep / sim wait:).
    if (now - s_last_ms > 120000 && s_last_ms != 0) {
        dungeon_offline(&s_dun, (uint32_t)((now - s_last_ms) / 1000), now);
        save_now();
        enter_mode(M_REPORT);
        s_last_ms = now;
        return;
    }
    s_last_ms = now;
    s_tick++;
    // battle-end beat: hold the final frame so the outcome banner is readable
    if (s_mode == M_BATTLE && s_battle_end_hold) {
        s_battle_end_hold--;
        if (s_battle_end_hold == 0 && s_beat_next_choice) enter_mode(M_CHOICE);
        refresh();
        return;
    }
    if (s_flash) s_flash--;
    if (s_fx_ticks) s_fx_ticks--;
    if (s_player_hurt_ticks) s_player_hurt_ticks--;
    if (s_banner_ticks > 0) s_banner_ticks--;
    for (int k = 0; k < 4; k++) if (s_dmg_ticks[k] > 0) s_dmg_ticks[k]--;
    // Reply beat: the enemies' damage lands visibly after the OC's swing.
    if (s_reply_ms && now >= s_reply_ms) {
        s_reply_ms = 0;
        if (s_pending_taken > 0) {
            popup(3, -s_pending_taken, 0xFF6B5A);
            s_player_hurt_ticks = 5;
            char line[DUNGEON_LOG_LEN];
            snprintf(line, sizeof(line), "< %s hits -%d", foe_name(0), s_pending_taken);
            log_push_ui(line);   // visual only: one hit cue per beat is plenty
        }
        s_pending_taken = 0;
        s_hp_shown = s_dun.hp_cur;
    }
    if (s_mimic_reveal_ticks) s_mimic_reveal_ticks--;
    if (s_attacking) {
        s_attack_frame++;
        if (s_attack_frame >= sprite_anim_frame_count(SPR_ANIM_ATTACK)) {
            s_attack_frame = 0;
            s_attacking = false;
        }
    }
    // Autopilot beats: leave town / rest / report by itself.
    if (s_auto_leave_ms && now >= s_auto_leave_ms && s_dun.autopilot) {
        s_auto_leave_ms = 0;
        if (s_mode == M_REST) {
            enter_mode(M_TOWN);            // shop, then the town beat leaves
        } else if (s_mode == M_TOWN || s_mode == M_REPORT) {
            s_dun.exploring = 1;
            audio_se(CT_SE_OK);
            enter_mode(M_BATTLE);
        }
        save_later();
    }
    if (s_mode == M_CHOICE && s_dun.choice_pending && s_dun.autopilot
        && now + DUNGEON_CHOICE_TIMEOUT_MS >= s_dun.choice_deadline_ms + DUNGEON_CHOICE_AUTO_MS) {
        dungeon_choice_pick(&s_dun, dungeon_choice_auto(&s_dun));
        audio_se(CT_SE_CHOICE);
        save_later();
        enter_mode(M_BATTLE);
    }
    if (s_mode == M_BATTLE && s_dun.exploring) {
        if (s_dun.choice_pending) {
            if (now >= s_dun.choice_deadline_ms) {
                dungeon_choice_timeout(&s_dun, now);
                save_later();
            } else {
                enter_mode(M_CHOICE);
            }
        } else if (s_dun.n_enemy > 0) {
            if (now >= s_round_due_ms) {
                uint8_t ult_before = s_dun.ult_used;
                uint16_t ehp0[3], hp0 = s_dun.hp_cur;
                uint8_t n0 = s_dun.n_enemy;
                uint8_t smoke_before = s_dun.cons[CONS_SMOKE];
                memcpy(ehp0, s_dun.enemy_hp, sizeof(ehp0));
                dungeon_autopilot_battle(&s_dun, now);
                dungeon_event_t e = DEV_NONE;
                if (s_dun.n_enemy) e = dungeon_round(&s_dun, now);
                bool ult_fired = !ult_before && s_dun.ult_used;
                s_round_due_ms = now + UI_ROUND_MS;
                start_attack();
                // Cause and effect: which foes lost HP to the OC this beat.
                s_hurt_mask = 0;
                for (uint8_t i = 0; i < n0 && i < 3; i++) {
                    uint16_t cur = (s_dun.n_enemy || e == DEV_DEAD) ? s_dun.enemy_hp[i] : 0;
                    if (cur < ehp0[i]) {
                        int d = (int)(ehp0[i] - cur);
                        s_hurt_mask |= (uint8_t)(1u << i);
                        popup(i, -d, cur == 0 ? 0xFFD25A : 0xFFFFFF);
                        char line[DUNGEON_LOG_LEN];
                        snprintf(line, sizeof(line), "> %s -%d%s", foe_name(i), d, cur == 0 ? " down" : "");
                        log_push_ui(line);
                    }
                }
                if (s_hurt_mask) s_flash = 4;
                else if (n0 && s_dun.n_enemy) log_push_ui("> miss");
                // The foes' reply is revealed half a beat later.
                if (s_dun.hp_cur < hp0) {
                    s_pending_taken = (int)(hp0 - s_dun.hp_cur);
                    s_reply_ms = now + UI_REPLY_MS;
                } else if (s_dun.hp_cur > hp0) {
                    popup(3, (int)(s_dun.hp_cur - hp0), 0x7CE38B);
                }
                if (ult_fired) {
                    show_fx(ult_fx_for((dungeon_job_t)s_dun.main_job), 10);
                    audio_se(ult_se_for((dungeon_job_t)s_dun.main_job));
                } else if (s_hurt_mask) {
                    audio_se(CT_SE_HIT);
                }
                // v11 UX: EVERY battle end gets an outcome banner + hold —
                // no more silent instant transitions (player report).
                if (e == DEV_CLEAR || e == DEV_CHOICE) {
                    banner(T(S_BANNER_CLEAR), 0xFFD25A);
                    s_battle_end_hold = 8;
                    s_beat_next_choice = (uint8_t)s_dun.choice_pending;
                }
                if (s_dun.n_enemy == 0 && e == DEV_NONE) {   // fled (smoke)
                    banner(T(S_BANNER_SMOKE), 0x7CE3FF);
                    s_battle_end_hold = 10;
                    s_beat_next_choice = (uint8_t)s_dun.choice_pending;
                }
                if (e == DEV_CLEAR) audio_se(CT_SE_FANFARE);
                if (e == DEV_CHOICE) enter_mode(M_CHOICE);
                if (e == DEV_DEAD) {
                    s_reply_ms = 0; s_pending_taken = 0; s_hp_shown = s_dun.hp_cur;
                    audio_se(CT_SE_FAIL); enter_mode(M_REST);
                }
                if (e == DEV_DEAD || e == DEV_CLEAR) save_now();   // milestones
                else if (e != DEV_NONE) save_later();
            }
        } else if (now >= s_explore_due_ms) {
            if (dungeon_autopilot_explore(&s_dun, now)) {   // feather home
                audio_se(CT_SE_RETURN);
                enter_mode(M_TOWN);
                save_now();
                return;
            }
            uint8_t mimic_before = s_dun.mimic_room;
            uint8_t rare_before = s_dun.rare_room;
            uint8_t hidden_before = s_dun.hidden_room;
            uint8_t challenge_before = s_dun.challenge_room;
            uint8_t starfalls_before = s_dun.starfalls;
            uint8_t starfall_pending = s_dun.starfall_next;
            uint16_t hp_before = s_dun.hp_cur;
            dungeon_event_t e = dungeon_explore(&s_dun, now, &s_explore_due_ms);
            if (e == DEV_BATTLE || e == DEV_BOSS) {
                for (int i = 0; i < 3; i++) s_ehp_max[i] = s_dun.enemy_hp[i];
                s_hurt_mask = 0;
                s_round_due_ms = now + UI_ROUND_MS;
            }
            if (s_energy_prev && s_dun.energy == 0) log_push_ui("Exhausted! HP drains");
            if (e == DEV_NONE && s_dun.hp_cur < hp_before && s_dun.energy == 0)
                popup(3, -(int)(hp_before - s_dun.hp_cur), 0xC9A3FF);   // exhaustion tick
            s_energy_prev = s_dun.energy;
            if (!mimic_before && s_dun.mimic_room) {
                s_mimic_reveal_ticks = MIMIC_REVEAL_TICKS;
                s_round_due_ms = now + 2000;
            }
            if (!challenge_before && s_dun.challenge_room)
                show_fx(SPR_EVENT_CURSED_GATE, 10);
            if ((starfall_pending || s_dun.starfalls != starfalls_before)
                && !s_dun.starfall_next)
                show_fx(SPR_EVENT_STARFALL, 14);
            if (e == DEV_DEAD) {
                audio_se(CT_SE_FAIL);
                enter_mode(M_REST);
            } else if (!mimic_before && s_dun.mimic_room) {
                audio_se(CT_SE_MIMIC);
            } else if ((!rare_before && s_dun.rare_room)
                       || (!hidden_before && s_dun.hidden_room)
                       || ((starfall_pending || s_dun.starfalls != starfalls_before)
                           && !s_dun.starfall_next)) {
                audio_se(CT_SE_RARE);
            } else if (e == DEV_TREASURE || e == DEV_REST) {
                audio_se(CT_SE_LOOT);
            } else if (e == DEV_TRAP) {
                audio_se(CT_SE_WARN);
            } else if (e == DEV_CHOICE) {
                audio_se(CT_SE_CHOICE);
            }
            if (e == DEV_CHOICE) enter_mode(M_CHOICE);
            if (e == DEV_DEAD) save_now();
            else if (e != DEV_NONE) save_later();
        }
    } else if (s_mode == M_CHOICE && s_dun.choice_pending
               && now >= s_dun.choice_deadline_ms) {
        dungeon_choice_timeout(&s_dun, now);
        enter_mode(M_BATTLE);
    } else if (s_mode == M_CHOICE && !s_dun.choice_pending) {
        enter_mode(M_BATTLE);
    }
    save_if_due(now);
    check_milestones();
    refresh();
}

static lv_obj_t *shadow(int x, int y, int w)
{
    lv_obj_t *o = lv_obj_create(s_scr);
    lv_obj_set_size(o, w, 10);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, 5, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_50, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    return o;
}

void demo_deskpet_enter(void)
{
    pet_save_defaults(&s_pet);
    dungeon_new_game(&s_dun, (uint32_t)now_ms());
    pomo_defaults(&s_pomo);
    s_lang = LANG_EN;
    bool loaded = deskpet_store_load(&s_pet, &s_dun, &s_pomo, &s_lang) == ESP_OK;
    if (!loaded)
        ESP_LOGI(TAG, "no save, fresh start");
    if (s_dun.version != DUNGEON_MODEL_VERSION) {
        dungeon_new_game(&s_dun, (uint32_t)now_ms());
        loaded = false;
        ESP_LOGI(TAG, "save version mismatch, fresh v3 start");
    }
    dungeon_rebase_clock(&s_dun, now_ms());  // saved deadlines were boot-relative
    deskpet_set_lang(s_lang);
    s_dirty = false;
    s_last_save_ms = now_ms();
    s_cursor = 0;
    s_tick = 0;
    s_flash = 0;
    s_attack_frame = 0;
    s_battle_end_hold = 0;
    s_fx_ticks = 0;
    s_mimic_reveal_ticks = 0;
    s_attacking = false;
    s_fx_sprite = SPR_EVENT_STARFALL;
    s_last_ms = now_ms();
    s_round_due_ms = s_last_ms;
    s_explore_due_ms = s_last_ms;

    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x0B0B1A), 0);
    lv_obj_set_style_text_color(s_scr, lv_color_hex(0xE8E8F0), 0);

    s_bg = lv_img_create(s_scr);
    img_set(s_bg, sprite_get(SPR_BG_ROOM));
    lv_obj_set_pos(s_bg, 0, 26);

    s_gold = lv_label_create(s_scr);
    lv_obj_set_width(s_gold, 90);
    lv_obj_set_pos(s_gold, 4, 4);
    lv_obj_set_style_text_color(s_gold, lv_color_hex(0xFFD25A), 0);
    s_stage = lv_label_create(s_scr);
    lv_obj_set_pos(s_stage, 100, 4);
    s_jobtext = lv_label_create(s_scr);
    lv_obj_set_pos(s_jobtext, 150, 4);
    lv_obj_set_width(s_jobtext, 86);

    shadow(22, 188, 56);
    shadow(158, 186, 48);
    s_player = lv_img_create(s_scr);
    lv_obj_set_pos(s_player, 18, 124);
    static const int MX[3] = {124, 162, 200};
    for (int i = 0; i < 3; i++) {
        s_mon[i] = lv_img_create(s_scr);
        lv_obj_set_pos(s_mon[i], MX[i], 146);
        lv_img_set_zoom(s_mon[i], 192);
        lv_image_set_antialias(s_mon[i], false);
        s_rare[i] = lv_img_create(s_scr);
        img_set(s_rare[i], sprite_get(SPR_EVENT_RARE_SPARKLE));
        lv_obj_set_pos(s_rare[i], MX[i] + 12, 136);
        show(s_rare[i], false);
    }
    s_overlay = lv_img_create(s_scr);
    show(s_overlay, false);
    s_ui_icon = lv_img_create(s_scr);
    lv_obj_set_pos(s_ui_icon, 216, 4);
    s_face = lv_img_create(s_scr);
    lv_obj_set_pos(s_face, 24, 62);

    s_hpbar = lv_bar_create(s_scr);
    lv_obj_set_size(s_hpbar, 80, 12);
    lv_obj_set_pos(s_hpbar, 26, 206);
    lv_obj_set_style_bg_color(s_hpbar, lv_color_hex(0xE43B2F), LV_PART_INDICATOR);
    s_enbar = lv_bar_create(s_scr);
    lv_obj_set_size(s_enbar, 80, 8);
    lv_obj_set_pos(s_enbar, 116, 208);
    lv_obj_set_style_bg_color(s_enbar, lv_color_hex(0x3FA7FF), LV_PART_INDICATOR);
    s_hptext = lv_label_create(s_scr);
    lv_obj_set_pos(s_hptext, 26, 216);
    s_ico_ess = lv_img_create(s_scr);
    img_set(s_ico_ess, sprite_get(SPR_UI_RESOURCE_ESSENCE));
    lv_obj_set_pos(s_ico_ess, 100, 215);
    s_ess_txt = lv_label_create(s_scr);
    lv_obj_set_pos(s_ess_txt, 118, 218);
    s_ico_fea = lv_img_create(s_scr);
    img_set(s_ico_fea, sprite_get(SPR_UI_ITEM_FEATHER));
    lv_obj_set_pos(s_ico_fea, 142, 215);
    s_fea_txt = lv_label_create(s_scr);
    lv_obj_set_pos(s_fea_txt, 160, 218);
    s_meta = lv_label_create(s_scr);
    lv_obj_set_pos(s_meta, 170, 218);
    lv_obj_set_width(s_meta, 70);
    lv_label_set_long_mode(s_meta, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(s_meta, lv_color_hex(0x9FB4D8), 0);
    lv_obj_set_style_text_color(s_ess_txt, lv_color_hex(0x9FB4D8), 0);
    lv_obj_set_style_text_color(s_fea_txt, lv_color_hex(0x9FB4D8), 0);
    for (int i = 0; i < 3; i++) {
        s_ehp[i] = lv_bar_create(s_scr);
        lv_obj_set_size(s_ehp[i], 40, 4);
        lv_obj_set_pos(s_ehp[i], MX[i] + 2, 140);
        lv_obj_set_style_bg_color(s_ehp[i], lv_color_hex(0x2A1A1A), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_ehp[i], lv_color_hex(0xE43B2F), LV_PART_INDICATOR);
        show(s_ehp[i], false);
    }
    for (int k = 0; k < 4; k++) {
        s_dmg[k] = lv_label_create(s_scr);
        show(s_dmg[k], false);
        s_dmg_ticks[k] = 0;
    }
    for (int i = 0; i < 3; i++) {
        s_elv[i] = lv_label_create(s_scr);
        lv_obj_set_pos(s_elv[i], MX[i] + 2, 128);
        show(s_elv[i], false);
    }
    s_entext = lv_label_create(s_scr);
    lv_obj_set_pos(s_entext, 200, 204);
    lv_obj_set_style_text_color(s_entext, lv_color_hex(0x9FC8FF), 0);
    s_dim = lv_obj_create(s_scr);
    lv_obj_set_size(s_dim, 240, 176);
    lv_obj_set_pos(s_dim, 0, 26);
    lv_obj_set_style_radius(s_dim, 0, 0);
    lv_obj_set_style_border_width(s_dim, 0, 0);
    lv_obj_set_style_bg_color(s_dim, lv_color_hex(0x000000), 0);
    show(s_dim, false);
    s_banner = lv_label_create(s_scr);
    lv_obj_set_width(s_banner, 240);
    lv_obj_set_style_text_align(s_banner, LV_TEXT_ALIGN_CENTER, 0);
    show(s_banner, false);
    s_banner_ticks = 0;
    s_prev_floor = s_dun.floor; s_prev_ng = s_dun.ng; s_prev_xlv = s_dun.explore_lv;
    s_prev_unlocked = s_dun.unlocked; memcpy(s_prev_joblv, s_dun.job_lv, sizeof(s_prev_joblv));
    s_hurt_mask = 0; s_player_hurt_ticks = 0; s_pending_taken = 0; s_reply_ms = 0;
    s_hp_shown = s_dun.hp_cur; s_energy_prev = s_dun.energy;
    for (int i = 0; i < 3; i++) s_ehp_max[i] = s_dun.enemy_hp[i];
    s_menu = lv_label_create(s_scr);
    lv_obj_set_width(s_menu, 230);
    lv_obj_set_height(s_menu, 44);          // 3 rows max, clipped, never over the log
    lv_obj_set_pos(s_menu, 4, 230);
    s_log = lv_label_create(s_scr);
    lv_obj_set_width(s_log, 230);
    lv_obj_set_height(s_log, 46);           // 4 rows at the 10 px font
    lv_obj_set_pos(s_log, 4, 274);

    s_timer = lv_timer_create(tick, 100, NULL);
    if (!loaded) s_dun.exploring = 1;
    // v11 fix: go THROUGH enter_mode — it arms the autopilot town beat.
    // The old direct s_mode assignment left s_auto_leave_ms at 0, so a save
    // made in town booted into town and never descended (player report).
    s_pairA = s_dun.main_job;
    s_pairB = s_dun.sub_job;
    apply_fonts();
    lv_screen_load(s_scr);
    enter_mode(s_dun.choice_pending ? M_CHOICE
                                    : (s_dun.exploring ? M_BATTLE : M_TOWN));
    refresh();
}

void demo_deskpet_exit(void)
{
    atomic_store(&s_audio_scene, CT_SCENE_NONE);
    save_now();
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_gold = s_stage = s_hpbar = s_hptext = s_enbar = s_jobtext = s_meta = NULL;
        s_ico_ess = s_ico_fea = s_ess_txt = s_fea_txt = NULL;
        for (int i = 0; i < 3; i++) s_ehp[i] = s_elv[i] = NULL;
        for (int k = 0; k < 4; k++) s_dmg[k] = NULL;
        s_banner = s_dim = s_entext = NULL;
        s_bg = s_player = s_face = s_overlay = s_ui_icon = NULL;
        for (int i = 0; i < 3; i++) s_mon[i] = s_rare[i] = NULL;
        s_menu = s_log = NULL;
    }
}

static bool lead_to(int job, uint64_t now)
{
    if (job < 0 || job >= (int)DUNGEON_JOB_COUNT || s_dun.main_job == (uint8_t)job) return false;
    if (now < s_dun.switch_ready_ms) return false;
    if (!((s_dun.unlocked >> job) & 1)) return false;
    // new pair = (job, current sub); fall back to current main if identical.
    uint8_t other = s_dun.sub_job;
    if (other == (uint8_t)job) other = s_dun.main_job;
    if (!dungeon_town(&s_dun, TOWN_SWAP, (uint8_t)(((uint8_t)job << 4) | other))) return false;
    s_dun.switch_ready_ms = now + DUNGEON_SWITCH_CD_MS;
    s_pairA = s_dun.main_job;
    s_pairB = s_dun.sub_job;
    return true;
}

// BGM scene for the audio task: battle picks boss/battle by gate flag.
int demo_deskpet_audio_scene(void)
{
    return atomic_load(&s_audio_scene);
}

void demo_deskpet_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    uint64_t now = now_ms();
    s_last_ms = now;
    // V2 playback latch (§11): any key in a battle — or in a mid-battle
    // choice card — hands the battle to the player for its remaining duration.
    if (s_mode == M_BATTLE || (s_mode == M_CHOICE && s_dun.n_enemy > 0)) {
        if (!s_manual_battle) {
            s_manual_battle = true;
            if (s_dun.autopilot) log_push_ui(T(S_MANUAL_TAKEOVER));
        }
    }
    if (ev == BSP_BTN_LONG) {
        // NOTE: OK-long is global (mode switcher, owned by app_shell) and
        // never reaches here. UP/DOWN-long switch job lead (game rule).
        // Town access: battle 4th row (cursor 3) or feather/double-OK.
        bool changed = false;
        if (btn == BSP_BTN_UP) changed = lead_to(s_pairA, now);
        else if (btn == BSP_BTN_DOWN) changed = lead_to(s_pairB, now);
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN)
            audio_se(changed ? CT_SE_CHOICE : CT_SE_WARN);
        refresh();
        return;
    }
    if (ev == BSP_BTN_DOUBLE) {
        if (btn == BSP_BTN_OK) {
            if (dungeon_use_cons(&s_dun, CONS_FEATHER)) {
                audio_se(CT_SE_RETURN);
                enter_mode(M_TOWN);
            } else {
                audio_se(CT_SE_WARN);
            }
        }
        refresh();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;
    s_auto_leave_ms = 0;   // the player is browsing: hold the auto-leave
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        int d = (btn == BSP_BTN_UP) ? -1 : 1;
        if (s_mode == M_BATTLE) s_cursor = (s_cursor + d + 4) % 4;
        else if (s_mode == M_TOWN) s_cursor = (s_cursor + d + 9) % 9;
        else if (s_mode == M_CHOICE && s_dun.choice_pending)
            s_dun.choice_cursor = (uint8_t)(s_dun.choice_cursor + d + 3) % 3;
        audio_se(CT_SE_OK);
        refresh();
        return;
    }
    if (btn != BSP_BTN_OK) return;
    if (s_mode == M_BATTLE) {
        if (s_cursor == 3) {
            // 4th row: home to town (OK-long belongs to the shell now).
            s_dun.exploring = 0;
            audio_se(CT_SE_RETURN);
            enter_mode(M_TOWN);
            return;
        }
        uint8_t cd_before = s_dun.skill_cd[s_cursor];
        uint8_t enemies_before = s_dun.n_enemy;
        uint16_t base_pow = dungeon_pow(&s_dun);
        int dmg = dungeon_cast(&s_dun, (uint8_t)s_cursor, now);
        bool cast = dmg > 0 || (cd_before == 0 && s_dun.skill_cd[s_cursor] > 0);
        if (cast) {
            s_flash = 4;
            start_attack();
            audio_se(dmg > (int)(base_pow * 3u / 2u) ? CT_SE_CRIT
                                                     : (dmg > 0 ? CT_SE_HIT : CT_SE_CHOICE));
            if (enemies_before && !s_dun.n_enemy) audio_se(CT_SE_FANFARE);
        } else {
            audio_se(CT_SE_WARN);
        }
        if (s_dun.choice_pending) enter_mode(M_CHOICE);
    } else if (s_mode == M_TOWN) {
        bool ok = false;
        ct_se_t cue = CT_SE_LOOT;
        if (s_cursor == 0) {
            s_dun.autopilot = s_dun.autopilot ? 0 : 1;
            s_dun.auto_job = s_dun.autopilot;
            ok = true;
            cue = CT_SE_CHOICE;
        } else if (s_cursor == 1) {
            ok = dungeon_town(&s_dun, TOWN_UPGRADE, (uint8_t)s_upsel);
            s_upsel = (s_upsel + 1) % 4;   // always advance: browse wpn/arm/smoke/pot
        } else if (s_cursor == 2) {
            ok = dungeon_town(&s_dun, TOWN_SYNTH, (uint8_t)s_synsel);
            if (ok) s_synsel ^= 1;
        } else if (s_cursor == 3) {
            ok = dungeon_town(&s_dun, TOWN_SANCTUM, (uint8_t)s_sansel);
            s_sansel = (s_sansel + 1) % SN_COUNT;   // browse tracks even when short
        } else if (s_cursor == 4) {
            ok = dungeon_town(&s_dun, TOWN_INN, 0);
        } else if (s_cursor == 7) {
            // cycle sub through unlocked jobs
            for (int j = 1; j <= (int)DUNGEON_JOB_COUNT; j++) {
                uint8_t cand = (uint8_t)((s_dun.sub_job + j) % DUNGEON_JOB_COUNT);
                if (cand != s_dun.main_job && ((s_dun.unlocked >> cand) & 1)) {
                    ok = dungeon_town(&s_dun, TOWN_SWAP,
                                      (uint8_t)((s_dun.main_job << 4) | cand));
                    break;
                }
            }
            s_pairA = s_dun.main_job;
            s_pairB = s_dun.sub_job;
            cue = CT_SE_CHOICE;
        } else if (s_cursor == 5) {
            ok = dungeon_town(&s_dun, TOWN_WISH, 0);
            if (ok) {
                show_fx(SPR_EVENT_WISHING_WELL, 12);
                cue = CT_SE_RARE;
            }
        } else if (s_cursor == 6) {
            ok = dungeon_town(&s_dun, TOWN_PRESTIGE, 0);
            cue = CT_SE_FANFARE;
        } else {
            s_dun.exploring = 1;
            ok = true;
            cue = CT_SE_OK;
            enter_mode(M_BATTLE);
        }
        audio_se(ok ? cue : CT_SE_WARN);
    } else if (s_mode == M_CHOICE) {
        bool challenge = s_dun.choice_pending
            && s_dun.choice_opts[s_dun.choice_cursor] == CH_CHALLENGE;
        if (s_dun.choice_pending) {
            dungeon_choice_pick(&s_dun, s_dun.choice_cursor);
            audio_se(CT_SE_CHOICE);
        } else {
            audio_se(CT_SE_WARN);
        }
        if (challenge) show_fx(SPR_EVENT_CURSED_GATE, 12);
        enter_mode(M_BATTLE);
    } else if (s_mode == M_REPORT || s_mode == M_REST) {
        s_dun.exploring = 1;
        audio_se(CT_SE_OK);
        enter_mode(M_BATTLE);
    }
    save_now();   // user actions are rare and worth a flush
    refresh();
}

#ifndef ESP_PLATFORM
void demo_deskpet_debug_floor(unsigned floor)
{
    if (floor >= 1 && floor <= DUNGEON_MAX_FLOOR) {
        s_dun.floor = (uint8_t)floor;
        refresh();
    }
}

void demo_deskpet_debug_art(const char *scene)
{
    if (!scene) return;
    if (!strncmp(scene, "job", 3)) {
        unsigned job = (unsigned)(scene[3] - '0');
        if (job < DUNGEON_JOB_COUNT) {
            s_dun.main_job = (uint8_t)job;
            s_dun.unlocked |= (uint8_t)(1u << job);
            s_dun.auto_job = 0;  // art QA must not swap the requested job
        }
    } else if (!strcmp(scene, "walk")) {
        s_dun.n_enemy = 0;
        s_dun.rare_room = 0;
        s_dun.mimic_room = 0;
        s_dun.hidden_room = 0;
        s_dun.exploring = 1;
        s_attacking = false;
        s_fx_ticks = 0;
        enter_mode(M_BATTLE);
    } else if (!strcmp(scene, "attack")) {
        s_dun.n_enemy = 1;
        s_dun.enemy_kind[0] = 0;
        s_dun.enemy_hp[0] = 999;
        s_dun.rare_room = 0;
        s_dun.mimic_room = 0;
        s_dun.hidden_room = 0;
        s_dun.exploring = 1;
        s_fx_ticks = 0;
        start_attack();
        enter_mode(M_BATTLE);
    } else if (!strcmp(scene, "ult")) {
        show_fx(ult_fx_for((dungeon_job_t)s_dun.main_job), 30);
        enter_mode(M_BATTLE);
    } else if (!strcmp(scene, "rare")) {
        s_dun.n_enemy = 1;
        s_dun.enemy_kind[0] = 2;
        s_dun.enemy_hp[0] = 999;
        s_dun.rare_room = 1;
        s_dun.mimic_room = 0;
        s_dun.hidden_room = 0;
        s_fx_ticks = 0;
        enter_mode(M_BATTLE);
    } else if (!strcmp(scene, "mimic")) {
        s_dun.n_enemy = 1;
        s_dun.enemy_kind[0] = 0;
        s_dun.enemy_hp[0] = 999;
        s_dun.rare_room = 0;
        s_dun.mimic_room = 1;
        s_dun.hidden_room = 0;
        s_fx_ticks = 0;
        s_mimic_reveal_ticks = MIMIC_REVEAL_TICKS;
        enter_mode(M_BATTLE);
    } else if (!strcmp(scene, "gate")) {
        s_dun.n_enemy = 0;
        s_dun.rare_room = 0;
        s_dun.mimic_room = 0;
        s_dun.hidden_room = 0;
        s_fx_ticks = 0;
        s_dun.choice_pending = 1;
        s_dun.choice_cursor = 0;
        s_dun.choice_opts[0] = CH_CHALLENGE;
        s_dun.choice_deadline_ms = now_ms() + 60000;
        enter_mode(M_CHOICE);
    } else if (!strcmp(scene, "starfall")) {
        s_dun.n_enemy = 0;
        s_dun.rare_room = 0;
        s_dun.mimic_room = 0;
        s_dun.hidden_room = 0;
        show_fx(SPR_EVENT_STARFALL, 30);
        enter_mode(M_BATTLE);
    } else if (!strcmp(scene, "town")) {
        s_dun.exploring = 0;
        enter_mode(M_TOWN);
    }
    refresh();
}
#endif
