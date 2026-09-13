// main/dungeon_model.c —— GAMEPLAY v4 infinite-maze crawler (docs/GAMEPLAY_V4.md).
// v3 base (seeded RNG, table jobs, behavior AI, choice cards, town, offline)
// plus: 100-floor depth curve, essence/shards meta economy (sanctum), gear
// rarity+affixes with forge synthesis, 5th job (dark) and autonomous job swap.
#include "dungeon_model.h"

#include <stdio.h>
#include <string.h>

// ---- tunable knobs (autotune) ----------------------------------------------
// Defaults are the hand-tuned v4.5 values; tools/autotune.py overrides them
// via -DTUN_* to search the space. Firmware behavior = defaults, always.
#ifndef TUN_ATK_NUM          // enemy atk = 2 + floor*ATK_NUM/5 + badge*ATK_BADGE
#define TUN_ATK_NUM 4
#endif
#ifndef TUN_ATK_BADGE          // v12.1: 10->12 — a working economy clears mazes
#define TUN_ATK_BADGE 12       // faster; steeper per-badge scaling keeps total
#endif                         // maze consumption bounded (sim: 34 -> 17/150)
#ifndef TUN_ELITE_LO         // elite spawn % at F>=15 / F>=40
#define TUN_ELITE_LO 12
#endif
#ifndef TUN_ELITE_HI
#define TUN_ELITE_HI 20
#endif
#ifndef TUN_SHARD_COST       // synthesis shards = COST*(rar+1)
#define TUN_SHARD_COST 5
#endif
#ifndef TUN_BOSS_SHARDS      // shards per gate boss
#define TUN_BOSS_SHARDS 1
#endif
#ifndef TUN_FORGE_LIN        // forge cost = 10 + LIN*s + s*s/SQ (s=w+a tier)
#define TUN_FORGE_LIN 3
#endif
#ifndef TUN_FORGE_SQ            // v12.1: /3 — the both-piece forge fix doubled
#define TUN_FORGE_SQ 3          // gold->power throughput; the steeper curve
#endif                          // restores the pre-fix pacing (m10=77)
#ifndef TUN_PRESTIGE_DIV     // rebirth needs ng >= 1 + lv/DIV
#define TUN_PRESTIGE_DIV 6   // v12.3: 4->6 - late rebirth every ~2-3 badges
#endif                       // instead of 4-5: north star showed the late
                             // arc at 0.5 milestones/h (anticipation desert)
#ifndef TUN_EXPLORE_NEED     // explore level-up cost = lv*NEED
#define TUN_EXPLORE_NEED 14
#endif
#ifndef TUN_GATE_ESSENCE     // essence per gate boss clear
#define TUN_GATE_ESSENCE 3
#endif
#ifndef TUN_SANCTUM_BASE     // geometric sanctum cost: BASE*(3/R2)^lv
#define TUN_SANCTUM_BASE 4
#endif
#ifndef TUN_SANCTUM_R2       // r = 3/R2 (default 1.5); RESEARCH.md 4
#define TUN_SANCTUM_R2 2
#endif
#ifndef TUN_MIMIC_CHANCE     // % of chests that are mimics (thief: half)
#define TUN_MIMIC_CHANCE 20
#endif
#ifndef TUN_UNDEAD_DRAIN     // undead heals taken/DRAIN on hit (0 = off)
#define TUN_UNDEAD_DRAIN 3
#endif
#ifndef TUN_RARE_PPM         // rare-foe spawn chance in ppm (normal rooms, F>=15)
#define TUN_RARE_PPM 100
#endif
#ifndef TUN_HIDDEN_PCT       // % chance a F25/50/75 gate hides the ancient boss
#define TUN_HIDDEN_PCT 15     // sens study: bounded up to 15 (ng+prst stays ~10-11),
#endif                        // first hidden 5.2h->2.2h; 39% per 100-floor maze
#ifndef TUN_WISH_COST        // essence per wish (v5.2 gacha)
#define TUN_WISH_COST 25
#endif
#ifndef TUN_WISH_GOLD_BASE   // v12.1: wishes also cost gold — after the forge
#define TUN_WISH_GOLD_BASE 50 // caps (~run 20) gold income has no sink and
#endif                        // piles to 40k+/run; the well is the drain
#ifndef TUN_WISH_GOLD_STEP   // gold/wish = BASE + STEP*(prestige+ng): scales
#define TUN_WISH_GOLD_STEP 30 // with meta so deep runs drain proportionally
#endif
#ifndef TUN_WISH_GOLD_TAX    // + gold/TAX per wish: progressive tax so a
#define TUN_WISH_GOLD_TAX 8  // hoarded pile never survives a town visit
#endif
#ifndef TUN_PRICE_SMOKE      // peddler price = BASE + floor: deep escapes
#define TUN_PRICE_SMOKE 15   // cost real money, so smoke stops being a free
#endif                        // chest handout (was: no purchase path at all)
// F-template job mastery (playbook balance_sim probes; job_lv>=12 unlocks)
#ifndef TUN_KNIGHT_MASTERY_LV
#define TUN_KNIGHT_MASTERY_LV 12
#endif
#ifndef TUN_KNIGHT_SHIELD_BONUS
#define TUN_KNIGHT_SHIELD_BONUS 2   // start shield 6 -> 8
#endif
#ifndef TUN_WHITE_MASTERY_LV
#define TUN_WHITE_MASTERY_LV 12
#endif
#ifndef TUN_WHITE_POTION_BONUS_PCT
#define TUN_WHITE_POTION_BONUS_PCT 50  // potion 40% -> 90% max HP
#endif
#ifndef TUN_BLACK_MASTERY_LV
#define TUN_BLACK_MASTERY_LV 12
#endif
#ifndef TUN_BLACK_HARVEST_ESSENCE
#define TUN_BLACK_HARVEST_ESSENCE 1    // +essence per kill
#endif
#ifndef TUN_THIEF_MASTERY_LV
#define TUN_THIEF_MASTERY_LV 12
#endif
#ifndef TUN_THIEF_RATION_SMOKE
#define TUN_THIEF_RATION_SMOKE 2       // dive kit smoke count at mastery
#endif
#ifndef TUN_DARK_MASTERY_LV
#define TUN_DARK_MASTERY_LV 12
#endif
#ifndef TUN_DARK_RATION_ESSENCE
#define TUN_DARK_RATION_ESSENCE 15     // dive kit essence at mastery
#endif
#ifndef TUN_PRICE_POT
#define TUN_PRICE_POT 20
#endif
#ifndef TUN_ULT_LV           // evolution unlock job level (cap is 10)
#define TUN_ULT_LV 8          // sens study: lv8 = evo 1.5h->1.3h AND badge1 -11%;
#endif                        // pacing-free fun, ladder 2/5/8-ult/10
#ifndef TUN_WISH_ANCIENT_PCT // base % a wish yields an ancient
#define TUN_WISH_ANCIENT_PCT 2
#endif
#ifndef TUN_WISH_PITY        // wishes since last ancient -> guaranteed ancient
#define TUN_WISH_PITY 25
#endif
// (V2 stance knobs live in dungeon_model.h so unit tests share tuned defaults.)
#ifndef TUN_FLYER_EVADE      // % fliers evade non-magic basic attacks
#define TUN_FLYER_EVADE 15
#endif
#ifndef TUN_CURSE_PCT        // undead curse: pow x(100-PCT)/100 while cursed
#define TUN_CURSE_PCT 20
#endif
#ifndef TUN_WHITE_REGEN      // white mage self-regen base (adds job_lv/2)
#define TUN_WHITE_REGEN 2
#endif
#ifndef TUN_GROWTH_DIV       // per-job C-growth divisor (FFT C-track)
#define TUN_GROWTH_DIV 4
#endif
#ifndef TUN_PLAYER_DMG_NUM   // global player damage scale (xNUM/100)
#define TUN_PLAYER_DMG_NUM 130 // v35: +30% all player damage — round p50
#endif                         // 20 was too long (70% of session is watching)

#ifndef TUN_EN_DMG_NUM       // global enemy damage scale (xNUM/10 on blob)
#define TUN_EN_DMG_NUM 8     // v12: 10 over-taxed the potion economy
#endif
#ifndef TUN_BLOB_GRD_DIV     // v12.2: guard past the softcap counts /DIV
#define TUN_BLOB_GRD_DIV 2   // toward the blob denominator
#endif
#ifndef TUN_GRD_SOFTCAP      // guard up to here counts fully (fresh bootstrap
#define TUN_GRD_SOFTCAP 20   // keeps its only defense); excess softens
#endif
#ifndef TUN_DESC_HEAL        // % max HP restored per floor descended
#define TUN_DESC_HEAL 35      // sweep: with EN_DMG 8 -> badge 60%, fresh 13
#endif
#ifndef TUN_STARFALL_PPM     // starfall chance per explore step, per-10000
#define TUN_STARFALL_PPM 4   // 4/10000 = 0.04% (~1 per 2.8h ambient)
#endif

