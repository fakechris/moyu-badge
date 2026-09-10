// main/dungeon_model.h —— GAMEPLAY v4 infinite-maze crawler core.
// Pure logic, seeded RNG, NVS-serializable (<2KB). No ESP-IDF/LVGL.
// Design source: docs/GAMEPLAY_V4.md (v4 branch). API is additive over v3:
// demo_deskpet/UI keeps compiling untouched.
// Time is monotonic ms from caller. Two clocks: explore steps (4s) when no
// battle/pending choice; battle rounds (0.8s) while enemies live.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DUNGEON_MODEL_VERSION 13u
#define DUNGEON_MAX_FLOOR 100u
#define DUNGEON_BOSS_EVERY 5u
#define DUNGEON_EXPLORE_MS 4000u
#define DUNGEON_ROUND_MS 800u
#define DUNGEON_ENERGY_MAX 100u
#define DUNGEON_ENERGY_DRAIN_SEC 20u
#define DUNGEON_OFFLINE_CAP_SEC 1200u
#define DUNGEON_JOB_COUNT 5u
#define DUNGEON_JOB_LV_CAP 10u
#define DUNGEON_MAX_ENEMIES 3u
#define DUNGEON_CONS_SLOTS 3u   // potion / smoke / feather
#define DUNGEON_CHOICE_OPTS 3u
#define DUNGEON_CHOICE_TIMEOUT_MS 12000u
#define DUNGEON_CHOICE_AUTO_MS 3000u    // autopilot picks after this beat
#define DUNGEON_CAMP_EVERY 25u          // autopilot camp (rest/peddler) cadence
#define DUNGEON_SWITCH_CD_MS 2000u
#define DUNGEON_LOG_LINES 5u
#define DUNGEON_LOG_LEN 40u
#define DUNGEON_GEAR_TIER_CAP 60u
#define DUNGEON_RAR_MAX 4u  // 4 = ancient relic (ultra-rare deep drop)
#define DUNGEON_SANCTUM_SLOTS 6u

typedef enum {
    JOB_KNIGHT = 0,   // charge nearest, opener shield, gate expert
    JOB_BLACK,        // snipe lowest HP
    JOB_WHITE,        // heal weakest (self in MVP), weak poke
    JOB_THIEF,        // backstab lowest + loot sense
    JOB_DARK,         // drain highest HP, execute <30%
} dungeon_job_t;

typedef enum {
    SK_NONE = 0,
    // knight
    SK_SHIELD_BASH, SK_TAUNT,
    // black
    SK_FIRE, SK_FROST,
    // white
    SK_CURE, SK_AEGIS,
    // thief
    SK_VENOM, SK_SMOKEOUT,
    // dark
    SK_DRAIN, SK_DOOM,
    // evolved ultimates (v5.1: job lv10 + matching affix, once per battle)
    SK_CASTLE, SK_METEOR, SK_SANCTUARY, SK_JACKPOT, SK_ECLIPSE,
    SK_COUNT,
} dungeon_skill_t;

typedef enum {
    AI_CHARGE = 0, AI_SNIPER, AI_MEDIC, AI_BACKSTAB, AI_EXECUTE,
} dungeon_ai_t;

// V2 stances: per-round offensive/defensive posture (see playbook docs/COMBAT_V2.md).
// Transient: recomputed every round, never saved (DUNGEON_MODEL_VERSION untouched).
typedef enum {
    ST_ATK = 0,   // press: dealt x1.3, taken x1.0
    ST_GRD,       // brace: dealt x0.4, taken x0.9 (+0 shield), heavy x0.4
    ST_EVA,       // slip: dealt 0, 30% dodge table, skill_cd ticks once extra
} dungeon_stance_t;

// V2 tuning knobs (E-series, sim-green 2026-09-06). #ifndef-kept so autotune /
// -D overrides keep working exactly like the .c TUNs. Tests include this
// header, so unit tests always run against the tuned defaults.
#ifndef TUN_ST_ATK_DEALT     // ATK basic dealt (xN/10)
#define TUN_ST_ATK_DEALT 13   // spiky RPS: pressing must end battles fast (C band)
#endif
#ifndef TUN_ST_GRD_DEALT     // GRD basic dealt (xN/10)
#define TUN_ST_GRD_DEALT 4
#endif
#ifndef TUN_ST_GRD_TAKEN     // GRD taken (xN/10): the heavy specialist rate
#define TUN_ST_GRD_TAKEN 9
#endif
#ifndef TUN_ST_GRD_HEAVY     // GRD vs heavy action taken (xN/10)
#define TUN_ST_GRD_HEAVY 4
#endif
#ifndef TUN_ST_GRD_SHIELD    // GRD shield gained per round
#define TUN_ST_GRD_SHIELD 0   // ANY income snowballs (fresh 90% deep); sim-only re-tune
#endif
#ifndef TUN_ST_GRD_SHIELD_CAP // shield_pool ceiling for the GRD income
#define TUN_ST_GRD_SHIELD_CAP 12  // guardrail (opener/Aegis set directly, unaffected)
#endif
#ifndef TUN_EVADE_DODGE      // EVA % dodge vs basic actions
#define TUN_EVADE_DODGE 30    // EVA keeps heavy-slip + CD tick
#endif
#ifndef TUN_FURY_MULT        // war-cry: enemy atk while fury>0 (xN/10)
#define TUN_FURY_MULT 13
#endif
#ifndef TUN_DRAIN_PCT        // leech action: % of dealt self-healed
#define TUN_DRAIN_PCT 50
#endif
#ifndef TUN_MARATHON_ROUNDS  // GRD kill slower than this -> race instead
#define TUN_MARATHON_ROUNDS 10  // pacing guard (sim G5b: replays get faster)
#endif
#ifndef TUN_RACE_ROUNDS      // soft-enrage (MMO): past this round, race
#define TUN_RACE_ROUNDS 100   // (pacing backstop: no turtle may outlast the cap)
#endif

// ---- V2 playback: beat report + streaming剪枝机 ----------------------------
// Pure-auto fast battles only: any manual touch exits to 1x (see playbook
// docs/COMBAT_V2.md §11). Beats are transient (caller-owned or NULL);
// NOTHING persists (save format untouched, no version bump).
#define BBEAT_HEAVY 0x01  // enemy heavy rolled this round
#define BBEAT_BLOCK 0x02  // perfect block landed
#define BBEAT_DODGE 0x04  // a dodge landed
#define BBEAT_SKILL 0x08  // in-round offensive skill fired
#define BBEAT_ULT   0x10  // evolved ultimate fired
#define BBEAT_HURT  0x20  // player took damage
#define BBEAT_MERGED 0x80 // pruner digest mark (model never sets this)
typedef struct {
    uint8_t stance;     // ST_* resolved this round
    uint8_t flags;      // BBEAT_*
    uint8_t foes;       // kill mask, bit/foe
    uint8_t span;       // rounds folded (1 = single; digest >1)
    uint16_t dealt;     // player damage this round
    uint16_t taken;     // player damage taken this round
} battle_beat_t;   // 10B, transient

// Streaming剪枝机: per-beat show/merge decisions with a wall-clock budget.
// Pure state machine (no I/O): unit-tested, sim-driven, UI-agnostic.
typedef enum { BPLAY_SHOW, BPLAY_MERGE } bplay_act_t;
typedef struct {
    uint32_t budget_ms;     // playback budget (default TUN below)
    uint32_t spent_ms;      // playback time consumed
    battle_beat_t digest;   // pending merged digest (span>0 iff pending)
} beat_player_t;
void beat_player_init(beat_player_t *p, uint32_t budget_ms);
bplay_act_t beat_player_push(beat_player_t *p, const battle_beat_t *b);
int beat_player_has_digest(const beat_player_t *p);   // nonzero iff pending
void beat_player_take_digest(beat_player_t *p, battle_beat_t *out);  // emits+accounts
uint32_t beat_player_total(const beat_player_t *p);   // spent + pending + outcome
#ifndef TUN_BPLAY_BUDGET_MS   // fast-playback wall-clock budget
#define TUN_BPLAY_BUDGET_MS 10000
#endif
#ifndef TUN_BPLAY_HL_MS       // cost of one highlight beat
#define TUN_BPLAY_HL_MS 600
#endif
#ifndef TUN_BPLAY_DIGEST_MS   // cost of one digest beat ("鏖战xN")
#define TUN_BPLAY_DIGEST_MS 800
#endif
#ifndef TUN_BPLAY_OUTCOME_MS  // reserved outcome banner (clear/dead/flee)
#define TUN_BPLAY_OUTCOME_MS 2500
#endif