static uint32_t rng_next(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

static uint32_t rng_range(uint32_t *s, uint32_t n) { return n ? rng_next(s) % n : 0; }

// Saturating counters: u16 economy must never wrap on month-long saves.
#define SAT_CAP 60000u
static uint16_t sat_add(uint16_t a, uint16_t b)
{
    uint32_t r = (uint32_t)a + b;
    return (uint16_t)(r > SAT_CAP ? SAT_CAP : r);
}
static uint16_t sat_inc(uint16_t a) { return a >= SAT_CAP ? (uint16_t)SAT_CAP : (uint16_t)(a + 1); }

// Job chart (GAMEPLAY_V5 5.4): three visible M axes + AI + skills. The
// spread IS the identity: knight walls, black nukes, white sustains,
// thief gambles, dark executes.
static const dungeon_job_def_t JOB_DEFS[DUNGEON_JOB_COUNT] = {
    [JOB_KNIGHT] = {AI_CHARGE, {SK_SHIELD_BASH, SK_TAUNT}, 1, 3, 1, 2, 100, 120, 130,
                    AF_GUARD, SK_CASTLE},
    // v29 B1 charter: survival ladder — hp/guard growth + M axes stagger the
    // attrition-ratio crossing floors per identity (target medians
    // black 8 / thief 11 / white 14 / dark 17 / knight 20).
    [JOB_BLACK]  = {AI_SNIPER, {SK_FIRE, SK_FROST}, 1, 2, 3, 0, 120, 100, 75,
                    AF_SLAYER, SK_METEOR},
    [JOB_WHITE]  = {AI_MEDIC, {SK_CURE, SK_AEGIS}, 6, 4, 1, 1, 85, 110, 105,
                    AF_LEECH, SK_SANCTUARY},
    [JOB_THIEF]  = {AI_BACKSTAB, {SK_VENOM, SK_SMOKEOUT}, 16, 3, 2, 3, 110, 90, 100,
                    AF_GREED, SK_JACKPOT},
    [JOB_DARK]   = {AI_EXECUTE, {SK_DRAIN, SK_DOOM}, 41, 4, 2, 3, 110, 105, 90,
                    AF_VENOM, SK_ECLIPSE},
};

static const char *const JOB_NAMES[DUNGEON_JOB_COUNT] = {
    "Knight", "Black Mage", "White Mage", "Thief", "Dark",
};

static const char *const SKILL_NAMES[SK_COUNT] = {
    "-", "Shield Bash", "Taunt",
    "Fire", "Frost",
    "Cure", "Aegis",
    "Venom Blade", "Smoke Out",
    "Drain", "Doom",
    "Castle", "Meteor", "Sanctuary", "Jackpot", "Eclipse",
};

// Restraint: attacker job x defender kind -> x1.5 / x0.7 / x1.0 (*10 fixed).
// kinds: 0 beast 1 flier 2 undead 3 boss.
static const uint8_t RESTRAIN[DUNGEON_JOB_COUNT][4] = {
    [JOB_KNIGHT] = {15, 7, 10, 10},
    [JOB_BLACK]  = {10, 15, 7, 10},
    [JOB_WHITE]  = {7, 10, 15, 7},
    [JOB_THIEF]  = {15, 10, 10, 7},
    [JOB_DARK]   = {10, 7, 15, 10},
};

const dungeon_job_def_t *dungeon_job_def(dungeon_job_t job)
{
    return (job < DUNGEON_JOB_COUNT) ? &JOB_DEFS[job] : &JOB_DEFS[0];
}

const char *dungeon_job_name_en(dungeon_job_t job)
{
    return (job < DUNGEON_JOB_COUNT) ? JOB_NAMES[job] : "?";
}

const char *dungeon_skill_name_en(dungeon_skill_t sk)
{
    return (sk < SK_COUNT) ? SKILL_NAMES[sk] : "?";
}

uint8_t dungeon_skill_unlocked(const dungeon_save_t *s, dungeon_job_t job, uint8_t slot)
{
    if (job >= DUNGEON_JOB_COUNT || slot > 1) return 0;
    uint8_t lv = s->job_lv[job];
    if (slot == 0) return lv >= 2;
    return lv >= 5;
}

static void log_push(dungeon_save_t *s, const char *msg)
{
    for (int i = DUNGEON_LOG_LINES - 1; i > 0; i--)
        memcpy(s->log[i], s->log[i - 1], DUNGEON_LOG_LEN);
    snprintf(s->log[0], DUNGEON_LOG_LEN, "%s", msg);
}

// ---- gear rarity -----------------------------------------------------------
uint16_t dungeon_item_eff(uint8_t tier, uint8_t rar)
{
    if (rar > DUNGEON_RAR_MAX) rar = DUNGEON_RAR_MAX;
    return (uint16_t)((uint32_t)tier * (10 + 5 * rar) / 10);
}

bool dungeon_item_has_affix(uint8_t packed, dungeon_affix_t af)
{
    if (af == AF_NONE || af >= AF_COUNT) return false;
    return (uint8_t)(packed & 0xF) == af || (uint8_t)(packed >> 4) == af;
}

static uint8_t affix_count(const dungeon_save_t *s, dungeon_affix_t af)
{
    uint8_t n = 0;
    if (dungeon_item_has_affix(s->weapon_affix, af)) n++;
    if (dungeon_item_has_affix(s->armor_affix, af)) n++;
    return n;
}

// Collection milestones (v5.2): every 6 codex entries = +2% pow & HP, cap +4%.
// The codex bitmap IS the collection track — no extra save fields.
// v13: bitmap is 64-bit; entries past the +4% cap are pure collection
// (north-star events), so lanes are balance-neutral by construction.
static uint8_t popcount8(uint64_t v)
{
    uint8_t n = 0;
    while (v) { n += (uint8_t)(v & 1); v >>= 1; }
    return n;
}

static uint8_t codex_lv(const dungeon_save_t *s)
{
    uint8_t lv = popcount8(s->codex) / 6;
    return lv > 2 ? 2 : lv;
}

uint8_t codex_milestone_lv_for_test(const dungeon_save_t *s)   // host-test hook
{
    return codex_lv(s);
}

// ---- v13 per-maze codex lanes (lever L8): each ng maze has 4 firsts -------
// bit 16 + 4*(maze-1) + lane, maze 1..12, lane: 0 gate boss / 1 maze clear /
// 2 rare foe / 3 ancient. Gives every new maze fresh collection moments so
// the late arc keeps new-content events (north star: mid/late was 0.5/h).
static void codex_maze_first(dungeon_save_t *s, int lane)
{
    if (s->ng < 1) return;                       // maze 1 firsts live in bits 0-15
    int m = s->ng > 12 ? 12 : s->ng;
    s->codex |= (uint64_t)1 << (16 + 4 * (m - 1) + lane);
}

// Pack up to 2 distinct affixes (rar n grants min(n,2)); keeps old ones first.
static uint8_t affix_pack(uint32_t *rng, uint8_t rar, uint8_t old)
{
    uint8_t keep[2] = {(uint8_t)(old & 0xF), (uint8_t)(old >> 4)};
    uint8_t out[2] = {0, 0};
    uint8_t n = rar > 2 ? 2 : rar;
    uint8_t used = 0;
    for (uint8_t i = 0; i < 2 && used < n; i++) {
        if (keep[i]) { out[used++] = keep[i]; }
    }
    while (used < n) {
        uint8_t a = (uint8_t)(1 + rng_range(rng, AF_COUNT - 1));
        if (a != out[0] && a != out[1]) out[used++] = a;
    }
    return (uint8_t)(out[0] | (out[1] << 4));
}

// Depth+luck rarity roll: white 18%+f/3+3*luck, blue/green fold by half.
// Ancient relics (rar 4): ultra-rare, deep floors only (~0.6% + 0.2%/luck).
static uint8_t roll_rar(dungeon_save_t *s)
{
    if (s->floor >= 60) {
        uint32_t ancient = 60 + 20 * s->sanctum[SN_LUCK];   // per-10000
        if (rng_range(&s->rng_state, 10000) < ancient) {
            s->codex |= (1u << 13);   // first relic logs the collection entry
            return 4;
        }
    }
    uint32_t p = 18 + s->floor / 3 + 3 * s->sanctum[SN_LUCK];
    if (p > 80) p = 80;
    uint32_t r = rng_range(&s->rng_state, 100);
    if (r < p / 4) return 3;
    if (r < p / 2) return 2;
    if (r < p) return 1;
    return 0;
}

// ---- derived stats ---------------------------------------------------------
static uint16_t pow_of(const dungeon_save_t *s)
{
    // 势 = (探索级 + 主职级 + 武器实际值 + 祝福) × 圣所力量；空精力 ×0.7.
    // C-track: each job grows its OWN raw stats (pow_grow differs per job —
    // the FFT 'C' split: switching jobs re-weights now, mastering grows later)
    uint16_t jl = s->job_lv[s->main_job];
    const dungeon_job_def_t *jd = dungeon_job_def((dungeon_job_t)s->main_job);
    uint16_t p = (uint16_t)(3 + s->explore_lv + jl + jl * jd->pow_grow / TUN_GROWTH_DIV
                            + dungeon_item_eff(s->weapon_pow, s->weapon_rar)
                            + s->bless_atk);
    p = (uint16_t)((uint32_t)p * (100 + 3 * s->sanctum[SN_FOR]) / 100);
    p = (uint16_t)((uint32_t)p * (100 + 2 * s->prestige) / 100);
    p = (uint16_t)((uint32_t)p * dungeon_job_def((dungeon_job_t)s->main_job)->m_pow10 / 100);
    p = (uint16_t)((uint32_t)p * (100 + 2 * codex_lv(s)) / 100);
    if (s->energy == 0) p = (uint16_t)(p * 7 / 10);
    return p;
}

static uint16_t guard_of(const dungeon_save_t *s)
{
    uint16_t jl = s->job_lv[s->main_job];
    const dungeon_job_def_t *d = dungeon_job_def((dungeon_job_t)s->main_job);
    uint16_t g = (uint16_t)(dungeon_item_eff(s->armor_def, s->armor_rar)
                      + s->bless_def + s->sanctum[SN_DEF]
                      + jl * d->guard_grow / TUN_GROWTH_DIV);
    return (uint16_t)((uint32_t)g * d->m_grd10 / 100);
}

uint16_t dungeon_guard(const dungeon_save_t *s) { return guard_of(s); }

static uint16_t max_hp_of(const dungeon_save_t *s)
{
    const dungeon_job_def_t *d = dungeon_job_def((dungeon_job_t)s->main_job);
    uint32_t hp = 26 + (uint32_t)s->explore_lv * 4 + (uint32_t)s->job_lv[s->main_job] * d->hp_grow;
    hp = hp * (100 + 5 * s->sanctum[SN_VIT]) / 100;
    hp = hp * (100 + 2 * s->prestige) / 100;
    hp = hp * dungeon_job_def((dungeon_job_t)s->main_job)->m_hp10 / 100;
    hp = hp * (100 + 2 * codex_lv(s)) / 100;
    if (s->wound) hp = hp * 4 / 5;
    return (uint16_t)(hp ? hp : 1);
}

// ---- v12.3 rebirth perks (lever L2): prestige as a stream of unlocks ------
// Industry shape (Loop Hero camp / ADimensions layers): rebirth both shortens
// the boring re-climb (start floor) and grants a named perk per early level.
// Each perk is a player-visible log moment (north-star event) at a fixed
// level, so the late arc keeps its anticipation structure.
static uint8_t start_floor_of(const dungeon_save_t *s)
{
    // v15: starts at prst 2 (was 6) and climbs 2/lv — mid-arc lives get
    // shorter (P25 north-star fix: the worst half-hours were in-life gaps)
    if (s->prestige < 2) return 1;
    uint16_t f = 2u + (s->prestige - 2) * 2;
    return f > 20 ? 20 : (uint8_t)f;
}
static bool perk_inn_free(const dungeon_save_t *s) { return s->prestige >= 1; }

// v13: the stream is ENDLESS. Levels 7+ cycle five mastery lines with hard
// caps, so every rebirth logs a named unlock forever (north-star: the late
// arc must never run out of new-content moments).
static int peddler_disc_pct(const dungeon_save_t *s)     // p2 base, p7+ cycle A
{
    int d = (s->prestige >= 2) ? 25 : 0;
    if (s->prestige >= 7) d += (s->prestige - 6) * 5;
    return d > 50 ? 50 : d;
}
static int forge_disc_pct(const dungeon_save_t *s)       // p4 base, p8+ cycle B
{
    int d = (s->prestige >= 4) ? 10 : 0;
    if (s->prestige >= 8) d += (s->prestige - 7) * 5;
    return d > 30 ? 30 : d;
}
static int desc_heal_pct(const dungeon_save_t *s)        // p5 base, p9+ cycle C
{
    int h = (s->prestige >= 5) ? 40 : (int)TUN_DESC_HEAL;
    if (s->prestige >= 9) h += s->prestige - 8;
    return h > 50 ? 50 : h;
}
static uint8_t shield_cap_of(const dungeon_save_t *s)    // p10+ cycle D
{
    uint8_t c = TUN_ST_GRD_SHIELD_CAP;
    if (s->prestige >= 10) c = (uint8_t)(c + (s->prestige - 9) * 2);
    return c > 24 ? 24 : c;
}
static uint8_t chest_smoke_cap(const dungeon_save_t *s)  // p3 base, p11+ cycle E
{
    uint8_t c = (s->prestige >= 3) ? 4 : 3;
    if (s->prestige >= 11) c = (uint8_t)(c + (s->prestige - 10));
    return c > 8 ? 8 : c;
}
static uint16_t peddler_cost(const dungeon_save_t *s, bool smoke, uint8_t floor)
{
    uint32_t c = (uint32_t)((smoke ? TUN_PRICE_SMOKE : TUN_PRICE_POT) + floor);
    c = c * (uint32_t)(100 - peddler_disc_pct(s)) / 100;
    return (uint16_t)(c ? c : 1);
}
static uint32_t forge_cost(const dungeon_save_t *s, uint32_t stier)
{
    uint32_t c = 10 + (uint32_t)TUN_FORGE_LIN * stier + stier * stier / TUN_FORGE_SQ;
    c = c * (uint32_t)(100 - forge_disc_pct(s)) / 100;
    return c;
}
static const char *rebirth_perk_of(uint8_t prst)   // perk granted AT level prst
{
    static const char *const PERKS[] = {
        NULL, "Inn free", "Peddler -25%", "Chest smoke 4",
        "Forge -10%", "Descend heal 40%", "Start floor +1/lv",
    };
    if (prst <= 6) return PERKS[prst];
    switch ((prst - 7) % 5) {                    // endless mastery cycle
        case 0: return "Mastery: Peddler -5%";
        case 1: return "Mastery: Forge -5%";
        case 2: return "Mastery: Descend +1%";
        case 3: return "Mastery: Shield cap +2";
        default: return "Mastery: Chest smoke +1";
    }
}

// ---- v14 run mutators (lever L9): every life is a fresh combination -------
// After the first badge, each new life rolls one of six PAIRED floor affixes
// (deterministic from run entropy, announced in the log). Reuses the existing
// affix machinery, so balance risk stays inside known bounds. This is the
// Hades-contract answer to the re-climb: the maze you know, remixed.
static const uint8_t MUT_PAIRS[6][2] = {
    {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4},
};
static const char *const MUT_NAMES[6] = {
    "Dark+Wet", "Dark+Thorn", "Dark+Chests", "Wet+Thorn", "Wet+Chests", "Thorn+Chests",
};
static bool has_affix(const dungeon_save_t *s, uint8_t affix)
{
    if (s->floor_affix == affix) return true;
    if (!s->run_mut || s->run_mut > 6) return false;
    const uint8_t *pair = MUT_PAIRS[s->run_mut - 1];
    return pair[0] == affix || pair[1] == affix;
}
static void roll_run_mut(dungeon_save_t *s)
{
    // dormant until explore_lv 5 (~first half hour): onboarding stays
    // vanilla, but the long pre-badge tail gets in-life beats too (v15:
    // the worst empty buckets were 15-24h, just before the first badge)
    if (!s->prestige && !s->ng && s->explore_lv < 5) { s->run_mut = 0; return; }
    s->run_mut = (uint8_t)(1 + (s->kills + s->prestige + s->ng) % 6);
    char buf[DUNGEON_LOG_LEN];
    snprintf(buf, sizeof(buf), "Mutant: %s!", MUT_NAMES[s->run_mut - 1]);
    log_push(s, buf);
}

static void start_battle(dungeon_save_t *s, bool boss);
static void roll_run_mut(dungeon_save_t *s);
static void job_rations(dungeon_save_t *s);

// Equipped pair is two jobs. Leading `want` swaps if it is already the
// sub, otherwise demotes current main into sub (FFT-style). Never writes
// main == sub — the gate-boss shortcut used to leave KNT+KNT.
static void pair_lead(dungeon_save_t *s, dungeon_job_t want)
{
    if (want >= DUNGEON_JOB_COUNT) return;
    if (s->main_job == (uint8_t)want) return;
    if (s->sub_job == (uint8_t)want) {
        uint8_t t = s->main_job;
        s->main_job = s->sub_job;
        s->sub_job = t;
        return;
    }
    s->sub_job = s->main_job;
    s->main_job = (uint8_t)want;
}

static void pair_distinct(dungeon_save_t *s)
{
    if (s->main_job != s->sub_job) return;
    for (uint8_t j = 0; j < DUNGEON_JOB_COUNT; j++) {
        if (j != s->main_job && ((s->unlocked >> j) & 1)) {
            s->sub_job = j;
            return;
        }
    }
}

void start_battle_for_test(dungeon_save_t *s, bool boss) { start_battle(s, boss); }
void run_mut_roll_for_test(dungeon_save_t *s) { roll_run_mut(s); }
void job_rations_for_test(dungeon_save_t *s);
void job_rations_for_test(dungeon_save_t *s) { job_rations(s); }
void job_rations_for_test_bench(dungeon_save_t *s) { job_rations(s); }   // benchmark B section

// v16b job rations (lever L11b): each job preps for a dive differently —
// this is what makes fresh per-job survival curves distinct (B1 identity),
// where behavior passives alone could not move the medians.
static void job_rations(dungeon_save_t *s)
{
    // Top-up to the job's baseline kit (bounded — swap-spam gains nothing):
    // the F4-8 pack attrition (needs/survived ~1.0-1.3) killed fresh first
    // lives; each job's baseline prep shifts that ratio its own way (B1).
    // Mastery (job_lv>=12) deepens thief/dark kits — playbook balance_sim.
    switch (s->main_job) {
        case JOB_KNIGHT: if (s->shield_pool < 3) s->shield_pool = 3; break;
        case JOB_WHITE:  if (s->cons[CONS_POTION] < 1) s->cons[CONS_POTION] = 1; break;
        case JOB_THIEF: {
            uint8_t smoke = (s->job_lv[JOB_THIEF] >= TUN_THIEF_MASTERY_LV)
                ? (uint8_t)TUN_THIEF_RATION_SMOKE : 1;
            if (s->cons[CONS_SMOKE] < smoke) s->cons[CONS_SMOKE] = smoke;
            break;
        }
        case JOB_DARK: {
            uint16_t ess = (s->job_lv[JOB_DARK] >= TUN_DARK_MASTERY_LV)
                ? (uint16_t)TUN_DARK_RATION_ESSENCE : 10;
            if (s->essence < ess) s->essence = ess;
            break;
        }
        default: break;
    }
}

static uint8_t knight_start_shield(const dungeon_save_t *s)
{
    uint8_t sh = 6;
    if (s->job_lv[JOB_KNIGHT] >= TUN_KNIGHT_MASTERY_LV)
        sh = (uint8_t)(sh + TUN_KNIGHT_SHIELD_BONUS);
    return sh;
}

// Job changes re-weight max HP (m_hp10): keep current HP inside the new cap.
static void clamp_hp(dungeon_save_t *s)
{
    uint16_t mh = max_hp_of(s);
    if (s->hp_cur > mh) s->hp_cur = mh;
}

static uint8_t energy_cap_of(const dungeon_save_t *s)
{
    uint16_t m = (uint16_t)(DUNGEON_ENERGY_MAX + 8 * s->sanctum[SN_STAM]);
    return (uint8_t)(m > 180 ? 180 : m);
}

// Start-of-run provisioning from the PACK track (death/recall/new game).
static void grant_pack(dungeon_save_t *s)
{
    uint8_t lv = s->sanctum[SN_PACK];
    uint8_t pot = (uint8_t)(lv / 2), fea = (uint8_t)(lv / 3 + 1);
    if (s->cons[CONS_POTION] < pot) s->cons[CONS_POTION] = pot;
    if (s->cons[CONS_FEATHER] < fea) s->cons[CONS_FEATHER] = fea;
}

void dungeon_new_game(dungeon_save_t *s, uint32_t seed)
{
    memset(s, 0, sizeof(*s));
    s->version = DUNGEON_MODEL_VERSION;
    s->main_job = JOB_KNIGHT;
    s->sub_job = JOB_BLACK;
    s->unlocked = (uint8_t)((1u << JOB_KNIGHT) | (1u << JOB_BLACK));
    s->codex = (uint16_t)((1u << JOB_KNIGHT) | (1u << JOB_BLACK));  // collection: starting jobs
    job_rations(s);   // v24: first dive carries the kit (pack attrition fix)
    job_rations(s);   // v24: the FIRST dive gets the kit too — the F4-8 pack
                      // attrition (needs/survived ~1.0-1.3) kills fresh first
                      // lives, and the B gate measures exactly that life
    s->job_lv[JOB_KNIGHT] = 1;
    s->job_lv[JOB_BLACK] = 1;
    s->explore_lv = 1;
    s->floor = 1;
    s->top_floor = 1;
    s->auto_job = 1;
    s->autopilot = 1;
    s->max_energy = energy_cap_of(s);
    s->energy = s->max_energy;
    s->weapon_pow = 1;
    s->armor_def = 1;
    s->cons[CONS_FEATHER] = 1;
    s->exploring = 0;
    s->hp_cur = max_hp_of(s);
    s->rng_state = seed ? seed : 0x12345678u;
    log_push(s, "Knight+Black ready F1");
}

uint16_t dungeon_max_hp(const dungeon_save_t *s) { return max_hp_of(s); }
uint16_t dungeon_pow(const dungeon_save_t *s) { return pow_of(s); }

// DESIGN damage: max(1, round((pow - def*0.5) * jobmod * rand .9-1.1)) * restraint.
static int damage(uint16_t pow, uint16_t def, uint16_t jobmod10,
                  uint8_t rest10, uint32_t *rng)
{
    int base = (int)pow - (int)(def / 2);
    if (base < 1) base = 1;
    int d = (base * (int)jobmod10 * (int)rest10) / 100;
    d = (d * (90 + (int)(rng_range(rng, 21)))) / 100;
    d = (d * TUN_PLAYER_DMG_NUM) / 100;
    return d < 1 ? 1 : d;
}

// Slayer affix: +30% vs boss kind.
static int post_slayer(const dungeon_save_t *s, uint8_t kind, int d)
{
    if (kind == 3 && affix_count(s, AF_SLAYER)) d = (d * 13) / 10;
    return d;
}

// Player damage application: ARMORED elites soften it; returns actual dealt.
static battle_beat_t *g_beat = NULL;   // scoped capture: _ex sets it for one
                                       // round; dungeon_cast clears it on any
                                       // out-of-round skill path (below), so a
                                       // stale pointer never observes a write.
static void beat_dealt(dungeon_save_t *s, uint8_t i, uint16_t before)
{
    if (!g_beat || !before || i >= 8) return;
    uint32_t add = (uint32_t)before - s->enemy_hp[i];
    uint32_t tot = (uint32_t)g_beat->dealt + add;
    g_beat->dealt = (uint16_t)(tot > 60000 ? 60000 : tot);
    if (!s->enemy_hp[i]) g_beat->foes |= (uint8_t)(1u << i);
}
static void beat_taken(dungeon_save_t *s, uint16_t before)
{
    if (!g_beat) return;
    uint32_t add = (uint32_t)before - s->hp_cur;
    if (!add) return;
    uint32_t tot = (uint32_t)g_beat->taken + add;
    g_beat->taken = (uint16_t)(tot > 60000 ? 60000 : tot);
    g_beat->flags |= BBEAT_HURT;
}
static int deal_to_enemy(dungeon_save_t *s, uint8_t i, int d)
{
    if (s->enemy_affix[i] == EA_ARMORED) d = (d * 2) / 3;
    uint16_t was = s->enemy_hp[i];
    s->enemy_hp[i] = (s->enemy_hp[i] > d) ? (uint16_t)(s->enemy_hp[i] - d) : 0;
    beat_dealt(s, i, was);
    return d;
}

static void gain_explore(dungeon_save_t *s, uint16_t amount)
{
    char buf[DUNGEON_LOG_LEN];
    s->explore_exp = (uint16_t)(s->explore_exp + amount);
    uint16_t need = (uint16_t)(s->explore_lv * TUN_EXPLORE_NEED);
    while (s->explore_exp >= need && s->explore_lv < 99) {
        s->explore_exp = (uint16_t)(s->explore_exp - need);
        s->explore_lv++;
        need = (uint16_t)(s->explore_lv * TUN_EXPLORE_NEED);
        snprintf(buf, sizeof(buf), "Explore LV%u", s->explore_lv);
        log_push(s, buf);
    }
}

static void gain_job(dungeon_save_t *s, uint16_t amount)
{
    char buf[DUNGEON_LOG_LEN];
    uint8_t j = s->main_job;
    if (s->job_lv[j] >= DUNGEON_JOB_LV_CAP) return;
    if (s->bless_learn) amount = (uint16_t)(amount + 1);
    s->job_exp[j] = (uint16_t)(s->job_exp[j] + amount);
    s->jobpt = s->job_exp[j];
    uint16_t need = (uint16_t)(s->job_lv[j] * 6);
    while (s->job_exp[j] >= need && s->job_lv[j] < DUNGEON_JOB_LV_CAP) {
        s->job_exp[j] = (uint16_t)(s->job_exp[j] - need);
        s->job_lv[j]++;
        s->jobpt = s->job_exp[j];
        need = (uint16_t)(s->job_lv[j] * 6);
        snprintf(buf, sizeof(buf), "%s LV%u%s", JOB_NAMES[j], s->job_lv[j],
                 (s->job_lv[j] == 2 || s->job_lv[j] == 5) ? " SK+!" : "");
        log_push(s, buf);
    }
}

// ---- enemies ---------------------------------------------------------------
static uint8_t theme_kind(const dungeon_save_t *s)
{
    uint16_t f = s->floor;
    if (f <= 20) return 0;   // plains: beasts
    if (f <= 40) return 2;   // caves: undead (dark/white turf)
    if (f <= 60) return 1;   // fire cave: fliers (black turf)
    if (f <= 80) return 0;   // frost: beasts
    return (uint8_t)(f % 3); // vault: mixed
}

static uint16_t enemy_hp(const dungeon_save_t *s, bool boss)
{
    // Quadratic depth curve vs linear player growth: the wall moves out
    // with meta progression. Badges scale LINEARLY (x1+b, not x2^b): an
    // exponential badge mult outruns the capped player plateau and turns
    // late mazes into 200-round marathons (caught by the sim stall gate).
    uint32_t f = s->floor;
    uint32_t hp = (6 + 4 * f) * (10 + f / 3) / 10;
    if (boss) hp *= 3;
    // Badge mult softens past +6: deep mazes must die by attrition (atk
    // keeps climbing), never re-enter marathon-battle territory (STALL).
    uint32_t m = (s->ng <= 6) ? (1 + s->ng) : (7 + (s->ng - 6) / 2);
    if (m > 20) m = 20;
    hp *= m;
    if (hp > 60000) hp = 60000;
    return (uint16_t)hp;
}

static uint16_t enemy_atk(const dungeon_save_t *s, bool boss)
{
    // Base 4 (was 2): with the Blob curve the FIRST floors must already cost
    // ~5-10% HP per battle — invisible early monsters were the complaint.
    uint32_t a = 4 + (uint32_t)s->floor * TUN_ATK_NUM / 5 + TUN_ATK_BADGE * s->ng;
    if (boss) a += s->floor / 4;
    if (has_affix(s, 1)) a += a * 15 / 100;   // dark: they hit harder, pay more
    return (uint16_t)(a > 250 ? 250 : a);
}

static void drop_gear(dungeon_save_t *s, bool boss, bool elite);

static void start_battle(dungeon_save_t *s, bool boss)
{
    char buf[DUNGEON_LOG_LEN];
    pair_distinct(s);
    s->round_ct = 0;
    s->ult_used = 0;
    s->rare_room = 0;
    s->enemy_slow = has_affix(s, 2) ? 2 : 0;   // wet: sluggish opener
    s->must_bright = boss ? 1 : 0;
    if (boss) {
        // Auto leads with the gate expert; manual switch still wins (no CD set).
        if (s->auto_job && s->main_job != JOB_KNIGHT && ((s->unlocked >> JOB_KNIGHT) & 1)) {
            pair_lead(s, JOB_KNIGHT);
            clamp_hp(s);
            log_push(s, "auto: Knight to the gate");
        }
        s->n_enemy = 1;
        s->enemy_hp[0] = enemy_hp(s, true);
        s->enemy_kind[0] = 3;
        s->enemy_affix[0] = EA_NONE;
        s->codex |= (1u << 8);   // collection: gatekeeper entry
        codex_maze_first(s, 0);  // v13: maze-N first gate boss
        s->shield_pool = 0;
        if (s->main_job == JOB_KNIGHT) s->shield_pool = knight_start_shield(s);
        if (dungeon_item_has_affix(s->armor_affix, AF_GUARD))
            s->shield_pool = (uint8_t)(s->shield_pool + 4 + 2 * s->armor_rar);
        if (dungeon_item_has_affix(s->weapon_affix, AF_GUARD))
            s->shield_pool = (uint8_t)(s->shield_pool + 4 + 2 * s->weapon_rar);
        snprintf(buf, sizeof(buf), "GATE F%u HP%u!", s->floor, s->enemy_hp[0]);
        log_push(s, buf);
        return;
    }
    uint8_t n = 1;
    if (s->floor >= 10) n = 2;
    if (s->floor >= 25 && rng_range(&s->rng_state, 100) < 40) n = 3;
    if (n > DUNGEON_MAX_ENEMIES) n = DUNGEON_MAX_ENEMIES;
    s->n_enemy = n;
    for (uint8_t i = 0; i < n; i++) {
        s->enemy_hp[i] = enemy_hp(s, false);
        s->enemy_kind[i] = theme_kind(s);
        s->enemy_affix[i] = EA_NONE;
        s->codex |= (uint16_t)(1u << (5 + (s->enemy_kind[i] & 3)));  // collection: kin entry
    }
    // RARE foe (v5.1): ~1%% of deep rooms gleam — every foe bears a modifier,
    // loot is rich (shards/essence burst + extra gear roll), codex-worthy.
    if (s->floor >= 15 && (int)rng_range(&s->rng_state, 10000) < TUN_RARE_PPM) {
        for (uint8_t i = 0; i < n; i++)
            s->enemy_affix[i] = (uint8_t)(1 + rng_range(&s->rng_state, EA_COUNT - 1));
        s->enemy_affix[0] = EA_GIANT;
        if (s->enemy_hp[0] < 60000)
            s->enemy_hp[0] = (uint16_t)(s->enemy_hp[0] + s->enemy_hp[0] / 2);
        s->rare_room = 1;
        s->codex |= (1u << 10);
        codex_maze_first(s, 2);  // v13: maze-N first rare
        log_push(s, "A rare foe gleams!");
    }
    // Elite spawn: one modified foe on deep floors (v4.4). Frequency is a
    // treadmill knob: every elite room pays double XP + essence + a card.
    // (Rare rooms already bear full modifiers and skip this roll.)
    uint32_t echance = s->rare_room ? 0
                     : (s->floor >= 40) ? TUN_ELITE_HI : (s->floor >= 15) ? TUN_ELITE_LO : 0;
    if (rng_range(&s->rng_state, 100) < echance) {
        uint8_t ei = (uint8_t)rng_range(&s->rng_state, n);
        dungeon_elite_t ea = (dungeon_elite_t)(1 + rng_range(&s->rng_state, EA_COUNT - 1));
        s->enemy_affix[ei] = (uint8_t)ea;
        if (ea == EA_GIANT)
            s->enemy_hp[ei] = (uint16_t)(s->enemy_hp[ei] + s->enemy_hp[ei] / 2);
        static const char *const EA_NAMES[EA_COUNT] = {
            "-", "armored", "venomous", "swift", "thorned", "giant",
        };
        snprintf(buf, sizeof(buf), "Elite %s foe!", EA_NAMES[ea]);
        log_push(s, buf);
    }
    s->shield_pool = 0;
    if (s->main_job == JOB_KNIGHT) s->shield_pool = knight_start_shield(s);
    if (dungeon_item_has_affix(s->armor_affix, AF_GUARD))
        s->shield_pool = (uint8_t)(s->shield_pool + 4 + 2 * s->armor_rar);
    if (dungeon_item_has_affix(s->weapon_affix, AF_GUARD))
        s->shield_pool = (uint8_t)(s->shield_pool + 4 + 2 * s->weapon_rar);
    snprintf(buf, sizeof(buf), "%u foe%s ahead!", n, n > 1 ? "s" : "");
    log_push(s, buf);
}

static int alive_count(const dungeon_save_t *s)
{
    int n = 0;
    for (uint8_t i = 0; i < s->n_enemy; i++)
        if (s->enemy_hp[i]) n++;
    return n;
}

static int pick_target(const dungeon_save_t *s)
{
    int fallback = -1;
    int extreme = -1;
    uint16_t best = 0;
    for (uint8_t i = 0; i < s->n_enemy; i++) {
        if (!s->enemy_hp[i]) continue;
        if (fallback < 0) fallback = i;
        bool better = (s->enemy_hp[i] < best);
        if (extreme < 0 || better) { best = s->enemy_hp[i]; extreme = i; }
    }
    if (fallback < 0) return -1;
    switch (dungeon_job_def((dungeon_job_t)s->main_job)->ai) {
    case AI_SNIPER:    // black: weakest
    case AI_BACKSTAB:  // thief: weakest
    case AI_MEDIC:     // white: weakest too, weak poke
        return extreme;
    case AI_EXECUTE: { // dark: fattest target
        int hi = fallback;
        uint16_t hv = 0;
        for (uint8_t i = 0; i < s->n_enemy; i++)
            if (s->enemy_hp[i] > hv) { hv = s->enemy_hp[i]; hi = i; }
        return hi;
    }
    case AI_CHARGE:    // knight: nearest = first alive
    default:
        return fallback;
    }
}

// ---- autonomous job swap ---------------------------------------------------
// Only between battles (n_enemy==0), gated by the same 2s switch CD as the
// player, so a manual "steal the wheel" always holds for at least one CD.
static void auto_job_tick(dungeon_save_t *s, uint64_t now_ms)
{
    char buf[DUNGEON_LOG_LEN];
    if (!s->auto_job || s->n_enemy || s->choice_pending) return;
    if (now_ms < s->switch_ready_ms) return;
    dungeon_job_t want = (dungeon_job_t)s->main_job;
    uint16_t mh = max_hp_of(s);
    if (s->hp_cur * 10 < mh * 4 && ((s->unlocked >> JOB_WHITE) & 1))
        want = JOB_WHITE;                                   // 1. dying -> white
    else if (s->floor >= 21 && s->floor <= 40 && ((s->unlocked >> JOB_DARK) & 1))
        want = JOB_DARK;                                    // 2. undead caves -> dark
    else if (s->floor >= 41 && s->floor <= 60 && ((s->unlocked >> JOB_BLACK) & 1))
        want = JOB_BLACK;                                   // 3. flier belts -> black
    else if (s->floor >= 81 && ((s->unlocked >> JOB_DARK) & 1))
        want = JOB_DARK;                                    // 4. vault: big pools -> execute
    else if (s->floor >= 16 && (s->floor % 10) >= 6 && (s->floor % 10) <= 8
             && ((s->unlocked >> JOB_THIEF) & 1) && s->hp_cur * 10 > mh * 6)
        want = JOB_THIEF;                                   // 5. rich stretch -> thief
    else if (s->floor >= 61 && s->floor <= 80 && ((s->unlocked >> JOB_KNIGHT) & 1))
        want = JOB_KNIGHT;                                  // 6. beast tundra -> tank
    if (want != s->main_job) {
        pair_lead(s, want);
        clamp_hp(s);
        s->switch_ready_ms = now_ms + DUNGEON_SWITCH_CD_MS;
        snprintf(buf, sizeof(buf), "auto: %s leads, %s supports",
                 JOB_NAMES[want], JOB_NAMES[s->sub_job]);
        log_push(s, buf);
    }
}

// ---- skills ----------------------------------------------------------------
// Leaving a room alive (smoke consumable / Smoke Out): every room flag must
// reset here, otherwise the NEXT clear pays this room's bounty (mimic x2,
// gate essence, even "MAZE CLEAR" at F100 — the review's Smoke Out exploit).
static void flee_room(dungeon_save_t *s)
{
    s->n_enemy = 0;
    s->mimic_room = 0;
    s->rare_room = 0;
    s->hidden_room = 0;
    s->challenge_room = 0;
    s->must_bright = 0;
    s->ult_used = 0;
    s->shield_pool = 0;
    s->enemy_slow = s->enemy_weak = s->enemy_dot = 0;
}

// Apply a skill effect. Returns damage dealt (0 utility).
static int skill_fire(dungeon_save_t *s, dungeon_skill_t sk)
{
    if (g_beat) g_beat->flags |= BBEAT_SKILL;   // in-round offensive skill
    char buf[DUNGEON_LOG_LEN];
    int t = pick_target(s);
    uint16_t pow = pow_of(s);
    uint8_t rest = 10;
    if (t >= 0) rest = RESTRAIN[s->main_job][s->enemy_kind[t] & 3];
    switch (sk) {
    case SK_SHIELD_BASH: {
        if (t < 0) return 0;
        int d = post_slayer(s, s->enemy_kind[t],
                            damage(pow, 0, 12, rest, &s->rng_state));
        d = deal_to_enemy(s, (uint8_t)t, d);
        s->enemy_weak = 2;  // dazed: -50% 2 rounds
        snprintf(buf, sizeof(buf), "Bash -%d dazed!", d);
        log_push(s, buf);
        return d;
    }
    case SK_TAUNT:
        // Solo adaptation: with one body, taunting isn't aggro control — it is
        // a BRACE: stagger the room (V2 retired the charge-snuff branch: the
        // charge telegraph was dead code, heavies are rolled actions now).
        s->enemy_weak = 3;
        log_push(s, "Taunt! foe -50%");
        return 0;
    case SK_FIRE: {
        int total = 0;
        // v31 identity: crowd nuke — +25% per extra foe (black clears packs,
        // the mid-game texture that separates it from the knight's 1v1 wall)
        uint8_t foes = 0;
        for (uint8_t i = 0; i < s->n_enemy; i++) if (s->enemy_hp[i]) foes++;
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i]) continue;
            uint8_t r = RESTRAIN[s->main_job][s->enemy_kind[i] & 3];
            int d = post_slayer(s, s->enemy_kind[i],
                                damage(pow, 0, 13, r, &s->rng_state));
            if (foes >= 2) d = d * (10 + 5 * (foes - 1)) / 10;
            total += deal_to_enemy(s, i, d);
        }
        snprintf(buf, sizeof(buf), "Fire row -%d!", total);
        log_push(s, buf);
        return total;
    }
    case SK_FROST: {
        if (t < 0) return 0;
        int d = post_slayer(s, s->enemy_kind[t],
                            damage(pow, 0, 9, rest, &s->rng_state));
        d = deal_to_enemy(s, (uint8_t)t, d);
        s->enemy_slow = 3;
        snprintf(buf, sizeof(buf), "Frost -%d slowed!", d);
        log_push(s, buf);
        return d;
    }
    case SK_CURE: {
        uint16_t mh = max_hp_of(s);
        uint16_t heal = (uint16_t)(6 + s->job_lv[s->main_job] * 2);
        s->hp_cur = (uint16_t)(s->hp_cur + heal > mh ? mh : s->hp_cur + heal);
        log_push(s, "Cure!");
        return 0;
    }
    case SK_AEGIS:
        s->shield_pool = 10;
        log_push(s, "Aegis up!");
        return 0;
    case SK_VENOM: {
        if (t < 0) return 0;
        int d = post_slayer(s, s->enemy_kind[t],
                            damage(pow, 0, 12, rest, &s->rng_state));
        d = deal_to_enemy(s, (uint8_t)t, d);
        s->enemy_dot = 3;
        snprintf(buf, sizeof(buf), "Venom -%d poison!", d);
        log_push(s, buf);
        return d;
    }
    case SK_SMOKEOUT:
        flee_room(s);
        log_push(s, "Smoke! slipped away");
        return 0;
    case SK_DRAIN: {
        if (t < 0) return 0;
        int d = post_slayer(s, s->enemy_kind[t],
                            damage(pow, 0, 11, rest, &s->rng_state));
        d = deal_to_enemy(s, (uint8_t)t, d);
        uint16_t mh = max_hp_of(s);
        uint16_t heal = (uint16_t)(d / 2);
        s->hp_cur = (uint16_t)(s->hp_cur + heal > mh ? mh : s->hp_cur + heal);
        snprintf(buf, sizeof(buf), "Drain -%d +%u", d, heal);
        log_push(s, buf);
        return d;
    }
    case SK_DOOM: {
        if (t < 0) return 0;
        int d = post_slayer(s, s->enemy_kind[t],
                            damage(pow, 0, 7, rest, &s->rng_state));
        d = deal_to_enemy(s, (uint8_t)t, d);
        s->enemy_dot = 4;
        snprintf(buf, sizeof(buf), "Doom -%d cursed!", d);
        log_push(s, buf);
        return d;
    }
    default:
        return 0;
    }
}

// Evolved ultimate (v5.1): VS-style "max job + matching affix = a third skill",
// auto-cast once per battle. Big, simple, expressive.
static int ult_fire(dungeon_save_t *s)
{
    s->codex |= (1u << 12);   // collection entry: first evolution fired
    uint16_t pow = pow_of(s);
    switch (dungeon_job_def((dungeon_job_t)s->main_job)->ultimate) {
    case SK_CASTLE:  // knight+guard: walls rise
        s->shield_pool = (uint8_t)(s->shield_pool + 15);
        log_push(s, "CASTLE! walls rise");
        return 0;
    case SK_METEOR: {  // black+slayer: heavy row burn
        int total = 0;
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i]) continue;
            uint8_t r = RESTRAIN[s->main_job][s->enemy_kind[i] & 3];
            total += deal_to_enemy(s, i, post_slayer(s, s->enemy_kind[i],
                                                    damage(pow, 0, 16, r, &s->rng_state)));
        }
        log_push(s, "METEOR! the row burns");
        return total;
    }
    case SK_SANCTUARY:  // white+leech: full restore
        s->hp_cur = max_hp_of(s);
        s->shield_pool = (uint8_t)(s->shield_pool + 5);
        s->enemy_dot = 0;
        log_push(s, "SANCTUARY! reborn");
        return 0;
    case SK_JACKPOT: {  // thief+greed: execute the weakest non-boss, coin burst
        int low = -1;
        uint16_t lv = 0xFFFF;
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i] || s->enemy_kind[i] == 3) continue;
            if (s->enemy_hp[i] < lv) { lv = s->enemy_hp[i]; low = i; }
        }
        if (low >= 0) s->enemy_hp[low] = 0;
        s->gold = sat_add(s->gold, 30);
        log_push(s, "JACKPOT! +30g");
        return 0;
    }
    case SK_ECLIPSE: {  // dark+venom: curse the room
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i]) continue;
            uint8_t r = RESTRAIN[s->main_job][s->enemy_kind[i] & 3];
            deal_to_enemy(s, i, damage(pow, 0, 9, r, &s->rng_state));
        }
        s->enemy_dot = 5;
        log_push(s, "ECLIPSE! cursed");
        return 0;
    }
    default:
        return 0;
    }
}