typedef struct {
    dungeon_ai_t ai;
    dungeon_skill_t active[2];  // unlocked at job lv 2 and lv 5
    uint8_t unlock_floor;       // entering this floor unlocks
    int8_t hp_grow, pow_grow, guard_grow;  // per job lv (main +2 / sub +1 applied by caller picking fields)
    // FFT-style M multipliers (GAMEPLAY_V5.md 2.1): switching jobs re-weights
    // the SAME raw stats now; job_lv/experience is the C (permanent) track.
    uint8_t m_pow10, m_hp10, m_grd10;   // v12: guard joins the M axes — the
    // tank identity must be visible in the defense stat, not just HP
    // VS-style evolution (v5.1): job at cap + matching gear affix unlocks the
    // ultimate (auto-cast once per battle).
    uint8_t ult_affix, ultimate;
} dungeon_job_def_t;

typedef enum {
    DEV_NONE = 0,
    DEV_BATTLE, DEV_TREASURE, DEV_TRAP, DEV_REST, DEV_BOSS, DEV_STAIR,
    DEV_CHOICE, DEV_DEAD, DEV_CLEAR,
} dungeon_event_t;

// Item affixes, packed 2x4bit per item. rar>=1 grants rar of them.
typedef enum {
    AF_NONE = 0,
    AF_LEECH,    // kill heals 8% max HP
    AF_GUARD,    // battle start: shield +4+2*rar (armor-flavored)
    AF_SWIFT,    // 12% dodge per stacked affix
    AF_VENOM,    // basic attacks apply 2-round dot
    AF_SLAYER,   // +30% damage vs boss
    AF_GREED,    // +30% gold
    AF_COUNT,
} dungeon_affix_t;

// Elite modifiers: one per elite enemy, rolled at spawn (v4.4). Deep floors
// only; elites are richer (extra essence/shard) and asymmetric.
typedef enum {
    EA_NONE = 0,
    EA_ARMORED,   // player damage to it x2/3
    EA_VENOMOUS,  // its attacks +50%
    EA_SWIFT,     // 20% player attacks miss it
    EA_THORNS,    // reflects atk/2 when player basic-attacks it
    EA_GIANT,     // +50% HP at spawn
    EA_COUNT,
} dungeon_elite_t;

// Sanctum meta-upgrades: star-dust sink, monotonic (never lost on death).
typedef enum {
    SN_VIT = 0,   // +5% max HP per lv
    SN_FOR,       // +3% pow per lv
    SN_DEF,       // +1 guard per lv
    SN_LUCK,      // +3% rarity roll per lv
    SN_STAM,      // +8 max energy per lv
    SN_PACK,      // start run with lv/2 potions, lv/3+1 feathers
    SN_COUNT,
} dungeon_sanctum_t;

typedef enum {
    CONS_POTION = 0, CONS_SMOKE, CONS_FEATHER,
} dungeon_cons_t;

typedef enum {
    // 9-blessing pool, offer 3 distinct rotating (count kept odd for the
    // start+i*2 distinctness trick).
    CH_AFFIX = 0, CH_BLESS_ATK, CH_BLESS_LEARN, CH_BLESS_DEF,
    CH_TRADE_WOUND, CH_TRADE_FEATHER, CH_SHRINE, CH_SHARDS,
    CH_CHALLENGE,   // v5.2: cursed door — modifier fight for rich bounty
    CH_COUNT,
} dungeon_choice_t;

typedef enum {
    TOWN_SWAP = 0,   // param: packed pair (main<<4|sub)
    TOWN_UPGRADE,    // param: 0 weapon / 1 armor (tier +1, gold)
    TOWN_INN,        // restore energy/HP (10g)
    TOWN_SYNTH,      // param: 0 weapon / 1 armor (rarity +1, shards+gold)
    TOWN_SANCTUM,    // param: dungeon_sanctum_t track (essence)
    TOWN_AUTOTOG,    // toggle auto_job
    TOWN_PRESTIGE,   // rebirth: bank ng badges into prestige (+2%/lv), maze resets
    TOWN_WISH,       // v5.2 gacha: essence -> random bounty, pity guarantees ancients
} dungeon_town_t;