// Evolution check: job mastered to cap + its affix carried + not yet spent.
uint8_t dungeon_ult_ready(const dungeon_save_t *s)
{
    if (s->n_enemy == 0 || s->ult_used) return 0;
    const dungeon_job_def_t *d = dungeon_job_def((dungeon_job_t)s->main_job);
    if (s->job_lv[s->main_job] < TUN_ULT_LV) return 0;
    return affix_count(s, (dungeon_affix_t)d->ult_affix) ? 1 : 0;
}

static dungeon_event_t clear_battle(dungeon_save_t *s, uint64_t now_ms);

int dungeon_cast(dungeon_save_t *s, uint8_t slot, uint64_t now_ms)
{
    (void)now_ms;
    g_beat = NULL;   // out-of-round skill path: never capture into a stale beat
                     // (manual casts + pre-round autopilot casts are presented
                     // immediately by the UI, not via beats)
    if (s->n_enemy == 0) return 0;  // skills need a fight
    dungeon_job_t job = (dungeon_job_t)s->main_job;
    dungeon_skill_t sk = SK_NONE;
    uint8_t cdslot = slot;
    if (slot <= 1) {
        if (!dungeon_skill_unlocked(s, job, slot)) return 0;
        if (s->skill_cd[slot]) return 0;
        sk = dungeon_job_def(job)->active[slot];
    } else if (slot == 2) {
        dungeon_job_t sub = (dungeon_job_t)s->sub_job;
        if (!dungeon_skill_unlocked(s, sub, 0)) return 0;
        if (s->skill_cd[2]) return 0;
        sk = dungeon_job_def(sub)->active[0];
        cdslot = 2;
    } else {
        return 0;
    }
    if (s->energy < 2) return 0;
    s->energy = (uint8_t)(s->energy - 2);
    s->skill_cd[cdslot] = (sk == SK_FIRE || sk == SK_CURE || sk == SK_DRAIN) ? 3 : 4;
    if (sk == SK_CURE) {
        uint16_t mh = max_hp_of(s);
        uint8_t lvj = (slot == 2) ? s->sub_job : s->main_job;
        uint16_t heal = (uint16_t)(6 + s->job_lv[lvj] * 2);
        s->hp_cur = (uint16_t)(s->hp_cur + heal > mh ? mh : s->hp_cur + heal);
        log_push(s, "Cure!");
        return 0;
    }
    int dealt = skill_fire(s, sk);
    // Manual killing blow resolves loot/choice. A flee (n_enemy already 0)
    // must NOT count as a clear.
    if (s->n_enemy && !alive_count(s)) clear_battle(s, now_ms);
    return dealt;
}

bool dungeon_switch(dungeon_save_t *s, uint8_t which, uint64_t now_ms)
{
    if (now_ms < s->switch_ready_ms) return false;
    if (which == 0) return true;  // already main
    if (which == 1) {
        uint8_t t = s->main_job;
        s->main_job = s->sub_job;
        s->sub_job = t;
        clamp_hp(s);
        s->switch_ready_ms = now_ms + DUNGEON_SWITCH_CD_MS;
        char buf[DUNGEON_LOG_LEN];
        snprintf(buf, sizeof(buf), "Switch! %s leads", JOB_NAMES[s->main_job]);
        log_push(s, buf);
        return true;
    }
    return false;
}

// ---- death / meta ----------------------------------------------------------
static void death_apply(dungeon_save_t *s)
{
    char buf[DUNGEON_LOG_LEN];
    s->deaths = sat_inc(s->deaths);
    uint8_t dfloor = s->floor;   // captured before reset (used by gradient + log)
    // M1 extraction gradient: deeper deaths keep a % of gold (the risk was
    // real, the loss shouldn't be total). F1-20 = full wipe (normal cadence),
    // F21-50 = 20%, F51+ = 35% (Loop Hero's praised 100/60/30 tier design).
    {
        if (dfloor > 50) s->gold = s->gold * 35 / 100;
        else if (dfloor > 20) s->gold = s->gold * 20 / 100;
        else s->gold = 0;
    }
    s->cons[0] = s->cons[1] = s->cons[2] = 0;
    {   // M2 session closure: per-life report gives micro-session completeness
        char rbuf[DUNGEON_LOG_LEN];
        snprintf(rbuf, sizeof(rbuf), "Run: F%u, %u kills, %ug",
                 (unsigned)dfloor, (unsigned)s->kills, (unsigned)s->gold);
        log_push(s, rbuf);
    }
    s->cons[0] = s->cons[1] = s->cons[2] = 0;
    s->explore_lv = s->explore_lv > 1 ? (s->explore_lv + 1) / 2 : 1;  // soft reset (v5)
    s->mimic_room = 0;
    s->rare_room = 0;
    s->hidden_room = 0;
    s->hidden_next = 0;
    s->challenge_room = 0;
    s->challenge_next = 0;
    s->starfall_next = 0;
    s->ult_used = 0;
    s->floor = start_floor_of(s);    // job_lv/essence/gear/sanctum kept
    roll_run_mut(s);                 // v14: every life is a fresh combination
    job_rations(s);                  // v16b: per-job dive prep
    s->floor_affix = 0;
    s->n_enemy = 0;
    s->bless_atk = s->bless_def = s->bless_learn = 0;
    s->wound = 0;
    s->max_energy = energy_cap_of(s);
    s->energy = s->max_energy;
    s->hp_cur = max_hp_of(s);
    grant_pack(s);
    snprintf(buf, sizeof(buf), "Down! kept Lv/essence (%u)", (unsigned)s->deaths);
    log_push(s, buf);
}

static void drop_gear(dungeon_save_t *s, bool boss, bool elite)
{
    char buf[DUNGEON_LOG_LEN];
    uint32_t chance = boss ? 100 : (elite ? 60 : 12);
    if (rng_range(&s->rng_state, 100) >= chance) return;
    uint8_t tier = (uint8_t)(1 + s->floor / 3 + rng_range(&s->rng_state, s->floor / 6 + 1));
    if (tier > DUNGEON_GEAR_TIER_CAP) tier = (uint8_t)DUNGEON_GEAR_TIER_CAP;
    uint8_t rar = roll_rar(s);
    bool wpn = rng_range(&s->rng_state, 2);
    uint8_t cur_t = wpn ? s->weapon_pow : s->armor_def;
    uint8_t cur_r = wpn ? s->weapon_rar : s->armor_rar;
    uint32_t score = (uint32_t)tier * (10 + 5 * rar);
    uint32_t cur = (uint32_t)cur_t * (10 + 5 * cur_r);
    if (score <= cur) return;  // keep the better one
    uint8_t *pt = wpn ? &s->weapon_pow : &s->armor_def;
    uint8_t *pr = wpn ? &s->weapon_rar : &s->armor_rar;
    uint8_t *pa = wpn ? &s->weapon_affix : &s->armor_affix;
    *pt = tier;
    *pr = rar;
    *pa = affix_pack(&s->rng_state, rar, *pa);
    snprintf(buf, sizeof(buf), "Loot: %s T%u R%u!", wpn ? "wpn" : "arm", tier, rar);
    log_push(s, buf);
}

static void loot_room(dungeon_save_t *s, bool boss, bool elite)
{
    char buf[DUNGEON_LOG_LEN];
    s->kills = sat_inc(s->kills);
    uint16_t gold = (uint16_t)(3 + s->floor + rng_range(&s->rng_state, 5));
    if (boss) gold = (uint16_t)(gold + 10 + s->floor);
    if (s->main_job == JOB_THIEF) gold = (uint16_t)(gold + gold / 4);  // loot sense
    uint8_t greed = affix_count(s, AF_GREED);
    if (greed) gold = (uint16_t)(gold + (uint32_t)gold * 30 * greed / 100);
    if (has_affix(s, 1)) gold = (uint16_t)(gold + gold / 4);   // dark: richer
    if (s->rare_room) {
        s->shards = (uint8_t)(s->shards + 5 > 99 ? 99 : s->shards + 5);
        s->essence = sat_add(s->essence, 5);
        drop_gear(s, true, true);   // guaranteed extra roll
        s->rare_room = 0;
        log_push(s, "Rare bounty!");
    }
    if (s->challenge_room) {
        s->shards = (uint8_t)(s->shards + 8 > 99 ? 99 : s->shards + 8);
        drop_gear(s, true, true);
        s->challenge_room = 0;
        log_push(s, "Challenge cleared!");
    }
    uint8_t mmult = 1;
    if (s->mimic_room) {
        mmult = (s->main_job == JOB_THIEF) ? 3 : 2;  // thief loot sense pays off
        gold = (uint16_t)(gold * mmult);
        s->mimic_room = 0;
    }
    s->gold = sat_add(s->gold, gold);
    gain_explore(s, (uint16_t)((4 + s->floor / 3) * (elite || boss ? 2 : 1) * mmult));
    gain_job(s, (uint16_t)(elite || boss ? 4 : 2));
    // no essence here: elites already pay double XP + shards + a card;
    // bosses pay at the gate (+3 in clear_battle). v4.4: this line was the
    // mid-game snowball (run-10 depth 54 -> 88) and is the removal knob.
    drop_gear(s, boss, elite);
    if (boss) s->shards = (uint8_t)(s->shards + TUN_BOSS_SHARDS > 99 ? 99 : s->shards + TUN_BOSS_SHARDS);
    else if (elite && rng_range(&s->rng_state, 100) < 25)
        s->shards = (uint8_t)(s->shards + 1 > 99 ? 99 : s->shards + 1);
    snprintf(buf, sizeof(buf), "Clear! +%ug", gold);
    log_push(s, buf);
}

// ---- choice cards ----------------------------------------------------------
void dungeon_choice_offer(dungeon_save_t *s, uint64_t now_ms)
{
    // L12: offers were arithmetic (k, k+2, k+4), which structurally paired
    // every low-value card with a preferred one - dead cards could never be
    // picked regardless of value. True 3-of-9 distinct random offers.
    uint8_t used[CH_COUNT] = {0};
    for (uint8_t i = 0; i < DUNGEON_CHOICE_OPTS; i++) {
        uint8_t c;
        do { c = (uint8_t)rng_range(&s->rng_state, CH_COUNT); } while (used[c]);
        used[c] = 1;
        s->choice_opts[i] = c;
    }
    s->choice_pending = 1;
    s->choice_cursor = 0;
    s->choice_deadline_ms = now_ms + DUNGEON_CHOICE_TIMEOUT_MS;
}

static void choice_apply(dungeon_save_t *s, uint8_t idx)
{
    char buf[DUNGEON_LOG_LEN];
    if (idx >= DUNGEON_CHOICE_OPTS) return;
    switch ((dungeon_choice_t)s->choice_opts[idx]) {
    case CH_AFFIX:
        s->floor_affix = (uint8_t)(1 + rng_range(&s->rng_state, 4));
        s->gold = sat_add(s->gold, 15);   // L12: omen sweetener — dead card revive
        snprintf(buf, sizeof(buf), "Omen: %s, +15g",
                 s->floor_affix == 1 ? "dark" : s->floor_affix == 2 ? "wet"
                 : s->floor_affix == 3 ? "thorn" : "chests+");
        log_push(s, buf);
        break;
    case CH_BLESS_ATK:
        if (s->bless_atk <= 8) s->bless_atk += 2;  // capped: 100-floor runs
        log_push(s, "Bless: ATK+2");               // would stack unbounded
        break;
    case CH_BLESS_LEARN: s->bless_learn = 1; log_push(s, "Bless: LEARN+"); break;
    case CH_BLESS_DEF:
        if (s->bless_def <= 8) s->bless_def += 2;
        log_push(s, "Bless: DEF+2");
        break;
    case CH_TRADE_WOUND:
        s->wound = 1;
        if (s->hp_cur > max_hp_of(s)) s->hp_cur = max_hp_of(s);  // max shrinks now
        s->gold = sat_add(s->gold, 35);   // L12: the wound trade must pay
        log_push(s, "Trade: wound, +chest");
        break;
    case CH_TRADE_FEATHER:
        if (s->cons[CONS_FEATHER] > 0) {
            s->cons[CONS_FEATHER]--;
            s->gold = sat_add(s->gold, 50);   // L12: feather is scarce, pay it
            log_push(s, "Trade: feather, rich!");
        } else {
            s->bless_atk += 2;
            log_push(s, "No feather: ATK+2");
        }
        break;
    case CH_SHRINE:
        s->essence = sat_add(s->essence, TUN_GATE_ESSENCE);
        {   // L12: shrine restores — a blessing, not a trickle
            uint16_t mh2 = max_hp_of(s);
            s->hp_cur = s->hp_cur + mh2 * 3 / 10 > mh2 ? mh2 : (uint16_t)(s->hp_cur + mh2 * 3 / 10);
        }
        log_push(s, "Shrine: +3 essence, restored");
        break;
    case CH_SHARDS:
        s->shards = (uint8_t)(s->shards + 2 > 99 ? 99 : s->shards + 2);
        log_push(s, "Cache: +2 shards");
        break;
    case CH_CHALLENGE:
        s->challenge_next = 1;
        s->codex |= (1u << 15);
        log_push(s, "The cursed door creaks open...");
        break;
    default: break;
    }
    s->choice_pending = 0;
}

void dungeon_choice_pick(dungeon_save_t *s, uint8_t idx)
{
    if (!s->choice_pending) return;
    choice_apply(s, idx);
}

void dungeon_choice_timeout(dungeon_save_t *s, uint64_t now_ms)
{
    (void)now_ms;
    if (!s->choice_pending) return;
    choice_apply(s, (uint8_t)rng_range(&s->rng_state, DUNGEON_CHOICE_OPTS));
}

// ---- town ------------------------------------------------------------------
// Geometric repeated-purchase cost r = 3/R2 (Pecorella: idle costs are
// geometric, r in 1.07..1.5; default r = 1.5). BASE=4: 4,6,9,13,19,28,...
uint8_t dungeon_sanctum_cost(uint8_t lv)
{
    uint16_t c = TUN_SANCTUM_BASE;
    for (uint8_t i = 0; i < lv && c < 1000; i++)
        c = (uint16_t)(c * 3 / TUN_SANCTUM_R2);
    return (uint8_t)(c > 255 ? 255 : c);
}

bool dungeon_sanctum_buy(dungeon_save_t *s, dungeon_sanctum_t tr)
{
    if (tr >= SN_COUNT || s->sanctum[tr] >= 10) return false;
    uint8_t cost = dungeon_sanctum_cost(s->sanctum[tr]);
    if (s->essence < cost) return false;
    s->essence = (uint16_t)(s->essence - cost);
    s->sanctum[tr]++;
    if (tr == SN_STAM) {
        s->max_energy = energy_cap_of(s);
        if (s->energy > s->max_energy) s->energy = s->max_energy;
    }
    if (tr == SN_PACK) grant_pack(s);
    return true;
}

bool dungeon_town(dungeon_save_t *s, dungeon_town_t act, uint8_t param)
{
    char buf[DUNGEON_LOG_LEN];
    if (act == TOWN_SWAP) {
        uint8_t a = (uint8_t)(param >> 4), b = (uint8_t)(param & 0xF);
        if (a >= DUNGEON_JOB_COUNT || b >= DUNGEON_JOB_COUNT || a == b) return false;
        if (!((s->unlocked >> a) & 1) || !((s->unlocked >> b) & 1)) return false;
        s->main_job = a;
        s->sub_job = b;
        job_rations(s);   // v24: the new lead preps (top-up, bounded)
        clamp_hp(s);
        snprintf(buf, sizeof(buf), "Pair: %s+%s", JOB_NAMES[a], JOB_NAMES[b]);
        log_push(s, buf);
        return true;
    }
    if (act == TOWN_AUTOTOG) {
        s->auto_job = s->auto_job ? 0 : 1;
        snprintf(buf, sizeof(buf), "Auto-job %s", s->auto_job ? "ON" : "OFF");
        log_push(s, buf);
        return true;
    }
    if (act == TOWN_SANCTUM) return dungeon_sanctum_buy(s, (dungeon_sanctum_t)param);
    if (act == TOWN_SYNTH) {
        bool wpn = (param == 0);
        uint8_t rar = wpn ? s->weapon_rar : s->armor_rar;
        if (rar >= 3) return false;  // ancient relics (rar4) only come from deep drops
        uint16_t sc = (uint16_t)(TUN_SHARD_COST * (rar + 1));  // shard cost gates synthesis pace:
        // R1=5 (~run 3), R2=15 total (~run 10), R3=30 total (~run 25+)
        uint16_t gc = (uint16_t)(40 * (rar + 1));
        if (s->shards < sc || s->gold < gc) return false;
        s->shards = (uint8_t)(s->shards - sc);
        s->gold = (uint16_t)(s->gold - gc);
        rar++;
        uint8_t *pr = wpn ? &s->weapon_rar : &s->armor_rar;
        uint8_t *pa = wpn ? &s->weapon_affix : &s->armor_affix;
        *pr = rar;
        *pa = affix_pack(&s->rng_state, rar, *pa);
        snprintf(buf, sizeof(buf), "Synth %s R%u!", wpn ? "wpn" : "arm", rar);
        log_push(s, buf);
        return true;
    }
    if (act == TOWN_WISH) {
        uint16_t cost = TUN_WISH_COST;
        // v12.1: the well is also the gold drain. Once both gear pieces hit
        // the forge cap, clears print money nothing absorbs (43k peaks in the
        // econ diag). Gold pricing is progressive (tax on the current pile):
        // hoards drain in a handful of wishes. A broke patron (death wiped
        // gold) still wishes at double essence, so the well never stalls.
        uint16_t gcost = (uint16_t)(TUN_WISH_GOLD_BASE +
                                    TUN_WISH_GOLD_STEP * (s->prestige + (uint8_t)s->ng) +
                                    s->gold / TUN_WISH_GOLD_TAX);
        if (s->gold >= gcost) {
            s->gold = (uint16_t)(s->gold - gcost);
        } else {
            cost = (uint16_t)(cost * 2);
            if (s->essence < cost) return false;
        }
        s->essence = (uint16_t)(s->essence - cost);
        // Pity: every TUN_WISH_PITY-th wish without an ancient guarantees one.
        int forced = (s->wish_pity + 1 >= TUN_WISH_PITY);
        uint32_t r = rng_range(&s->rng_state, 100);
        if (forced || r < TUN_WISH_ANCIENT_PCT) {
            drop_gear(s, true, true);
            if (s->weapon_rar < 4 && s->armor_rar < 4) {
                bool wpn = rng_range(&s->rng_state, 2);
                if (wpn) { s->weapon_rar = 4; s->weapon_pow = (uint8_t)(s->weapon_pow > 20 ? s->weapon_pow : 20); s->weapon_affix = affix_pack(&s->rng_state, 2, s->weapon_affix); }
                else { s->armor_rar = 4; s->armor_def = (uint8_t)(s->armor_def > 20 ? s->armor_def : 20); s->armor_affix = affix_pack(&s->rng_state, 2, s->armor_affix); }
            }
            s->wish_pity = 0;
            log_push(s, "The well grants an ANCIENT!");
        } else if (r < 20) {                       // 18%: gear (min blue)
            drop_gear(s, true, true);
            s->wish_pity++;
            log_push(s, "A gift of steel");
        } else if (r < 45) {                       // 25%: shards
            s->shards = (uint8_t)(s->shards + 6 > 99 ? 99 : s->shards + 6);
            s->wish_pity++;
            log_push(s, "A gift of shards");
        } else {                                   // 55%: gold burst
            uint16_t g = (uint16_t)(30 + s->floor * 2);
            s->gold = sat_add(s->gold, g);
            s->wish_pity++;
            snprintf(buf, sizeof(buf), "A gift of %u gold", g);
            log_push(s, buf);
        }
        return true;
    }
    if (act == TOWN_PRESTIGE) {
        // Escalating cost: level N needs ng >= 1+N/3. Flat ng>=1 let shallow
        // maze farms ramp prestige ~50 by run 60 (mazes_max=31 in sim) -- the
        // one-sided endgame the design forbids. Escalation forces deeper mazes.
        if (s->ng < (int)(1 + s->prestige / TUN_PRESTIGE_DIV)) return false;
        uint8_t old_prst = s->prestige;
        s->prestige = (uint8_t)(s->prestige + s->ng > 50 ? 50 : s->prestige + s->ng);
        s->ng = 0;
        s->floor = start_floor_of(s);
        roll_run_mut(s);
        job_rations(s);
        snprintf(buf, sizeof(buf), "Rebirth! lv %u (+2%%/lv)", s->prestige);
        log_push(s, buf);
        for (uint8_t lv = (uint8_t)(old_prst + 1); lv <= s->prestige; lv++) {
            const char *perk = rebirth_perk_of(lv);
            if (perk) { snprintf(buf, sizeof(buf), "Perk: %s", perk); log_push(s, buf); }
        }
        return true;
    }
    if (act == TOWN_UPGRADE && param >= 2) {
        // Peddler consumables at floor-scaled prices: survival now draws on
        // gold every deep run instead of riding free chest restocks.
        dungeon_cons_t ck = (param == 2) ? CONS_SMOKE : CONS_POTION;
        uint8_t cap = (param == 2) ? 5 : 3;
        if (s->cons[ck] >= cap) return false;
        uint16_t cost = peddler_cost(s, param == 2, s->floor);
        if (s->gold < cost) return false;
        s->gold = (uint16_t)(s->gold - cost);
        s->cons[ck]++;
        snprintf(buf, sizeof(buf), param == 2 ? "Smoke x%u (-%ug)" : "Potion x%u (-%ug)",
                 s->cons[ck], cost);
        log_push(s, buf);
        return true;
    }
    if (act == TOWN_UPGRADE) {
        // Quadratic cost: gold income scales linearly with depth, so a linear
        // price let deep runs buy tier to the cap in ~9 runs (v4.4 dump).
        // Quadratic keeps the gold->tier loop alive but decelerating.
        uint32_t stier = (uint32_t)s->weapon_pow + s->armor_def;
        uint16_t cost = (uint16_t)forge_cost(s, stier);
        if (s->gold < cost) return false;
        if (param == 0) {
            if (s->weapon_pow >= DUNGEON_GEAR_TIER_CAP) return false;
            s->weapon_pow++;
            snprintf(buf, sizeof(buf), "Weapon +%u (%ug)", s->weapon_pow, cost);
        } else {
            if (s->armor_def >= DUNGEON_GEAR_TIER_CAP) return false;
            s->armor_def++;
            snprintf(buf, sizeof(buf), "Armor +%u (%ug)", s->armor_def, cost);
        }
        s->gold = (uint16_t)(s->gold - cost);
        log_push(s, buf);
        return true;
    }
    if (act == TOWN_INN) {
        if (!perk_inn_free(s)) {
            if (s->gold < 10) return false;
            s->gold = (uint16_t)(s->gold - 10);
        }
        s->max_energy = energy_cap_of(s);
        s->energy = s->max_energy;
        s->hp_cur = max_hp_of(s);
        log_push(s, "Inn: rested full");
        return true;
    }
    return false;
}

bool dungeon_use_cons(dungeon_save_t *s, dungeon_cons_t kind)
{
    if (kind >= DUNGEON_CONS_SLOTS || !s->cons[kind]) return false;
    s->cons[kind]--;
    if (kind == CONS_POTION) {
        uint16_t mh = max_hp_of(s);
        uint16_t hp = s->hp_cur;
        uint8_t pct = 40;
        if (s->main_job == JOB_WHITE && s->job_lv[JOB_WHITE] >= TUN_WHITE_MASTERY_LV)
            pct = (uint8_t)(pct + TUN_WHITE_POTION_BONUS_PCT);
        hp = (uint16_t)(hp + mh * pct / 100 > mh ? mh : hp + mh * pct / 100);
        s->hp_cur = hp;
        log_push(s, pct >= 90 ? "Potion +90%" : "Potion +40%");
    } else if (kind == CONS_SMOKE) {
        flee_room(s);   // the ancient presence lets you go... this time
        log_push(s, "Smoke! fled room");
    } else {
        s->exploring = 0;
        s->n_enemy = 0;
        grant_pack(s);  // back in town: pack re-provisions for the next run
        log_push(s, "Feather home!");
    }
    return true;
}

// ---- explore ---------------------------------------------------------------
dungeon_event_t dungeon_explore(dungeon_save_t *s, uint64_t now_ms, uint64_t *next_due_ms)
{
    if (!s->exploring) return DEV_NONE;
    if (s->choice_pending) {
        if (now_ms >= s->choice_deadline_ms) {
            dungeon_choice_timeout(s, now_ms);
            if (next_due_ms) *next_due_ms = now_ms + DUNGEON_EXPLORE_MS;
            return DEV_CHOICE;
        }
        return DEV_NONE;
    }
    if (s->n_enemy > 0) return DEV_BATTLE;  // caller runs rounds
    if (next_due_ms) *next_due_ms = now_ms + DUNGEON_EXPLORE_MS;
    pair_distinct(s);
    auto_job_tick(s, now_ms);
    s->step_count++;
    if (s->step_count % 5 == 0 && s->energy > 0) s->energy--;
    if (s->energy == 0 && s->hp_cur > 1) s->hp_cur = (uint16_t)(s->hp_cur - 1);
    // entering new unlock floors
    for (uint8_t j = 0; j < DUNGEON_JOB_COUNT; j++) {
        const dungeon_job_def_t *d = dungeon_job_def((dungeon_job_t)j);
        if (!((s->unlocked >> j) & 1) && s->floor >= d->unlock_floor
            && j != JOB_KNIGHT && j != JOB_BLACK) {
            s->unlocked |= (uint8_t)(1u << j);
            s->codex |= (uint16_t)(1u << j);   // collection: job entry
            char buf[DUNGEON_LOG_LEN];
            snprintf(buf, sizeof(buf), "%s crystal glows!", JOB_NAMES[j]);
            log_push(s, buf);
        }
    }
    if (s->floor > s->top_floor) s->top_floor = s->floor;
    // STARFALL (v5.2): ultra-rare ambient event — the chamber waits next step.
    // F>=10 only: starfall boosts belong to the journey, not fresh-floor luck.
    if (!s->starfall_next && s->floor >= 10 &&
        (int)rng_range(&s->rng_state, 10000) < TUN_STARFALL_PPM) {
        s->starfall_next = 1;
        if (s->starfalls < 255) s->starfalls++;
        log_push(s, "A star falls from the ceiling!");
    }
    // Hidden boss intercepts the next step after its gate rumbled.
    if (s->hidden_next) {
        s->hidden_next = 0;
        start_battle(s, true);            // boss frame: must_bright, gate frame
        s->n_enemy = 1;
        s->enemy_hp[0] = (uint16_t)(s->enemy_hp[0] + s->enemy_hp[0] / 2);  // gate x1.5 more
        s->hidden_room = 1;
        s->codex |= (1u << 11);
        log_push(s, "An ancient presence looms!");
        return DEV_BOSS;
    }
    // Star-fall chamber: essence/shard burst, guaranteed gear, and a card.
    if (s->starfall_next) {
        s->starfall_next = 0;
        s->essence = sat_add(s->essence, 30);
        s->shards = (uint8_t)(s->shards + 5 > 99 ? 99 : s->shards + 5);
        drop_gear(s, true, true);
        s->codex |= (1u << 14);
        codex_maze_first(s, 3);  // v13: maze-N first ancient
        log_push(s, "Star-metal everywhere!");
        dungeon_choice_offer(s, now_ms);
        return DEV_CHOICE;
    }
    // Cursed door: the accepted challenge waits here.
    if (s->challenge_next) {
        s->challenge_next = 0;
        start_battle(s, false);
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            s->enemy_affix[i] = (uint8_t)(1 + rng_range(&s->rng_state, EA_COUNT - 1));
            if (i == 0) s->enemy_affix[0] = EA_GIANT;
            if (s->enemy_hp[i] < 45000)
                s->enemy_hp[i] = (uint16_t)(s->enemy_hp[i] + s->enemy_hp[i] / 4);
        }
        s->challenge_room = 1;
        log_push(s, "The challenge begins!");
        return DEV_BATTLE;
    }
    bool boss_floor = (s->floor % DUNGEON_BOSS_EVERY) == 0;
    if (boss_floor) {
        start_battle(s, true);
        return DEV_BOSS;
    }
    uint32_t r = rng_range(&s->rng_state, 100);
    if (has_affix(s, 4) && r < 65 && r >= 45) r = 60;   // chests+: 10 battle rolls become chests
    if (r < 55) {
        start_battle(s, false);
        return DEV_BATTLE;
    }
    if (r < 65) {
        // MIMIC role (Shiren-style): some chests bite. Thief sense halves it.
        uint32_t mc = TUN_MIMIC_CHANCE;
        if (s->main_job == JOB_THIEF) mc /= 2;
        if (s->floor >= 6 && (int)rng_range(&s->rng_state, 100) < (int)mc) {
            start_battle(s, false);
            s->n_enemy = 1;
            s->enemy_hp[0] = (uint16_t)(enemy_hp(s, false) + enemy_hp(s, false) / 2);
            s->enemy_kind[0] = theme_kind(s);
            s->enemy_affix[0] = EA_GIANT;
            s->mimic_room = 1;
            s->codex |= (1u << 9);
            log_push(s, "It's a mimic!");
            return DEV_BATTLE;
        }
        // Chest: gold mostly, consumables and deep shards sometimes.
        uint32_t cr = rng_range(&s->rng_state, 100);
        if (cr < 25 && s->cons[CONS_POTION] < 5) {
            s->cons[CONS_POTION]++;
            log_push(s, "Chest: potion!");
            return DEV_TREASURE;
        }
        // v12.1: free restock caps at the reserve (3) — beyond that the
        // peddler sells smoke at floor-scaled prices, so gold buys survival.
        if (cr < 33 && s->cons[CONS_SMOKE] < chest_smoke_cap(s)) {
            s->cons[CONS_SMOKE]++;
            log_push(s, "Chest: smoke!");
            return DEV_TREASURE;
        }
        if (cr < 43 && s->floor > 30) {
            s->shards = (uint8_t)(s->shards + 1 > 99 ? 99 : s->shards + 1);
            log_push(s, "Chest: shard!");
            return DEV_TREASURE;
        }
        uint16_t gold = (uint16_t)(3 + rng_range(&s->rng_state, 8));
        s->gold = sat_add(s->gold, gold);
        s->chests = sat_inc(s->chests);
        char buf[DUNGEON_LOG_LEN];
        snprintf(buf, sizeof(buf), "Chest +%ug", gold);
        log_push(s, buf);
        return DEV_TREASURE;
    }
    if (r < 72) {
        int taken = 2 + s->floor / 4 + (int)rng_range(&s->rng_state, 5);
        uint16_t hp = s->hp_cur;
        s->hp_cur = hp > taken ? (uint16_t)(hp - taken) : 0;
        if (s->hp_cur == 0) {
            death_apply(s);
            return DEV_DEAD;
        }
        log_push(s, "Trap! ouch");
        return DEV_TRAP;
    }
    if (r < 80) {
        s->hp_cur = max_hp_of(s);
        s->max_energy = energy_cap_of(s);
        s->energy = (uint8_t)(s->energy + 10 > s->max_energy ? s->max_energy : s->energy + 10);
        log_push(s, "Rest: breather");
        return DEV_REST;
    }
    // corridor: descend one floor (+1 essence, +TUN_DESC_HEAL% HP), choice
    // every 3rd descent. Floor-heal converts run viability into per-floor
    // economics the meta can win (MD/idle standard answer to attrition).
    if (s->floor < DUNGEON_MAX_FLOOR) {
        s->floor++;
        s->floor_affix = 0;   // a floor affix lives on its floor only
        s->essence = sat_add(s->essence, 1);
        uint16_t mh = max_hp_of(s);
        uint16_t dheal = (uint16_t)(mh * desc_heal_pct(s) / 100);
        if (dheal && s->hp_cur < mh)
            s->hp_cur = (uint16_t)(s->hp_cur + dheal > mh ? mh : s->hp_cur + dheal);
        gain_explore(s, 2);
        char buf[DUNGEON_LOG_LEN];
        snprintf(buf, sizeof(buf), "Down -> F%u", s->floor);
        log_push(s, buf);
        if (s->floor % 3 == 0) {
            dungeon_choice_offer(s, now_ms);
            return DEV_CHOICE;
        }
        return DEV_STAIR;
    }
    log_push(s, "Quiet... forward");
    return DEV_NONE;
}