typedef struct {
    uint16_t version;
    uint8_t main_job, sub_job;       // equipped pair (auto_job may move main beyond pair)
    uint8_t job_lv[DUNGEON_JOB_COUNT];
    uint16_t job_exp[DUNGEON_JOB_COUNT];
    uint8_t unlocked;                // bitmask (5 jobs)
    uint16_t explore_lv, explore_exp;
    uint8_t floor;                   // 1..100
    uint8_t ng;                      // maze badge (cleared 100 -> ng+1, back to F1)
    uint8_t energy, max_energy;
    uint16_t hp_cur;                 // current HP (max derived)
    uint16_t gold;
    uint16_t jobpt;                  // current-main-job points (display)
    uint8_t weapon_pow, weapon_affix;  // tier (1..40) + packed 2x4bit affixes
    uint8_t armor_def, armor_affix;
    uint8_t weapon_rar, armor_rar;   // 0..3
    uint16_t essence;                // meta currency, never lost on death
    uint8_t shards;                  // synthesis material
    uint8_t sanctum[DUNGEON_SANCTUM_SLOTS];
    uint8_t auto_job;                // autonomous job-change enabled
    uint8_t autopilot;               // v10: OC plays itself (items/skills/town/cards)
    uint8_t prestige;                // rebirth level (+2% pow & HP each, cap 50)
    uint8_t cons[DUNGEON_CONS_SLOTS];
    uint8_t top_floor;
    uint64_t codex;                  // seen bits: 0-15 legacy (jobs/foes/bosses),
                                     // 16+ = per-maze firsts (v13): 4 lanes x 12 mazes
    uint16_t kills, chests, deaths;
    // battle state
    uint8_t n_enemy;
    uint16_t enemy_hp[DUNGEON_MAX_ENEMIES];
    uint8_t enemy_kind[DUNGEON_MAX_ENEMIES];  // 0 beast 1 flier 2 undead 3 boss
    uint8_t enemy_affix[DUNGEON_MAX_ENEMIES]; // dungeon_elite_t per enemy
    uint8_t enemy_slow, enemy_weak;  // debuff rounds left
    uint8_t enemy_dot;               // venom rounds left (applied to all)
    uint8_t shield_pool;             // aegis / opener absorb
    uint8_t skill_cd[3];             // rounds left: [main0, main1, sub0]
    uint8_t must_bright;             // gatekeeper: needs bright screen
    uint8_t mimic_room;              // treasure-mimic fight: double loot on clear
    uint8_t ult_used;                // evolved ultimate spent this battle
    uint8_t rare_room;               // rare-foe room: richer loot
    uint8_t hidden_next;             // hidden boss waits at the next step
    uint8_t hidden_room;             // hidden boss fight in progress
    uint8_t wish_pity;               // v5.2 wishes since last ancient (guarantee)
    uint8_t challenge_next;          // cursed door waits at the next step
    uint8_t challenge_room;          // cursed-door fight in progress (rich bounty)
    uint8_t player_curse;            // rounds of -20% pow (undead signature)
    uint8_t enemy_fury;              // V2 war-cry buff: enemy atk +30% while >0
                                     // (rounds left; reuses the retired charge_mask byte:
                                     // same offset/size, no save migration)
    uint8_t enrage;                  // gatekeeper below 1/3 HP: atk +30%
    uint8_t starfall_next;           // star-fall chamber waits at the next step
    uint8_t starfalls;               // lifetime starfalls (collection stat)
    uint8_t floor_affix;             // 0 none 1 dark 2 wet 3 thorn 4 chests
    uint8_t run_mut;                 // v14 per-life mutator: 0 off, 1..6 pair id
    uint8_t mut_bounty;              // v15 mutant gate bounties collected (sat 255)
    uint8_t bless_atk, bless_def, bless_learn;  // this-run buffs
    uint8_t wound;                   // traded injury flag this run
    uint8_t step_count;              // explore steps (energy drain clock)
    uint8_t round_ct;                // battle rounds (slow parity, anim)
    uint8_t exploring;               // 1 = auto-explore on (pocket mode)
    uint64_t switch_ready_ms;        // job-switch CD (manual AND auto)
    // pending choice card
    uint8_t choice_pending;
    uint8_t choice_opts[DUNGEON_CHOICE_OPTS];
    uint64_t choice_deadline_ms;
    uint8_t choice_cursor;           // UI-owned but saved for timeout determinism
    uint32_t rng_state;
    char log[DUNGEON_LOG_LINES][DUNGEON_LOG_LEN];  // EN only for MVP
} dungeon_save_t;                    // ~330B < 2KB

const dungeon_job_def_t *dungeon_job_def(dungeon_job_t job);
const char *dungeon_job_name_en(dungeon_job_t job);
const char *dungeon_skill_name_en(dungeon_skill_t sk);
uint8_t dungeon_skill_unlocked(const dungeon_save_t *s, dungeon_job_t job, uint8_t slot);  // slot 0/1