// Enemy action tables: data, not code — a future adaptive Boss mutates the
// weights instead of branching. Weights are public knowledge (codex); the
// per-round roll is hidden from the autopilot (rolled inside dungeon_round,
// after dungeon_autopilot_battle has already run).
#define EACT_FIRST 0x01  // resolves in the flier first-strike phase
#define EACT_MULTI 0x02  // two hits, each dodged independently
#define EACT_DRAIN 0x04  // heals TUN_DRAIN_PCT% of dealt
#define EACT_HEAVY 0x08  // heavy: GRD x0.4 (perfect block), EVA slips it, ATK eats it
#define EACT_TRUE  0x10  // true hit: ignores EVA dodge (the designated turtle answer)
#define EACT_BUFF  0x20  // war cry: no damage, sets enemy_fury (the room rallies)

typedef struct { uint8_t w; uint8_t mult10; uint8_t flags; } eact_t;

// kinds: 0 beast 1 flier 2 undead 3 boss. mult10 scales D (= today's strike).
static const eact_t EACT_BEAST[2] = { {55, 10, 0}, {45, 24, EACT_HEAVY} };
static const eact_t EACT_FLIER[2] = { {60, 10, EACT_FIRST}, {40, 7, EACT_MULTI} };
static const eact_t EACT_UNDEAD[2] = { {55, 10, 0}, {45, 10, EACT_DRAIN} };
static const eact_t EACT_BOSS[3] = { {50, 24, EACT_HEAVY}, {30, 12, EACT_TRUE}, {20, 0, EACT_BUFF} };

static const eact_t *eact_roll(dungeon_save_t *s, uint8_t kind)
{
    const eact_t *tab = EACT_BEAST;
    uint8_t n = 2;
    switch (kind & 3) {
        case 1: tab = EACT_FLIER; break;
        case 2: tab = EACT_UNDEAD; break;
        case 3: tab = EACT_BOSS; n = 3; break;
        default: break;
    }
    uint8_t r = (uint8_t)rng_range(&s->rng_state, 100);
    uint8_t acc = 0;
    for (uint8_t k = 0; k < n; k++) {
        acc += tab[k].w;
        if (r < acc) return &tab[k];
    }
    return &tab[n - 1];  // weight-sum shortfall falls through to the last
}

// Home stance per job (threat-1 fallback): knight/white brace, black/dark
// press, thief slips. Indexed by dungeon_job_t.
static const uint8_t ST_HOME[DUNGEON_JOB_COUNT] =
    { ST_GRD, ST_ATK, ST_GRD, ST_EVA, ST_ATK };

// Round stance is recomputed inside dungeon_round from live state (no static:
// a stale stance once froze a battle when round ran without a prior autopilot
// pass). Manual play (autopilot off) resolves as ST_ATK = legacy behavior.
static uint8_t assess_threat(const dungeon_save_t *s);  // defined with the OC below

// ---- combat round ----------------------------------------------------------
// One enemy attack (flier first-strike and the main phase share it).
// Returns 1 if the player died. UNDEAD role: drains HP (race pressure).
// One enemy action resolution (flier first-strike and the main phase share it).
// act is the hidden roll for this foe this round; stance is the round stance.
// Returns 1 if the player died. D (= today's strike output) is scaled by the
// action mult first, then by the stance matrix.
// ONE per-hit damage math, two consumers: enemy_strike (reality) and
// enemy_threat_est (the OC's read of the field). v12.2: they had diverged —
// the AI compared raw atk sums against HP and smoked at shadows (user:
// full-HP smoke with the last foe one round from death), while the real hit
// went through blob + stance + shield and landed for a fraction of it.
static int blob_hit(const dungeon_save_t *s, uint16_t eatk, uint16_t edef)
{
    (void)s;
    // Blob curve (atk^2/(atk+guard_eff)): smooth from floor 1, never zero,
    // no cliff. v12.2: guard past the softcap halves — full-meta guard (~3x
    // foe atk) had collapsed deep damage to ~10/round (user report), but a
    // flat divisor also gutted the fresh bootstrap (F1 death loop), where
    // small guard is the ONLY defense.
    uint16_t geff = edef;
    if (geff > TUN_GRD_SOFTCAP)
        geff = (uint16_t)(TUN_GRD_SOFTCAP + (geff - TUN_GRD_SOFTCAP) / TUN_BLOB_GRD_DIV);
    int taken = (int)((uint32_t)eatk * eatk / (eatk + geff + 1));
    if (taken < 1) taken = 1;                       // pressure floor: never zero
    return taken;
}

static int enemy_strike(dungeon_save_t *s, uint8_t i, uint16_t edef, uint8_t swift,
                        const eact_t *act, uint8_t stance)
{
    if (swift && (int)rng_range(&s->rng_state, 100) < swift) return 0;
    if (s->main_job == JOB_BLACK && rng_range(&s->rng_state, 100) < 30) return 0;
    if (s->main_job == JOB_THIEF && rng_range(&s->rng_state, 100) < 30) return 0;
    if (act->flags & EACT_BUFF) {
        s->enemy_fury = 2;   // war cry rallies the room (later foes this round too)
        log_push(s, "Foe war cry!");
        return 0;
    }
    uint16_t eatk = enemy_atk(s, s->enemy_kind[i] == 3);
    if (s->enemy_fury) {
        uint32_t f = (uint32_t)eatk * TUN_FURY_MULT / 10;
        eatk = (uint16_t)(f > 250 ? 250 : f);
    }
    if (s->enemy_affix[i] == EA_VENOMOUS)
        eatk = (uint16_t)(eatk + eatk / 2 > 250 ? 250 : eatk + eatk / 2);
    int taken = blob_hit(s, eatk, edef);
    if (taken > 1) taken = 1 + (taken - 1) * TUN_EN_DMG_NUM / 10;  // scale the
    // excess above the 1-point design floor: x0.8 rounding must not zero it
    // V2 matrix: action mult, then stance (D is today's output by construction).
    taken = (taken * act->mult10) / 10;
    if (act->flags & EACT_HEAVY) {
        if (stance == ST_EVA) { if (g_beat) g_beat->flags |= BBEAT_DODGE; log_push(s, "Evaded heavy!"); return 0; }
        if (stance == ST_GRD) { taken = (taken * TUN_ST_GRD_HEAVY) / 10; if (g_beat) g_beat->flags |= BBEAT_BLOCK; log_push(s, "Blocked heavy!"); }
    } else if (stance == ST_GRD) {
        taken = (taken * TUN_ST_GRD_TAKEN) / 10;
    } else if (stance == ST_EVA && !(act->flags & EACT_TRUE)) {
        if ((int)rng_range(&s->rng_state, 100) < TUN_EVADE_DODGE) { if (g_beat) g_beat->flags |= BBEAT_DODGE; log_push(s, "Evaded!"); return 0; }
    }
    if (taken < 1) taken = 1;   // V2: matrix mults must not zero a landed hit
                                // (clean dodges above stay 0; shield may still soak)
    if (s->enrage && s->enemy_kind[i] == 3) taken = taken * 13 / 10;
    if (s->enemy_weak) taken /= 2;
    if (s->shield_pool) {
        if (taken <= s->shield_pool) { s->shield_pool = (uint8_t)(s->shield_pool - taken); taken = 0; }
        else { taken -= s->shield_pool; s->shield_pool = 0; }
    }
    if (taken > 0 && s->enemy_kind[i] == 2 && s->player_curse < 2)
        s->player_curse = 2;                    // disturb signature: -20% pow
    if (taken > 0 && s->enemy_kind[i] == 2 && TUN_UNDEAD_DRAIN > 0) {
        uint16_t cap = enemy_hp(s, false);
        uint16_t heal = (uint16_t)(taken / TUN_UNDEAD_DRAIN);
        if (heal && s->enemy_hp[i] < cap)
            s->enemy_hp[i] = (uint16_t)((s->enemy_hp[i] + heal > cap) ? cap : s->enemy_hp[i] + heal);
    }
    if ((act->flags & EACT_DRAIN) && taken > 0 && TUN_DRAIN_PCT > 0) {
        uint16_t cap = enemy_hp(s, false);   // V2 leech action: % of applied hit
        uint16_t heal = (uint16_t)(taken * TUN_DRAIN_PCT / 100);
        if (heal && s->enemy_hp[i] < cap)
            s->enemy_hp[i] = (uint16_t)((s->enemy_hp[i] + heal > cap) ? cap : s->enemy_hp[i] + heal);
    }
    uint16_t hp = s->hp_cur;
    s->hp_cur = hp > taken ? (uint16_t)(hp - taken) : 0;
    beat_taken(s, hp);
    if (taken > 0 && has_affix(s, 3) && s->enemy_hp[i]) {   // thorn floor: bite back
        uint16_t ref = (uint16_t)(1 + edef / 2);
        { uint16_t was = s->enemy_hp[i];
          s->enemy_hp[i] = s->enemy_hp[i] > ref ? (uint16_t)(s->enemy_hp[i] - ref) : 0;
          beat_dealt(s, i, was); }
    }
    return s->hp_cur == 0;
}

// ---- combat round (cont.) --------------------------------------------------
// Legacy wrapper (beat capture off). Presenting callers use _ex.
// ---- v12 combat stance ------------------------------------------------
// Three stances, picked per round by the autopilot. Creates the tactical
// layer: the OC reads the situation and picks HOW to fight.
//
//   0=ATK  : deal x1.2, take x1.0  (aggressive: push advantage)
//   1=GUARD: deal x0.5, take x0.5, +2 shield  (defensive: brace)
//   2=REST : deal x0.0, take x0.7, +2 HP regen  (recover: buy time)
//
// Job personality: the autopilot prefers the job's home stance when threat
// is low, and switches to survival stance when threat is high.

typedef enum { ST_ATTACK = 0, ST_GUARD = 1, ST_REST = 2 } combat_stance_t;


dungeon_event_t dungeon_round(dungeon_save_t *s, uint64_t now_ms)
{
    return dungeon_round_ex(s, now_ms, NULL);
}

dungeon_event_t dungeon_round_ex(dungeon_save_t *s, uint64_t now_ms, battle_beat_t *beat)
{
    g_beat = beat;
    if (beat) { *beat = (battle_beat_t){0}; beat->span = 1; }
    (void)now_ms;
    if (s->n_enemy == 0 || !alive_count(s)) return DEV_NONE;
    s->round_ct++;
    if (s->enemy_fury) s->enemy_fury--;   // V2 war-cry buff clock
    // V2 stance, recomputed from live state (manual = ATK = legacy behavior).
    uint8_t stance = s->autopilot ? dungeon_stance_pick(s, assess_threat(s)) : ST_ATK;
    if (g_beat) g_beat->stance = stance;
    if (stance == ST_GRD) {
        uint16_t sp = (uint16_t)s->shield_pool + TUN_ST_GRD_SHIELD;
        { uint8_t scap = shield_cap_of(s);
          if (sp > scap) sp = scap; }
        s->shield_pool = (uint8_t)sp;
    }
    // venom/doom ticks on all.
    if (s->enemy_dot) {
        s->enemy_dot--;
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i]) continue;
            uint16_t dot = 3;  // venom must carry thief's race (no sustain kit)
            if (s->main_job == JOB_THIEF) dot += s->job_lv[JOB_THIEF] / 4;  // v31 venom mastery
            { uint16_t was = s->enemy_hp[i];
              s->enemy_hp[i] = (s->enemy_hp[i] > dot) ? (uint16_t)(s->enemy_hp[i] - dot) : 0;
              beat_dealt(s, i, was); }
        }
    }
    // FLIER role: ranged first strike — they shoot before the party acts.
    {
        uint16_t edef0 = guard_of(s);
        uint8_t swift0 = (uint8_t)(12 * affix_count(s, AF_SWIFT));
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i] || s->enemy_kind[i] != 1) continue;
            if (s->enemy_slow && (s->round_ct % 2 == 0)) continue;  // respect slow
            const eact_t *act0 = eact_roll(s, s->enemy_kind[i]);  // hidden roll
            if (g_beat && (act0->flags & EACT_HEAVY)) g_beat->flags |= BBEAT_HEAVY;
            if (!(act0->flags & EACT_FIRST)) continue;  // feather resolves main phase
            if (enemy_strike(s, i, edef0, swift0, act0, stance)) {
                death_apply(s);
                return DEV_DEAD;
            }
        }
    }
    // Evolved ultimate: round 3, once per battle (idle-first drama beat).
    if (s->round_ct == 3 && dungeon_ult_ready(s)) {
        s->ult_used = 1;
        if (g_beat) g_beat->flags |= BBEAT_ULT;
        ult_fire(s);
    }
    // player auto-attack by AI.
    int t = pick_target(s);
    uint16_t pow = pow_of(s);
    // EVA slips the whole offensive phase (basics AND auto-sustain): dodging
    // and bandaging don't mix. This is the structural anti-turtle guarantee —
    // every stance either deals damage or bleeds, so no equilibrium stalls.
    // Same for the all-in race: an in-round cure would heal over the all-in
    // line (and eat the basic via acted), un-racing the race.
    uint16_t mh0 = max_hp_of(s);
    bool racing = (dungeon_combat_margin(s) < 0 && !s->cons[CONS_POTION]
                   && !s->cons[CONS_SMOKE] && s->hp_cur * 10 < mh0 * 3);
    bool acted = (stance == ST_EVA);
    if (!acted && !racing && s->main_job == JOB_WHITE && !s->skill_cd[0]) {
        uint16_t hp = s->hp_cur, mh = max_hp_of(s);
        if (hp * 10 < mh * 6) {
            // v16b: the medic identity is lv1 — fresh white sustains from the
            // first battle (the lv6 CURE skill keeps its stronger cast).
            uint16_t heal = dungeon_skill_unlocked(s, JOB_WHITE, 0)
                ? (uint16_t)((6 + s->job_lv[JOB_WHITE] * 2) * 3 / 2)
                : (uint16_t)(4 + s->job_lv[JOB_WHITE]);
            s->hp_cur = hp + heal > mh ? mh : (uint16_t)(hp + heal);
            { uint8_t scap = shield_cap_of(s);
              s->shield_pool = s->shield_pool + 2 > scap ? scap : (uint8_t)(s->shield_pool + 2); }
            s->skill_cd[0] = 3;
            log_push(s, "White auto-cure");
            acted = true;
        }
    }
    if (!acted && s->main_job == JOB_BLACK && dungeon_skill_unlocked(s, JOB_BLACK, 0) && !s->skill_cd[0] && alive_count(s) >= 2) {
        s->skill_cd[0] = 3;
        if (s->energy >= 2) s->energy -= 2;
        skill_fire(s, SK_FIRE);
        acted = true;
    }
    if (!acted && s->main_job == JOB_KNIGHT && dungeon_skill_unlocked(s, JOB_KNIGHT, 0) && !s->skill_cd[0] && s->round_ct <= 2) {
        s->skill_cd[0] = 4;
        if (s->energy >= 2) s->energy -= 2;
        skill_fire(s, SK_SHIELD_BASH);
        acted = true;
    }
    if (!acted && s->main_job == JOB_THIEF && dungeon_skill_unlocked(s, JOB_THIEF, 0) && !s->skill_cd[0] && s->round_ct == 1) {
        s->skill_cd[0] = 4;
        if (s->energy >= 2) s->energy -= 2;
        skill_fire(s, SK_VENOM);
        acted = true;
    }
    if (!acted && s->main_job == JOB_DARK && dungeon_skill_unlocked(s, JOB_DARK, 0) && !s->skill_cd[0]
        && (alive_count(s) >= 2 || s->hp_cur * 10 < max_hp_of(s) * 6)) {
        s->skill_cd[0] = 3;
        if (s->energy >= 2) s->energy -= 2;
        skill_fire(s, SK_DRAIN);
        acted = true;
    }
    if (!acted && t >= 0 && stance != ST_EVA) {   // EVA deals nothing (COMBAT_V2 §4)
        // SWIFT elites dodge basic attacks (skills always land).
        if (s->enemy_affix[t] == EA_SWIFT && (int)rng_range(&s->rng_state, 100) < 20) {
            log_push(s, "Attacked... dodged!");
        } else {
            uint8_t rest = RESTRAIN[s->main_job][s->enemy_kind[t] & 3];
            int d = damage(pow, 0, 10, rest, &s->rng_state);  // white's low M lives in the job table now
            if (s->main_job == JOB_THIEF && s->round_ct <= 2) d = (d * 15) / 10;  // backstab opener
            if (s->main_job == JOB_BLACK && s->round_ct == 1) d = (d * 13) / 10;  // v16: opening burst
            // dark execute: target below 1/3 of depth-typical HP
            bool dark_execute = false;
            if (s->main_job == JOB_DARK) {
                uint16_t third = enemy_hp(s, false) / 3;
                if (third && s->enemy_hp[t] < third) { d = (d * 15) / 10; dark_execute = true; }
            }
            if (stance == ST_GRD) d = (d * TUN_ST_GRD_DEALT) / 10;   // V2 stance mult
            else d = (d * TUN_ST_ATK_DEALT) / 10;  // ATK here (EVA skipped above)
            if (d < 1) d = 1;   // V2: stance mults must not zero a hit (GRD still chips)
            d = post_slayer(s, s->enemy_kind[t], d);
            d = deal_to_enemy(s, (uint8_t)t, d);
            if (dark_execute && !s->enemy_hp[t] && s->hp_cur) {   // v16: execute leech
                uint16_t mh2 = max_hp_of(s);
                s->hp_cur = s->hp_cur + 8 > mh2 ? mh2 : (uint16_t)(s->hp_cur + 8);
            }
            if ((dungeon_item_has_affix(s->weapon_affix, AF_VENOM)
                 || dungeon_item_has_affix(s->armor_affix, AF_VENOM)) && !s->enemy_dot)
                s->enemy_dot = 2;
            // THORNS elite reflects atk/2 on the striker.
            if (s->enemy_affix[t] == EA_THORNS && s->hp_cur > 0) {
                int ref = enemy_atk(s, s->enemy_kind[t] == 3) / 2;
                if (s->shield_pool) {
                    if (ref <= s->shield_pool) { s->shield_pool = (uint8_t)(s->shield_pool - ref); ref = 0; }
                    else { ref -= s->shield_pool; s->shield_pool = 0; }
                }
                if (ref > 0) {
                    uint16_t hp = s->hp_cur;
                    s->hp_cur = hp > ref ? (uint16_t)(hp - ref) : 0;
                    beat_taken(s, hp);   // thorned elite retaliation
                    if (s->hp_cur == 0) {
                        death_apply(s);
                        return DEV_DEAD;
                    }
                }
            }
        }
    }
    for (uint8_t i = 0; i < 3; i++)
        if (s->skill_cd[i]) s->skill_cd[i]--;
    if (stance == ST_EVA)   // slip cycles skills: one extra tick (COMBAT_V2 §4)
        for (uint8_t i = 0; i < 3; i++)
            if (s->skill_cd[i]) s->skill_cd[i]--;
    // enemies act (slow: every other round).
    bool skip = s->enemy_slow && (s->round_ct % 2 == 0);
    if (s->enemy_slow) s->enemy_slow--;
    if (s->enemy_weak) s->enemy_weak--;
    if (!skip) {
        uint16_t edef = guard_of(s);
        uint8_t swift = (uint8_t)(12 * affix_count(s, AF_SWIFT));
        for (uint8_t i = 0; i < s->n_enemy; i++) {
            if (!s->enemy_hp[i]) continue;
            const eact_t *act = eact_roll(s, s->enemy_kind[i]);  // hidden roll
            if (g_beat && (act->flags & EACT_HEAVY)) g_beat->flags |= BBEAT_HEAVY;
            if (s->enemy_kind[i] == 1 && (act->flags & EACT_FIRST)) continue;  // already struck first
            uint8_t hits = (act->flags & EACT_MULTI) ? 2 : 1;
            for (uint8_t h = 0; h < hits; h++) {
                if (!s->enemy_hp[i] || !s->hp_cur) break;  // thorn-floor suicide ends the flurry
                if (enemy_strike(s, i, edef, swift, act, stance)) {
                    death_apply(s);
                    return DEV_DEAD;
                }
            }
        }
    }
    // WHITE signature: regeneration stance — slow to kill, slow to kill with.
    // Suppressed while slipping (see above): no bandaging mid-dodge.
    if (s->main_job == JOB_WHITE && s->hp_cur && stance != ST_EVA) {
        uint16_t mh = max_hp_of(s);
        uint16_t regen = (uint16_t)(TUN_WHITE_REGEN + s->job_lv[JOB_WHITE] / 2);
        if (regen && s->hp_cur < mh)
            s->hp_cur = (uint16_t)(s->hp_cur + regen > mh ? mh : s->hp_cur + regen);
    }
    if (!alive_count(s)) return clear_battle(s, now_ms);
    return DEV_BATTLE;
}

// Shared battle-clear path: round() and manual cast() both land here, so a
// player-cast killing blow can never leave n_enemy>0 with all HP 0 (softlock).
static dungeon_event_t clear_battle(dungeon_save_t *s, uint64_t now_ms)
{
    // Hidden boss bounty: prestige +1, essence/shard burst, guaranteed deep
    // drop — and no extra descent (it haunts the floor after its gate).
    if (s->hidden_room) {
        s->hidden_room = 0;
        s->n_enemy = 0;                    // shared battle cleanup — playbook 02:
        s->enemy_slow = s->enemy_weak = s->enemy_dot = 0;   // EVERY clear funnels
        s->shield_pool = 0;                // through the same state reset
        // Treasure-vault bounty: materials + guaranteed gold-tier gear. NO
        // prestige here — v8 sim showed +1 rebirth/kill compounds the meta
        // spiral past the bounded ceiling (ng+prst 10 -> 35 at 150 runs).
        s->essence = sat_add(s->essence, 50);
        s->shards = (uint8_t)(s->shards + 10 > 99 ? 99 : s->shards + 10);
        drop_gear(s, true, true);
        drop_gear(s, true, true);
        if (s->weapon_rar < 3) { s->weapon_rar = 3; s->weapon_affix = affix_pack(&s->rng_state, 3, s->weapon_affix); }
        if (s->armor_rar < 3) { s->armor_rar = 3; s->armor_affix = affix_pack(&s->rng_state, 3, s->armor_affix); }
        log_push(s, "Ancient bounty!");
        dungeon_choice_offer(s, now_ms);
        return DEV_CHOICE;
    }
    bool boss = (s->enemy_kind[0] == 3);
    bool elite = boss || s->n_enemy >= 3;
    for (uint8_t i = 0; i < s->n_enemy; i++)
        if (s->enemy_affix[i] != EA_NONE) elite = true;  // elite rooms pay out
    // Leech affix: 8% max HP per kill in the room.
    uint8_t dead = 0;
    for (uint8_t i = 0; i < s->n_enemy; i++)
        if (!s->enemy_hp[i]) dead++;
    if (dead && affix_count(s, AF_LEECH)) {
        uint16_t mh = max_hp_of(s);
        uint16_t heal = (uint16_t)(mh / 12 * dead);
        s->hp_cur = (uint16_t)(s->hp_cur + heal > mh ? mh : s->hp_cur + heal);
    }
    // Black mastery: +essence per kill (identity harvest).
    if (dead && s->main_job == JOB_BLACK
        && s->job_lv[JOB_BLACK] >= TUN_BLACK_MASTERY_LV)
        s->essence = sat_add(s->essence,
            (uint16_t)(TUN_BLACK_HARVEST_ESSENCE * dead));
    s->n_enemy = 0;
    s->enemy_slow = s->enemy_weak = s->enemy_dot = 0;
    s->shield_pool = 0;
    loot_room(s, boss, elite);
    if (boss) {
        if (s->floor >= DUNGEON_MAX_FLOOR) {
            codex_maze_first(s, 1);  // v13: maze-N first clear (pre-increment ng)
            s->ng++;
            s->floor = start_floor_of(s);
            s->floor_affix = 0;
            roll_run_mut(s);
            job_rations(s);
            log_push(s, "MAZE CLEAR! badge +1");
            return DEV_CLEAR;
        }
        s->floor++;
        s->floor_affix = 0;
        s->essence = sat_add(s->essence, TUN_GATE_ESSENCE);
        gain_explore(s, 2);
        // HIDDEN boss (v5.1): mid-maze gates may hide an ancient presence —
        // a tougher gate on the next step, with a deep bounty.
        uint8_t gate_was = (uint8_t)(s->floor - 1);
        if (s->run_mut) {        // v15: mutant bounty — the in-life reward beat
            uint16_t g = (uint16_t)(10 + gate_was * 2);
            s->gold = sat_add(s->gold, g);
            if (s->mut_bounty < 255) s->mut_bounty++;
            char bbuf[DUNGEON_LOG_LEN];
            snprintf(bbuf, sizeof(bbuf), "Mutant bounty +%ug!", g);
            log_push(s, bbuf);
        }
        if (gate_was == 25 || gate_was == 50 || gate_was == 75) {
            if ((int)rng_range(&s->rng_state, 100) < TUN_HIDDEN_PCT) {
                s->hidden_next = 1;
                log_push(s, "The floor trembles...");
            }
        }
    }
    if (elite) {
        dungeon_choice_offer(s, now_ms);
        return DEV_CHOICE;
    }
    return DEV_BATTLE;
}

// Saved deadlines are boot-relative ms; after a reboot they point hours into
// the future (or past). Re-anchor them to the new clock.
void dungeon_rebase_clock(dungeon_save_t *s, uint64_t now_ms)
{
    s->switch_ready_ms = 0;
    if (s->choice_pending) s->choice_deadline_ms = now_ms + DUNGEON_CHOICE_TIMEOUT_MS;
    else s->choice_deadline_ms = 0;
    // v11 fix (player report: OC stands in town forever after flashing):
    // with autopilot on, a save made mid-town booted into town and the UI
    // beat never re-armed, so the OC never descended. The OC PLAYS ITSELF
    // when autopilot is on — boot re-joins the run.
    if (s->autopilot && !s->exploring) s->exploring = 1;
}

void dungeon_offline(dungeon_save_t *s, uint32_t secs, uint64_t now_ms)
{
    char buf[DUNGEON_LOG_LEN];
    if (secs > DUNGEON_OFFLINE_CAP_SEC) secs = DUNGEON_OFFLINE_CAP_SEC;
    if (!s->exploring) {
        log_push(s, "Offline: rested");
        return;
    }
    uint8_t f0 = s->floor;
    uint16_t g0 = s->gold;
    uint16_t d0 = s->deaths;
    // init hp on first run (fresh saves start 0).
    if (s->hp_cur == 0) s->hp_cur = max_hp_of(s);
    uint32_t steps = secs * 1000 / DUNGEON_EXPLORE_MS;
    uint64_t t = now_ms;
    for (uint32_t i = 0; i < steps; i++) {
        if (s->choice_pending) { dungeon_choice_timeout(s, t); continue; }
        if (s->n_enemy > 0) {
            // gatekeeper: never auto-fight bright bosses offline — not even
            // one round, and the flag must survive so the NEXT settlement
            // stops here too.
            if (s->must_bright) break;
            dungeon_event_t e = dungeon_round(s, t);
            if (e == DEV_DEAD) break;
            continue;
        }
        // stop before a gatekeeper floor: needs bright screen.
        if (s->floor % DUNGEON_BOSS_EVERY == 0) break;
        if (s->energy == 0) break;
        t += DUNGEON_EXPLORE_MS;
        dungeon_event_t e = dungeon_explore(s, t, NULL);
        if (e == DEV_DEAD) break;
        if (e == DEV_BOSS) break;  // reached a gate: wait for bright
    }
    uint16_t dg = s->gold >= g0 ? (uint16_t)(s->gold - g0) : 0;  // death zeroes gold mid-run
    // NOTE: 64B staging: -Werror=format-truncation on device; log_push clips to 40.
    char wide[64];
    if (s->deaths != d0)
        snprintf(wide, sizeof(wide), "Away %um: F%u->F%u +%ug died",
                 (unsigned)(secs / 60), f0, s->floor, (unsigned)dg);
    else
        snprintf(wide, sizeof(wide), "Away %um: F%u->F%u +%ug safe",
                 (unsigned)(secs / 60), f0, s->floor, (unsigned)dg);
    log_push(s, wide);
    (void)buf;
}

// ---- autopilot (v10) -------------------------------------------------------
// This is the balance sim's greedy bot moved into the model: the OC plays
// the game the way the sim proved it can be played. Manual keys still work
// on top (a cast/switch/pick just lands first).

uint8_t dungeon_choice_auto(const dungeon_save_t *s)
{
    // healthy enough -> the cursed door is the best expected-value card
    if (s->hp_cur * 10 > max_hp_of(s) * 7)
        for (uint8_t k = 0; k < DUNGEON_CHOICE_OPTS; k++)
            if (s->choice_opts[k] == CH_CHALLENGE) return k;
    static const uint8_t PREF[] = {CH_BLESS_ATK, CH_BLESS_DEF, CH_BLESS_LEARN,
                                   CH_SHRINE, CH_SHARDS};
    for (uint8_t p = 0; p < sizeof(PREF); p++)
        for (uint8_t k = 0; k < DUNGEON_CHOICE_OPTS; k++)
            if (s->choice_opts[k] == PREF[p]) return k;
    return 0;
}

// Combat assessment: measures the ACTUAL combat state each round —
// "can I kill them before they kill me?" — not absolute HP thresholds.
// Returns threat 0-3: 0=crushing, 1=winning slow, 2=losing, 3=dying now.
// Shared combat clock: rounds-to-die vs rounds-to-win. assess_threat and
// dungeon_combat_margin read the same numbers; the former's behavior is
// unchanged (V2 only exposes the margin to the action layer).
static void combat_clock(const dungeon_save_t *s, uint16_t *r_die, uint16_t *r_win)
{
    uint16_t pow = dungeon_pow(s);
    uint16_t my_ehp = s->hp_cur + s->shield_pool;
    uint16_t ehp = 0, edps = 0;
    for (uint8_t i = 0; i < s->n_enemy; i++) {
        if (!s->enemy_hp[i]) continue;
        ehp += s->enemy_hp[i];
        edps += enemy_atk(s, s->enemy_kind[i] == 3);
    }
    if (edps == 0) { *r_die = 99; *r_win = 0; return; }  // nothing shooting at me
    *r_die = (uint16_t)(my_ehp / edps);        // rounds before death
    *r_win = (uint16_t)(ehp / (pow ? pow : 1)); // rounds to clear the room
}