void dungeon_new_game(dungeon_save_t *s, uint32_t seed);
// UI helpers (derived stats).
uint16_t dungeon_max_hp(const dungeon_save_t *s);
uint16_t dungeon_pow(const dungeon_save_t *s);
uint16_t dungeon_guard(const dungeon_save_t *s);
// Item helpers: effective value (tier * (10+5*rar)/10) and packed affix test.
uint16_t dungeon_item_eff(uint8_t tier, uint8_t rar);
bool dungeon_item_has_affix(uint8_t packed, dungeon_affix_t af);
uint8_t dungeon_sanctum_cost(uint8_t lv);           // essence cost of next lv
bool dungeon_sanctum_buy(dungeon_save_t *s, dungeon_sanctum_t tr);
// Explore step (call every EXPLORE_MS when no battle and no pending choice).
dungeon_event_t dungeon_explore(dungeon_save_t *s, uint64_t now_ms, uint64_t *next_due_ms);
// Combat round (call every ROUND_MS while n_enemy > 0).
dungeon_event_t dungeon_round(dungeon_save_t *s, uint64_t now_ms);
// Same, with per-round beat capture for V2 playback (beat NULL = same as above).
dungeon_event_t dungeon_round_ex(dungeon_save_t *s, uint64_t now_ms, battle_beat_t *beat);
// Manual skill: slot 0/1 = main actives, 2 = sub first active. Returns damage or 0.
int dungeon_cast(dungeon_save_t *s, uint8_t slot, uint64_t now_ms);
// Instant main switch between equipped pair (2s CD). Returns false on CD.
bool dungeon_switch(dungeon_save_t *s, uint8_t which, uint64_t now_ms);  // which 0=A(main) 1=B(sub->main, swap pair)
// Choice card: offer fills opts (call once when DEV_CHOICE), pick applies, timeout random-picks.
void dungeon_choice_offer(dungeon_save_t *s, uint64_t now_ms);
void dungeon_choice_pick(dungeon_save_t *s, uint8_t idx);
void dungeon_choice_timeout(dungeon_save_t *s, uint64_t now_ms);
// Town: swap pair / upgrade gear / inn / synth / sanctum / auto toggle. Returns false if short.
bool dungeon_town(dungeon_save_t *s, dungeon_town_t act, uint8_t param);
// Re-anchor boot-relative deadlines after a reboot / save load.
void dungeon_rebase_clock(dungeon_save_t *s, uint64_t now_ms);
// Offline settlement, secs uncapped (caps inside). Writes report line to log.
void dungeon_offline(dungeon_save_t *s, uint32_t secs, uint64_t now_ms);
// Consume a consumable. Returns false if none.
bool dungeon_use_cons(dungeon_save_t *s, dungeon_cons_t kind);
// Evolved ultimate available this battle (job at cap + matching affix, unused).
uint8_t dungeon_ult_ready(const dungeon_save_t *s);

// ---- autopilot (v10) -------------------------------------------------------
// The OC plays itself; every hook is a no-op while s->autopilot == 0. The
// balance sim's greedy bot IS this policy, so what the sim proves is what
// the device does.
// Before each battle round: potion / smoke when dying, then a ready skill.
void dungeon_autopilot_battle(dungeon_save_t *s, uint64_t now_ms);
// Before each explore step: feather home when energy is gone (returns true,
// exploring=0 so the UI shows town), camp every DUNGEON_CAMP_EVERY steps.
bool dungeon_autopilot_explore(dungeon_save_t *s, uint64_t now_ms);
// In town (exploring=0): inn, forge, synth, rebirth, sanctum, wish. Returns
// number of purchases so the UI can show a "shopping" beat.
int dungeon_autopilot_town(dungeon_save_t *s);
// Greedy card index for the pending offer (ATK > DEF > LEARN > SHRINE > SHARDS,
// cursed door when healthy). Valid whether or not autopilot is on.
uint8_t dungeon_choice_auto(const dungeon_save_t *s);
// Safety margin: rounds-to-die minus rounds-to-win. >=0 = clears the room alive.
int16_t dungeon_combat_margin(const dungeon_save_t *s);
// Pure stance picker: threat 0 ATK / 1 home-job / 2 GRD / 3 EVA, with a
// no-resources-and-dying all-in ATK override. Reads s, writes nothing.
uint8_t dungeon_stance_pick(const dungeon_save_t *s, uint8_t threat);