int16_t dungeon_combat_margin(const dungeon_save_t *s)
{
    uint16_t r_die, r_win;
    combat_clock(s, &r_die, &r_win);
    int32_t m = (int32_t)r_die - (int32_t)r_win;
    if (m > 99) m = 99;
    if (m < -99) m = -99;
    return (int16_t)m;
}

static uint8_t assess_threat(const dungeon_save_t *s)
{
    uint16_t r_die, r_win;
    combat_clock(s, &r_die, &r_win);
    // Note: edps==0 yields r_win==0 -> first branch, same as the old early return.
    if (r_win * 2 <= r_die) return 0;    // crushing: win in half the time
    if (r_win <= r_die) return 1;        // winning but slow
    if (r_die * 2 >= r_win) return 2;    // losing slowly
    return 3;                             // dying in <2 rounds
}

// The OC estimates the NEXT round's real damage: same blob math the enemies
// roll, with stance expectation instead of dodge RNG. v12.2 replaces the raw
// atk-sum read that fired smoke while the blob + guard + shield chain was
// about to shrug the hit off (user: smoke with the last foe nearly dead).
static uint16_t enemy_threat_est(const dungeon_save_t *s, uint8_t stance)
{
    uint16_t edef = guard_of(s);
    uint32_t est = 0;
    for (uint8_t i = 0; i < s->n_enemy; i++) {
        if (!s->enemy_hp[i]) continue;
        uint16_t eatk = enemy_atk(s, s->enemy_kind[i] == 3);
        if (s->enemy_fury) {
            uint32_t f = (uint32_t)eatk * TUN_FURY_MULT / 10;
            eatk = (uint16_t)(f > 250 ? 250 : f);
        }
        if (s->enemy_affix[i] == EA_VENOMOUS)
            eatk = (uint16_t)(eatk + eatk / 2 > 250 ? 250 : eatk + eatk / 2);
        int hit = blob_hit(s, eatk, edef);
        if (hit > 1) hit = 1 + (hit - 1) * TUN_EN_DMG_NUM / 10;
        // action profile: multi-hitters swing twice; fliers add their first
        // strike; war-cry buffs are skipped (no damage, sets fury next round).
        int hits = 1;
        if (s->enemy_kind[i] == 1) hits++;                      // flier volley
        est += (uint32_t)hit * hits;
    }
    // stance expectation: ATK x1.0, GRD xTUN_ST_GRD_TAKEN (heavies dodge the
    // read, close enough), EVA = 1 - dodge chance.
    static const uint8_t EV[3] = {100, TUN_ST_GRD_TAKEN * 10, 100 - TUN_EVADE_DODGE};
    est = est * EV[stance % 3] / 100;
    return (uint16_t)(est > 60000 ? 60000 : est);
}

// Expected clear time under GRD (restraint x GRD mult included): the
// no-marathon guard measures what bracing actually costs in rounds.
static uint16_t grd_kill_rounds(const dungeon_save_t *s)
{
    int t = pick_target(s);
    if (t < 0) return 0;
    uint16_t ehp = 0;
    for (uint8_t k = 0; k < s->n_enemy; k++) ehp += s->enemy_hp[k];
    uint8_t rest = RESTRAIN[s->main_job][s->enemy_kind[t] & 3];
    uint32_t per = (uint32_t)dungeon_pow(s) * rest / 10;
    per = per * TUN_ST_GRD_DEALT / 10;
    if (!per) per = 1;
    return (uint16_t)(ehp / per);
}

// EVA bridge: slipping is only allowed with a purpose — an offensive skill
// coming online within 3 rounds (the extra tick shortens its fuse). A
// purposeless turtle has no exit but bleeding. CURE never counts (suppressed
// on EVA rounds anyway). Slots: 0 = main first active, 2 = sub first active.
static bool eva_bridge(const dungeon_save_t *s)
{
    const dungeon_job_def_t *m0 = dungeon_job_def((dungeon_job_t)s->main_job);
    const dungeon_job_def_t *s0 = dungeon_job_def((dungeon_job_t)s->sub_job);
    if (m0->active[0] != SK_CURE && dungeon_skill_unlocked(s, s->main_job, 0)
        && s->skill_cd[0] >= 1 && s->skill_cd[0] <= 3) return true;
    if (s0->active[0] != SK_CURE && dungeon_skill_unlocked(s, s->sub_job, 0)
        && s->skill_cd[2] >= 1 && s->skill_cd[2] <= 3) return true;
    return false;
}

uint8_t dungeon_stance_pick(const dungeon_save_t *s, uint8_t threat)
{
    // Soft-enrage first: past round 100 the battle must end (pacing backstop
    // for fat-pool slow bleeds that would otherwise outlast the 200 cap).
    // ATK still chips >=1, so the race always concludes. Cure stays allowed.
    if (s->round_ct > TUN_RACE_ROUNDS) return ST_ATK;
    // v23 finishing-blow override (B1 charter forensics): a foe one hit from
    // death must never be EVA-turtled (zero offense) into a 50-round bleed —
    // fresh non-knight lives were dying exactly this way (round_ct 50+ vs a
    // weak foe). Pressing the attack also ends the stall.
    {
        uint32_t ehp_tot = 0;
        for (uint8_t i = 0; i < s->n_enemy; i++) ehp_tot += s->enemy_hp[i];
        if (ehp_tot && ehp_tot <= (uint32_t)dungeon_pow(s) * 2) return ST_ATK;
    }
    // All-in: dying, nothing left to drink/smoke, and too frail for guard to
    // buy a round (playbook docs/COMBAT_V2.md §6). GRD/EVA only delay certain
    // death down here; ATK is the only nonzero win chance. Note a healthy-but-
    // outgunned OC (margin<0, hp high) still braces: mitigation buys rounds.
    uint16_t mh = max_hp_of(s);
    if (dungeon_combat_margin(s) < 0
        && !s->cons[CONS_POTION] && !s->cons[CONS_SMOKE]
        && s->hp_cur * 10 < mh * 3) return ST_ATK;
    uint8_t st;
    switch (threat) {
        case 0: st = ST_ATK; break;
        case 1: {
            // Winning slowly: turtle only with a purpose. GRD must finish fast
            // (a finishing posture, not a way of life; 8 mirrors the C band);
            // EVA needs a skill bridge. Otherwise press the advantage.
            uint8_t home = ST_HOME[s->main_job < DUNGEON_JOB_COUNT ? s->main_job : 0];
            if (home == ST_GRD && grd_kill_rounds(s) > 8) return ST_ATK;
            if (home == ST_EVA && !eva_bridge(s)) return ST_ATK;
            st = home; break;
        }
        case 2: st = ST_GRD; break;
        default: st = ST_EVA; break;
    }
    // No-marathon: a GRD that needs 15+ rounds to clear is a pacing failure —
    // brace-and-chip turns tough fights into slogs (and slows replays).
    // Race it instead. EVA is exempt (1-round tactical slip, always bleeds).
    if (st == ST_GRD && grd_kill_rounds(s) > TUN_MARATHON_ROUNDS) return ST_ATK;
    return st;
}

// ---- V2 playback: streaming剪枝机 -------------------------------------------
// Budget proof: pressure point P = B - O - 3K - D (B=budget, O=outcome,
// K=highlight cost, D=digest cost). Past P only kill-beats SHOW (each foe
// dies once: <=3 per battle) and the single terminal digest costs D once:
// total <= P + 3K + D + O = B, always. Digest totals are exact (sat-capped).
static int beat_is_highlight(const battle_beat_t *b)
{
    return (b->flags & (BBEAT_HEAVY | BBEAT_BLOCK | BBEAT_DODGE | BBEAT_SKILL | BBEAT_ULT)) || b->foes;
}
void beat_player_init(beat_player_t *p, uint32_t budget_ms)
{
    p->budget_ms = budget_ms ? budget_ms : TUN_BPLAY_BUDGET_MS;
    p->spent_ms = 0;
    p->digest = (battle_beat_t){0};
}
bplay_act_t beat_player_push(beat_player_t *p, const battle_beat_t *b)
{
    uint32_t reserve = TUN_BPLAY_OUTCOME_MS + 3u * TUN_BPLAY_HL_MS + TUN_BPLAY_DIGEST_MS;
    uint32_t pat = p->budget_ms > reserve ? p->budget_ms - reserve : 0;
    int pressured = p->spent_ms > pat;
    int show = beat_is_highlight(b)
               && (!pressured || b->foes || (b->flags & BBEAT_ULT));
    if (!show) {
        battle_beat_t *d = &p->digest;
        if (!d->span) { *d = *b; d->span = 1; }
        else {
            uint32_t dd = (uint32_t)d->dealt + b->dealt;
            uint32_t dt = (uint32_t)d->taken + b->taken;
            d->dealt = (uint16_t)(dd > 60000 ? 60000 : dd);
            d->taken = (uint16_t)(dt > 60000 ? 60000 : dt);
            if (d->span < 255) d->span++;
            d->stance = b->stance;
        }
        d->flags |= BBEAT_MERGED | (b->flags & (BBEAT_HURT | BBEAT_HEAVY | BBEAT_BLOCK | BBEAT_DODGE));
        return BPLAY_MERGE;
    }
    p->spent_ms += TUN_BPLAY_HL_MS;
    return BPLAY_SHOW;
}
int beat_player_has_digest(const beat_player_t *p) { return p->digest.span != 0; }
void beat_player_take_digest(beat_player_t *p, battle_beat_t *out)
{
    *out = p->digest;
    p->digest = (battle_beat_t){0};
    p->spent_ms += TUN_BPLAY_DIGEST_MS;
}
uint32_t beat_player_total(const beat_player_t *p)
{
    return p->spent_ms + (p->digest.span ? TUN_BPLAY_DIGEST_MS : 0) + TUN_BPLAY_OUTCOME_MS;
}

// Combat reasoning: the OC reads the field like a player would.
//
// Priority 1 — "Can I finish them?"  enemy_hp ≤ my burst → attack to end it.
// Priority 2 — "Am I about to die?"  incoming ≥ my EHP → smoke or potion.
// Priority 3 — "Am I losing the race?"  r_die < r_win → switch to tank/defensive.
// Priority 4 — "Is my skill worth casting?"  burst when it matters.
// Priority 5 — "Do I need a potion?"  HP below 35% and potion available.
// Default — attack (the idle baseline: the OC keeps fighting).
//
// Every step reads the ACTUAL combat math, not HP-percentage thresholds.
void dungeon_autopilot_battle(dungeon_save_t *s, uint64_t now_ms)
{
    if (!s->autopilot || s->n_enemy == 0) return;

    // ---- read the field ----
    uint16_t my_pow = dungeon_pow(s);
    uint16_t my_ehp = s->hp_cur + s->shield_pool;
    // v12.2: read REAL next-round damage (blob + stance chain), not raw atk
    // sums — the raw read overestimated deep-floor hits ~5x and smoked at
    // shadows (user: full-HP smoke / last-foe smoke).
    uint16_t incoming = enemy_threat_est(s, ST_ATK);   // as if pressing the attack
    uint16_t enemy_hp = 0;     // total alive enemy HP
    for (uint8_t i = 0; i < s->n_enemy; i++) {
        if (!s->enemy_hp[i]) continue;
        enemy_hp += s->enemy_hp[i];
    }

    // ---- Priority 1: finishing blow — attack to end it now ----
    if (enemy_hp <= my_pow * 2) {
        // enemy total HP is low enough that 1-2 rounds finishes them
        // → no defensive action needed, just attack
    }
    // ---- Priority 2: about to die — emergency survival ----
    else if (my_ehp <= incoming) {
        // next round WILL kill me: use emergency resources
        if (s->cons[CONS_SMOKE]) { dungeon_use_cons(s, CONS_SMOKE); return; }
        if (s->cons[CONS_POTION]) { dungeon_use_cons(s, CONS_POTION); return; }
        // nothing available: fight to the last breath
    }
    // ---- Priority 3: losing the race — change tactics ----
    else if (my_ehp < incoming * 3) {
        // surviving <3 rounds: use a potion or guard to stabilise
        if (s->cons[CONS_POTION]) dungeon_use_cons(s, CONS_POTION);
    }

    // ---- Priority 4: skill burst (faster kill = less damage taken) ----
    if (dungeon_skill_unlocked(s, (dungeon_job_t)s->main_job, 0) && !s->skill_cd[0])
        dungeon_cast(s, 0, now_ms);
    else if (dungeon_skill_unlocked(s, (dungeon_job_t)s->sub_job, 0) && !s->skill_cd[2])
        dungeon_cast(s, 2, now_ms);
}

// Forge the weaker piece first while keeping road money (inn/feather).
// v12.1: actually alternates — the old code hardcoded param 0, so armor sat
// at drop tier forever (econ diag: wpn 60 / arm 46 after 60 runs).
static int auto_forge(dungeon_save_t *s, int max_buys)
{
    int bought = 0;
    while (bought < max_buys && s->gold >= 110) {
        uint8_t p = (s->weapon_pow <= s->armor_def) ? 0 : 1;
        if (!dungeon_town(s, TOWN_UPGRADE, p) && !dungeon_town(s, TOWN_UPGRADE, (uint8_t)(p ^ 1)))
            break;
        bought++;
    }
    return bought;
}

// Keep a survival reserve: 3 smoke / 1 potion at floor-scaled peddler prices.
// Chest restocks still top up to 5/3 for free; this only fires below reserve.
static int auto_resupply(dungeon_save_t *s)
{
    // v25 automation tier 1 (prst 2): earned, not given — the early game
    // teaches the peddler by hand (ADimensions-style automation ladder).
    if (s->prestige < 2) return 0;
    int bought = 0;
    while (s->cons[CONS_SMOKE] < 3 && dungeon_town(s, TOWN_UPGRADE, 2)) bought++;
    while (s->cons[CONS_POTION] < 1 && dungeon_town(s, TOWN_UPGRADE, 3)) bought++;
    return bought;
}

bool dungeon_autopilot_explore(dungeon_save_t *s, uint64_t now_ms)
{
    (void)now_ms;
    if (!s->autopilot || !s->exploring || s->n_enemy || s->choice_pending) return false;
    if (s->energy < 12 && s->cons[CONS_FEATHER]) {
        dungeon_use_cons(s, CONS_FEATHER);   // exploring=0: the UI shows town
        return true;
    }
    if (s->step_count % DUNGEON_CAMP_EVERY == 0) {
        // Camp: a rest and a passing peddler, so long runs never stall on a
        // dry energy bar the way the review's bot dumps showed.
        if (s->energy < 60 && (perk_inn_free(s) || s->gold >= 10)) {
            if (!perk_inn_free(s)) s->gold = (uint16_t)(s->gold - 10);
            s->max_energy = energy_cap_of(s);
            s->energy = s->max_energy;
            log_push(s, perk_inn_free(s) ? "Camp: rested" : "Camp: rested (10g)");
        }
        if (auto_forge(s, 6)) log_push(s, "Peddler: gear forged");
        auto_resupply(s);
    }
    return false;
}

int dungeon_autopilot_town(dungeon_save_t *s)
{
    if (!s->autopilot) return 0;
    int n = 0;
    if (s->gold >= 10 && (s->energy < s->max_energy || s->hp_cur < max_hp_of(s)))
        n += dungeon_town(s, TOWN_INN, 0);
    n += auto_forge(s, 10);
    n += auto_resupply(s);
    for (int i = 0; i < 3; i++) if (!dungeon_town(s, TOWN_SYNTH, 0)) break; else n++;
    for (int i = 0; i < 3; i++) if (!dungeon_town(s, TOWN_SYNTH, 1)) break; else n++;
    // rebirth whenever the escalating cost allows: early and often while
    // cheap, then the curve forces deeper mazes.
    n += dungeon_town(s, TOWN_PRESTIGE, 0);
    static const uint8_t ORDER[SN_COUNT] = {SN_FOR, SN_VIT, SN_DEF, SN_STAM, SN_LUCK, SN_PACK};
    int bought = 1;
    while (bought) {
        bought = 0;
        for (uint8_t k = 0; k < SN_COUNT; k++)
            if (dungeon_sanctum_buy(s, (dungeon_sanctum_t)ORDER[k])) { bought = 1; n++; break; }
    }
    if (n && s->sanctum[SN_FOR]) {
        char buf[DUNGEON_LOG_LEN];
        snprintf(buf, sizeof(buf), "Sanctum F%u V%u D%u", s->sanctum[SN_FOR],
                 s->sanctum[SN_VIT], s->sanctum[SN_DEF]);
        log_push(s, buf);
    }
    // v25 automation tier 2 (prst 3): the well automates only after the
    // player has proven the economy loop by hand.
    if (s->prestige < 3) return n;
    while (s->essence >= TUN_WISH_COST + 20)
        if (!dungeon_town(s, TOWN_WISH, 0)) break; else n++;
    return n;
}
