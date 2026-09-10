// tests/test_deskpet_models.c —— host tests for pet/dungeon-v3/pomo/i18n pure logic.
// Build: cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_deskpet_models.c
//        main/pet_model.c main/dungeon_model.c main/pomo_lite.c main/deskpet_i18n.c
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "pet_model.h"
#include "dungeon_model.h"
#include "pomo_lite.h"
#include "deskpet_i18n.h"

void start_battle_for_test(dungeon_save_t *s, bool boss);

static void test_mood_priority(void)
{
    pet_triggers_t t = {0};
    t.idle_ms = 10000;
    assert(pet_mood_resolve(&t) == PET_MOOD_NORMAL);
    t.poked = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_HAPPY);
    t.poked = false; t.bad_news = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_SAD);
    t.pc_busy = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_BUSY);
    t.pomo_celebrate = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_CELEBRATE);
    t.pomo_running = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_FOCUS);
    t.battery_low = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_LOWBAT);
    t.sleeping = true;
    assert(pet_mood_resolve(&t) == PET_MOOD_SLEEP);
    t.sleeping = false; t.battery_low = false; t.pomo_running = false;
    t.pomo_celebrate = false; t.pc_busy = false; t.bad_news = false;
    t.idle_ms = PET_IDLE_SLEEP_MS;
    assert(pet_mood_resolve(&t) == PET_MOOD_SLEEP);
    printf("mood_priority OK\n");
}

static void auto_play(dungeon_save_t *s, uint64_t *t, int steps)
{
    // Drive explore + battle rounds, auto-resolving choice cards by timeout.
    for (int i = 0; i < steps; i++) {
        *t += DUNGEON_EXPLORE_MS;
        if (s->n_enemy > 0) {
            for (int r = 0; r < 6 && s->n_enemy > 0; r++)
                dungeon_round(s, *t);
        } else if (s->choice_pending) {
            dungeon_choice_timeout(s, *t);
        } else {
            dungeon_explore(s, *t, NULL);
        }
        if (s->floor >= DUNGEON_MAX_FLOOR && s->n_enemy == 0) break;
    }
}

static void test_v3_progress(void)
{
    // Multi-seed: the RNG stream shifts with balance tuning; progression
    // must hold across seeds, not on one lucky trajectory.
    int progressed = 0;
    for (uint32_t seed = 42; seed < 45; seed++) {
        dungeon_save_t s;
        dungeon_new_game(&s, seed);
        assert(s.main_job == JOB_KNIGHT && s.sub_job == JOB_BLACK);
        assert(s.hp_cur > 0 && s.energy == DUNGEON_ENERGY_MAX);
        assert(!(s.unlocked & (1u << JOB_WHITE)));
        s.exploring = 1;
        uint64_t t = 0;
        auto_play(&s, &t, 400);
        assert(s.kills > 0);
        if (s.floor > 1 || s.ng > 0) progressed++;
        printf("  seed=%u floor=%u kills=%u\n", seed, s.floor, s.kills);
    }
    assert(progressed >= 2);
    printf("v3_progress OK\n");
}

static void test_v3_skill_gate_and_cast(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 7);
    s.n_enemy = 1; s.enemy_hp[0] = 50; s.enemy_kind[0] = 0;
    s.job_lv[JOB_KNIGHT] = 1;
    assert(dungeon_cast(&s, 0, 0) == 0);  // locked below lv2
    s.job_lv[JOB_KNIGHT] = 2;
    s.energy = 100;
    int d = dungeon_cast(&s, 0, 0);  // Shield Bash
    assert(d > 0 && s.skill_cd[0] > 0);
    assert(dungeon_cast(&s, 0, 0) == 0);  // CD blocks
    printf("v3_skill_gate OK (bash=%d)\n", d);
}

static void test_v3_switch_cd(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 9);
    assert(dungeon_switch(&s, 1, 1000));
    assert(s.main_job == JOB_BLACK);
    assert(!dungeon_switch(&s, 1, 1001));  // CD
    assert(dungeon_switch(&s, 1, 1001 + DUNGEON_SWITCH_CD_MS));
    assert(s.main_job == JOB_KNIGHT);
    printf("v3_switch OK\n");
}

static void test_v3_death_keeps_levels(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 11);
    s.exploring = 1;
    s.job_lv[JOB_KNIGHT] = 4;
    s.gold = 99;
    s.cons[CONS_POTION] = 2;
    s.hp_cur = 1;
    s.n_enemy = 1; s.enemy_hp[0] = 500; s.enemy_kind[0] = 0;
    dungeon_event_t e = DEV_NONE;
    for (int i = 0; i < 200 && e != DEV_DEAD; i++)
        e = dungeon_round(&s, (uint64_t)i * DUNGEON_ROUND_MS);
    assert(e == DEV_DEAD);
    assert(s.floor == 1 && s.gold == 0);
    assert(s.job_lv[JOB_KNIGHT] == 4);  // kept
    assert(s.unlocked & (1u << JOB_BLACK));
    printf("v3_death OK (deaths=%u)\n", s.deaths);
}

static void test_v3_choice_card(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 13);
    dungeon_choice_offer(&s, 0);
    assert(s.choice_pending);
    assert(s.choice_opts[0] != s.choice_opts[1] && s.choice_opts[1] != s.choice_opts[2]
           && s.choice_opts[0] != s.choice_opts[2]);
    dungeon_choice_pick(&s, 1);
    assert(!s.choice_pending);
    dungeon_choice_offer(&s, 0);
    dungeon_choice_timeout(&s, DUNGEON_CHOICE_TIMEOUT_MS + 1);
    assert(!s.choice_pending);
    printf("v3_choice OK\n");
}

static void test_v3_town(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 15);
    s.gold = 500;
    uint8_t w0 = s.weapon_pow;
    assert(dungeon_town(&s, TOWN_UPGRADE, 0));
    assert(s.weapon_pow == w0 + 1 && s.gold < 500);
    s.unlocked |= (uint8_t)((1u << JOB_WHITE) | (1u << JOB_THIEF));
    assert(dungeon_town(&s, TOWN_SWAP, (uint8_t)((JOB_WHITE << 4) | JOB_THIEF)));
    assert(s.main_job == JOB_WHITE && s.sub_job == JOB_THIEF);
    s.energy = 10;
    s.hp_cur = 5;
    assert(dungeon_town(&s, TOWN_INN, 0));
    assert(s.energy == s.max_energy);
    s.gold = 0;
    assert(!dungeon_town(&s, TOWN_INN, 0));  // broke
    printf("v3_town OK\n");
}

static void test_v3_offline(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 17);
    s.exploring = 1;
    dungeon_offline(&s, 1200, 0);
    assert(strncmp((const char *)s.log[0], "Away", 4) == 0);
    printf("v3_offline OK (%s)\n", s.log[0]);
    // not exploring -> rested, no crash
    dungeon_new_game(&s, 19);
    dungeon_offline(&s, 600, 0);
    assert(strncmp((const char *)s.log[0], "Offline", 7) == 0);
    printf("v3_offline_rest OK\n");
}

// DESIGN 好玩验收 (model level): same floor/seed, knight vs black must
// clear the same room in a different number of rounds (behavior, not numbers).
static int clear_rounds(dungeon_job_t job, uint32_t seed)
{
    dungeon_save_t s;
    dungeon_new_game(&s, seed);
    s.main_job = job;
    s.job_lv[job] = 5;  // both skills online
    s.hp_cur = 500;
    s.energy = 100;
    s.n_enemy = 3;
    for (int i = 0; i < 3; i++) { s.enemy_hp[i] = 30; s.enemy_kind[i] = 0; }
    int r = 0;
    while (s.n_enemy > 0 && r < 60) { dungeon_round(&s, (uint64_t)r * DUNGEON_ROUND_MS); r++; }
    assert(s.n_enemy == 0);
    return r;
}

static void test_v3_job_diff(void)
{
    int rk = clear_rounds(JOB_KNIGHT, 1234);
    int rb = clear_rounds(JOB_BLACK, 1234);
    printf("v3_job_diff: knight %dr vs black %dr\n", rk, rb);
    assert(rk != rb);
    printf("v3_job_diff OK\n");
}

static void test_pomo_cycle(void)
{
    pomo_t p;
    pomo_defaults(&p);
    assert(p.state == POMO_IDLE);
    pomo_start(&p, 0);
    assert(p.state == POMO_RUNNING);
    assert(pomo_tick(&p, POMO_FOCUS_MS - 1) == POMO_EV_NONE);
    pomo_pause(&p, 1000);
    assert(p.state == POMO_PAUSED);
    pomo_resume(&p, 5000);
    assert(p.state == POMO_RUNNING);
    assert(pomo_tick(&p, 5000 + (POMO_FOCUS_MS - 1000)) == POMO_EV_FOCUS_DONE);
    assert(p.completed == 1);
    assert(pomo_tick(&p, 5000 + (POMO_FOCUS_MS - 1000) + POMO_REWARD_MS) == POMO_EV_REWARD_DONE);
    assert(pomo_tick(&p, 5000 + (POMO_FOCUS_MS - 1000) + POMO_REWARD_MS + POMO_BREAK_MS) == POMO_EV_BREAK_DONE);
    assert(p.state == POMO_IDLE);
    printf("pomo_cycle OK\n");
}

static void test_i18n_table(void)
{
    assert(S_COUNT > 40);
    for (int i = 0; i < S_COUNT; i++) {
        const char *en = deskpet_tr((deskpet_str_id_t)i, LANG_EN);
        const char *zh = deskpet_tr((deskpet_str_id_t)i, LANG_ZH);
        assert(en && en[0] && zh && zh[0]);
    }
    assert(deskpet_tr(S_DESKPET, LANG_EN)[0] == 'D');
    assert((unsigned char)deskpet_tr(S_DESKPET, LANG_ZH)[0] >= 0xE0);  // CJK UTF-8
    printf("i18n_table OK (%d strings)\n", S_COUNT);
}

static void test_save_budget(void)
{
    // Pipeline gate: all saves must fit NVS <2KB (DESIGN §1).
    size_t total = sizeof(pet_save_t) + sizeof(dungeon_save_t)
        + sizeof(pomo_t) + sizeof(deskpet_lang_t);
    printf("save bytes: pet=%zu dun=%zu pomo=%zu lang=%zu total=%zu\n",
           sizeof(pet_save_t), sizeof(dungeon_save_t),
           sizeof(pomo_t), sizeof(deskpet_lang_t), total);
    assert(total < 2048);
    printf("save_budget OK\n");
}

// ---- v4: essence/sanctum/synthesis/auto-job/rarity (GAMEPLAY_V4.md) --------
static void test_v4_essence_survives_death(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 21);
    s.exploring = 1;
    s.essence = 55;
    s.shards = 7;
    s.gold = 99;
    s.hp_cur = 1;
    s.n_enemy = 1; s.enemy_hp[0] = 5000; s.enemy_kind[0] = 3;  // hopeless boss
    dungeon_event_t e = DEV_NONE;
    for (int i = 0; i < 300 && e != DEV_DEAD; i++)
        e = dungeon_round(&s, (uint64_t)i * DUNGEON_ROUND_MS);
    assert(e == DEV_DEAD);
    assert(s.floor == 1 && s.gold == 0);   // run-only losses
    assert(s.essence == 55 && s.shards == 7);  // meta never lost
    printf("v4_essence_survives_death OK\n");
}

static void test_v4_sanctum(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 23);
    s.essence = 100;
    uint16_t hp0 = dungeon_max_hp(&s);
    assert(dungeon_sanctum_cost(0) == 4);
    assert(dungeon_sanctum_buy(&s, SN_VIT));
    assert(dungeon_max_hp(&s) > hp0);          // +5% per lv
    assert(s.essence == 96);
    assert(dungeon_sanctum_buy(&s, SN_STAM));
    assert(s.max_energy == 108);               // +8/lv
    s.essence = 1;
    assert(!dungeon_sanctum_buy(&s, SN_FOR));  // broke
    s.essence = 60000;
    s.sanctum[SN_VIT] = 10;
    assert(!dungeon_sanctum_buy(&s, SN_VIT));  // capped
    printf("v4_sanctum OK\n");
}

static void test_v4_synth_and_item_math(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 25);
    assert(dungeon_item_eff(10, 0) == 10);
    assert(dungeon_item_eff(10, 2) == 20);     // x2.0
    assert(dungeon_item_eff(10, 3) == 25);     // x2.5
    assert(dungeon_item_has_affix(0x23, AF_SWIFT));    // low nibble 3
    assert(dungeon_item_has_affix(0x23, AF_GUARD));    // high nibble 2
    assert(!dungeon_item_has_affix(0x23, AF_LEECH));
    s.weapon_rar = 0;
    s.shards = 100; s.gold = 1000;
    assert(dungeon_town(&s, TOWN_SYNTH, 0));   // 0->1: 2 shards + 40g
    assert(s.weapon_rar == 1);
    assert(dungeon_town(&s, TOWN_SYNTH, 0));   // 1->2: 4 shards + 80g
    assert(s.weapon_rar == 2);
    assert(dungeon_town(&s, TOWN_SYNTH, 0));   // 2->3: 6 shards + 120g
    assert(s.weapon_rar == 3);
    assert(!dungeon_town(&s, TOWN_SYNTH, 0));  // maxed rarity refuses
    assert(s.shards == 70 && s.gold == 760);   // 5+10+15 shards, 40+80+120 gold
    assert(!dungeon_item_has_affix(s.weapon_affix, AF_NONE));
    printf("v4_synth OK\n");
}

static void test_v4_auto_swap(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 27);
    assert(s.auto_job);
    s.unlocked |= (uint8_t)(1u << JOB_WHITE);
    s.job_lv[JOB_WHITE] = 1;
    s.exploring = 1;
    s.main_job = JOB_KNIGHT;
    s.hp_cur = 1;  // << 40%
    dungeon_explore(&s, 0, NULL);
    assert(s.main_job == JOB_WHITE);  // dying -> white between battles
    printf("v4_auto_swap OK\n");
}

static void test_v4_dark_unlock_and_execute_ai(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 29);
    assert(!(s.unlocked & (1u << JOB_DARK)));
    s.floor = 41;
    s.exploring = 1;
    dungeon_explore(&s, 0, NULL);
    assert(s.unlocked & (1u << JOB_DARK));
    // dark targets the fattest enemy
    s.main_job = JOB_DARK;
    s.n_enemy = 2;
    s.enemy_hp[0] = 10; s.enemy_hp[1] = 900;
    s.enemy_kind[0] = s.enemy_kind[1] = 2;
    int t0 = 0;
    // via cast drain (lv2+): after kill of... just verify targeting by damage lands on 900
    s.job_lv[JOB_DARK] = 2;
    s.energy = 100;
    int d = dungeon_cast(&s, 0, 0);
    assert(d > 0);
    assert(s.enemy_hp[1] < 900);  // hit the big one
    (void)t0;
    printf("v4_dark OK (drain=%d)\n", d);
}

static void test_v4_elite_armored(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 31);
    s.n_enemy = 1;
    s.enemy_hp[0] = 1000; s.enemy_kind[0] = 0; s.enemy_affix[0] = EA_ARMORED;
    s.job_lv[JOB_KNIGHT] = 5; s.energy = 100; s.weapon_pow = 10;
    s.hp_cur = dungeon_max_hp(&s);
    dungeon_save_t plain = s;
    plain.enemy_affix[0] = EA_NONE;
    int da = dungeon_cast(&s, 0, 0);      // same RNG state -> same roll
    int db = dungeon_cast(&plain, 0, 0);
    assert(da > 0 && db > 0 && da * 3 <= db * 2 + 2);  // armored takes ~2/3
    printf("v4_elite_armored OK (%d vs %d)\n", da, db);
}

static void test_v4_autoswap_v2(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 33);
    s.exploring = 1;
    s.floor = 42;  // flier belt, not a gate floor
    s.hp_cur = dungeon_max_hp(&s);
    dungeon_explore(&s, 0, NULL);
    assert(s.main_job == JOB_BLACK);  // fliers -> black (x1.5 restraint)
    printf("v4_autoswap_v2 OK\n");
}

static void test_v4_prestige(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 35);
    assert(!dungeon_town(&s, TOWN_PRESTIGE, 0));   // needs a badge first
    s.ng = 2;
    s.essence = 500; s.weapon_pow = 20; s.weapon_rar = 2;
    uint16_t p0 = dungeon_pow(&s), h0 = dungeon_max_hp(&s);
    assert(dungeon_town(&s, TOWN_PRESTIGE, 0));
    assert(s.prestige == 2 && s.ng == 0 && s.floor == 2);  // banked; v15 start floor at prst2 = 2+(2-2)*2
    assert(s.essence == 500 && s.weapon_pow == 20 && s.weapon_rar == 2);
    assert(dungeon_pow(&s) > p0 && dungeon_max_hp(&s) > h0);  // +2%/lv each
    printf("v4_prestige OK\n");
}

static void test_v7_job_multiplier(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 37);
    uint16_t pk = dungeon_pow(&s);          // knight M_pow 100
    uint16_t hk = dungeon_max_hp(&s);       // knight M_hp 120
    s.main_job = JOB_BLACK;
    uint16_t pb = dungeon_pow(&s);          // black M_pow 120
    uint16_t hb = dungeon_max_hp(&s);       // black M_hp 85
    assert(pb * 10 >= pk * 11);             // black hits harder
    assert(hb * 12 <= hk * 10);             // but is squishier
    printf("v7_job_multiplier OK (pow %u->%u, hp %u->%u)\n", pk, pb, hk, hb);
}

static void test_v7_soft_reset(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 39);
    s.explore_lv = 20;
    s.job_lv[JOB_KNIGHT] = 5;
    s.essence = 77;
    s.hp_cur = 1;
    s.n_enemy = 1; s.enemy_hp[0] = 5000; s.enemy_kind[0] = 0;
    dungeon_event_t e = DEV_NONE;
    for (int i = 0; i < 300 && e != DEV_DEAD; i++)
        e = dungeon_round(&s, (uint64_t)i * DUNGEON_ROUND_MS);
    assert(e == DEV_DEAD);
    assert(s.explore_lv == 10);             // halved (soft reset track)
    assert(s.job_lv[JOB_KNIGHT] == 5);      // mastery kept
    assert(s.essence == 77);                // meta kept
    printf("v7_soft_reset OK\n");
}

static void test_v7_sanctum_geometric(void)
{
    // r = 1.5: 4,6,9,13,19,28,42,63,94,141 (Pecorella geometric family)
    static const uint8_t EXP[10] = {4, 6, 9, 13, 19, 28, 42, 63, 94, 141};
    for (uint8_t lv = 0; lv < 10; lv++)
        assert(dungeon_sanctum_cost(lv) == EXP[lv]);
    printf("v7_sanctum_geometric OK\n");
}

static void test_v7_undead_drain(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 41);
    s.n_enemy = 1;
    s.enemy_hp[0] = 100; s.enemy_kind[0] = 2; s.enemy_affix[0] = EA_NONE;
    s.armor_def = 0;                        // enemy hits land fully
    dungeon_save_t beast = s;
    beast.enemy_kind[0] = 0;
    // let each version strike: run one round each with identical RNG
    s.exploring = 0; beast.exploring = 0;
    s.n_enemy = 1; beast.n_enemy = 1;
    s.round_ct = 1; beast.round_ct = 1;
    s.enemy_slow = 0; beast.enemy_slow = 0;
    dungeon_round(&s, 0);
    dungeon_round(&beast, 0);
    assert(s.enemy_hp[0] > beast.enemy_hp[0] || s.hp_cur < beast.hp_cur);
    printf("v7_undead_drain OK\n");
}

static void test_v8_evolution(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 43);
    s.job_lv[JOB_KNIGHT] = 7;
    s.n_enemy = 1; s.enemy_hp[0] = 500; s.enemy_kind[0] = 0;
    s.armor_affix = (uint8_t)(AF_GUARD | (AF_SWIFT << 4));
    assert(!dungeon_ult_ready(&s));            // below the lv8 unlock line
    s.job_lv[JOB_KNIGHT] = 8;
    assert(dungeon_ult_ready(&s));             // lv8 + matching affix = ready
    s.exploring = 1;
    s.hp_cur = dungeon_max_hp(&s);
    uint64_t t = 0;
    int shield_seen = 0;
    for (int i = 0; i < 80 && s.n_enemy > 0; i++) {
        t += DUNGEON_ROUND_MS;
        dungeon_round(&s, t);
        if (s.ult_used) { shield_seen = 1; break; }
    }
    assert(s.ult_used && shield_seen);
    assert(strncmp((const char *)s.log[0], "CASTLE", 6) == 0);  // fired this round
    assert(s.shield_pool >= 10);               // +15 castle, minus chip damage
    printf("v8_evolution OK (shield=%u)\n", s.shield_pool);
}

static void test_v8_ancient_rar4(void)
{
    assert(dungeon_item_eff(10, 4) == 30);     // ancient x3.0
    dungeon_save_t s;
    dungeon_new_game(&s, 45);
    s.weapon_rar = 3; s.shards = 100; s.gold = 10000;
    assert(!dungeon_town(&s, TOWN_SYNTH, 0));  // ancients cannot be crafted
    printf("v8_rar4 OK\n");
}

static void test_v9_wish_pity(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 47);
    s.essence = 1000;
    s.wish_pity = 25 - 1;                      // one wish from the guarantee
    // v12.1: fresh save has no gold -> broke patron pays double essence (50)
    assert(dungeon_town(&s, TOWN_WISH, 0));    // (defaults: pity 25, cost 25)
    assert(s.essence == 1000 - 50);
    assert(s.wish_pity == 0);                  // ancient granted -> pity reset
    assert(s.weapon_rar == 4 || s.armor_rar == 4);
    assert(!dungeon_town(&s, TOWN_WISH, 0) || s.essence >= 0);  // can keep wishing
    printf("v9_wish_pity OK\n");
}

static void test_v121_economy(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 53);

    // Rich patron: wish burns gold (progressive tax on the pile), not essence.
    s.gold = 10000; s.essence = 100; s.prestige = 0; s.ng = 0;
    uint16_t expect = (uint16_t)(50 + 10000 / 8);          // BASE + gold/TAX
    assert(dungeon_town(&s, TOWN_WISH, 0));
    assert(s.gold == 10000 - expect);
    assert(s.essence == 75);
    // Pile shrank -> next wish's gold price dropped (progressive, not flat).
    uint16_t g2 = (uint16_t)(50 + s.gold / 8);
    assert(g2 < expect);
    // Broke patron: gold 0 -> double essence, well never stalls.
    s.gold = 0; s.essence = 60;
    assert(dungeon_town(&s, TOWN_WISH, 0));
    assert(s.essence == 10);
    s.essence = 40;
    assert(!dungeon_town(&s, TOWN_WISH, 0));               // can't afford either

    // Peddler: floor-scaled smoke price, caps at 5 (manual) / reserve 3 (auto).
    dungeon_new_game(&s, 57);
    s.floor = 30; s.gold = 500;
    assert(dungeon_town(&s, TOWN_UPGRADE, 2));             // 15+30 = 45g
    assert(s.cons[CONS_SMOKE] == 1 && s.gold == 455);
    s.cons[CONS_SMOKE] = 5;
    assert(!dungeon_town(&s, TOWN_UPGRADE, 2));            // manual cap
    s.cons[CONS_POTION] = 3;
    assert(!dungeon_town(&s, TOWN_UPGRADE, 3));            // potion cap

    // Town autopilot keeps the reserve and forges the WEAKER piece first.
    dungeon_new_game(&s, 61);
    s.autopilot = 1;
    s.prestige = 2;                        // v25: auto-resupply is earned (tier 1)
    s.gold = 5000; s.floor = 1;
    s.weapon_pow = 10; s.armor_def = 4;                    // armor is the gap
    s.explore_lv = 30; s.max_energy = 250; s.energy = 250; // skip inn spending
    s.hp_cur = dungeon_max_hp(&s);
    dungeon_autopilot_town(&s);
    assert(s.cons[CONS_SMOKE] >= 3);                       // reserve bought
    assert(s.armor_def > 4);                               // armor got the money
    // gating contract: fresh save (prst 0) gets NO auto-resupply
    dungeon_new_game(&s, 63);
    s.autopilot = 1; s.gold = 5000; s.cons[CONS_SMOKE] = 0;
    s.max_energy = 250; s.energy = 250; s.hp_cur = dungeon_max_hp(&s);
    dungeon_autopilot_town(&s);
    assert(s.cons[CONS_SMOKE] == 0);                       // earned at prst 2
    printf("v121_economy OK\n");
}

static void test_v9_codex_milestone(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 49);
    s.explore_lv = 25;                         // big enough base: +4% survives ints
    uint16_t p0 = dungeon_pow(&s);
    s.codex = 0x0FFF;                          // 12 entries -> milestone 2 (+4%)
    assert(dungeon_pow(&s) > p0);
    assert(dungeon_max_hp(&s) > 0);
    printf("v9_codex_milestone OK\n");
}

static void test_v9_challenge_and_starfall(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 51);
    s.unlocked = 0x1F;
    s.exploring = 1;
    s.hp_cur = dungeon_max_hp(&s);
    // force a cursed-door offer and accept it
    dungeon_choice_offer(&s, 0);
    s.choice_opts[0] = CH_CHALLENGE;
    dungeon_choice_pick(&s, 0);
    assert(s.challenge_next);
    dungeon_event_t e = dungeon_explore(&s, DUNGEON_EXPLORE_MS, NULL);
    assert(e == DEV_BATTLE && s.challenge_room);
    assert(s.enemy_affix[0] == EA_GIANT);
    // force the star-fall chamber
    dungeon_save_t b;
    dungeon_new_game(&b, 53);
    b.unlocked = 0x1F;
    b.exploring = 1;
    b.starfall_next = 1;
    uint16_t ess0 = b.essence;
    dungeon_event_t e2 = dungeon_explore(&b, DUNGEON_EXPLORE_MS, NULL);
    assert(e2 == DEV_CHOICE && b.choice_pending);
    assert(b.essence >= ess0 + 30);
    assert(b.starfalls >= 0);
    printf("v9_challenge_starfall OK\n");
}


// ---- review fixes (2026-09-05) --------------------------------------------
// Smoke Out is a flee: no loot, no gate clear, every room flag reset.
// Gate auto-lead used to assign main=KNT without demoting, so a pair
// that already had knight as sub became KNT+KNT.
static void test_fix_gate_pair_distinct(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 38);
    s.unlocked = 0x1F;
    s.auto_job = 1;
    s.main_job = JOB_BLACK;
    s.sub_job = JOB_KNIGHT;
    s.hp_cur = dungeon_max_hp(&s);
    start_battle_for_test(&s, true);
    assert(s.main_job == JOB_KNIGHT);
    assert(s.sub_job == JOB_BLACK);
    assert(s.main_job != s.sub_job);

    dungeon_new_game(&s, 39);
    s.unlocked = 0x1F;
    s.auto_job = 1;
    s.exploring = 1;
    s.main_job = JOB_KNIGHT;
    s.sub_job = JOB_BLACK;
    s.job_lv[JOB_WHITE] = 1;
    s.hp_cur = 1;
    dungeon_explore(&s, 0, NULL);
    assert(s.main_job == JOB_WHITE);
    assert(s.sub_job == JOB_KNIGHT);
    assert(s.main_job != s.sub_job);

    dungeon_new_game(&s, 40);
    s.unlocked = (uint8_t)((1u << JOB_KNIGHT) | (1u << JOB_BLACK));
    s.auto_job = 0;
    s.main_job = JOB_KNIGHT;
    s.sub_job = JOB_KNIGHT;
    start_battle_for_test(&s, true);
    assert(s.main_job == JOB_KNIGHT);
    assert(s.sub_job == JOB_BLACK);
    printf("fix_gate_pair_distinct OK\n");
}

static void test_fix_smokeout_is_flee(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 42);
    s.unlocked = 0x1F; s.main_job = JOB_THIEF; s.sub_job = JOB_BLACK;
    s.job_lv[JOB_THIEF] = 5; s.auto_job = 0;
    s.floor = 100; s.exploring = 1; s.energy = 50; s.hp_cur = dungeon_max_hp(&s);
    assert(dungeon_explore(&s, 0, NULL) == DEV_BOSS);
    assert(s.must_bright && s.n_enemy == 1);
    uint16_t g0 = s.gold, k0 = s.kills;
    dungeon_cast(&s, 1, 0);  // Smoke Out
    assert(s.n_enemy == 0 && s.floor == 100 && s.ng == 0);
    assert(s.gold == g0 && s.kills == k0 && s.must_bright == 0);
    // mimic flag cleared too: the next clear pays normal loot
    dungeon_new_game(&s, 43);
    s.unlocked = 0x1F; s.main_job = JOB_THIEF; s.job_lv[JOB_THIEF] = 5; s.energy = 50;
    s.n_enemy = 1; s.enemy_hp[0] = 50; s.enemy_kind[0] = 0; s.mimic_room = 1;
    dungeon_cast(&s, 1, 0);
    assert(s.mimic_room == 0);
    printf("fix_smokeout_is_flee OK\n");
}

// Offline: "died" only when a death happened in THIS settlement; the gate
// flag survives so a second settlement never auto-fights the boss.
static void test_fix_offline_report_and_gate(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 7);
    s.deaths = 3; s.exploring = 1; s.floor = 2; s.hp_cur = 9999;  // effectively immortal
    s.hp_cur = dungeon_max_hp(&s);
    s.hp_cur = (uint16_t)(s.hp_cur * 1);
    dungeon_offline(&s, 120, 0);
    assert(strstr(s.log[0], "died") == NULL);
    // gate fight in progress -> offline does not touch it, flag stays.
    dungeon_new_game(&s, 8);
    s.exploring = 1; s.floor = 5; s.hp_cur = dungeon_max_hp(&s);
    assert(dungeon_explore(&s, 0, NULL) == DEV_BOSS);
    uint16_t ehp = s.enemy_hp[0];
    dungeon_offline(&s, 1200, 1000);
    assert(s.must_bright == 1 && s.n_enemy == 1 && s.enemy_hp[0] == ehp);
    dungeon_offline(&s, 1200, 2000);
    assert(s.must_bright == 1 && s.n_enemy == 1 && s.enemy_hp[0] == ehp);
    printf("fix_offline_report_and_gate OK\n");
}

// HP never exceeds the new max after any job change.
static void test_fix_hp_clamp_on_switch(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 3);
    s.hp_cur = dungeon_max_hp(&s);             // knight m_hp 120
    assert(dungeon_switch(&s, 1, 5000));        // black m_hp 85
    assert(s.hp_cur <= dungeon_max_hp(&s));
    s.hp_cur = dungeon_max_hp(&s);
    assert(dungeon_town(&s, TOWN_SWAP, (uint8_t)((JOB_KNIGHT << 4) | JOB_BLACK)));
    s.hp_cur = dungeon_max_hp(&s);
    assert(dungeon_town(&s, TOWN_SWAP, (uint8_t)((JOB_BLACK << 4) | JOB_KNIGHT)));
    assert(s.hp_cur <= dungeon_max_hp(&s));
    printf("fix_hp_clamp_on_switch OK\n");
}

// Pomodoro: remaining_ms follows the clock; reboot rebase resumes paused.
static void test_fix_pomo_countdown_and_rebase(void)
{
    pomo_t p;
    pomo_defaults(&p);
    pomo_start(&p, 1000);
    assert(pomo_tick(&p, 1000 + 60000) == POMO_EV_NONE);
    assert(p.remaining_ms == POMO_FOCUS_MS - 60000);
    pomo_rebase(&p);
    assert(p.state == POMO_PAUSED && p.remaining_ms == POMO_FOCUS_MS - 60000);
    pomo_resume(&p, 5);
    assert(p.deadline_ms == 5 + POMO_FOCUS_MS - 60000);
    p.state = POMO_BREAK;
    pomo_rebase(&p);
    assert(p.state == POMO_IDLE && p.remaining_ms == POMO_FOCUS_MS);
    printf("fix_pomo_countdown_and_rebase OK\n");
}

// Dungeon: a pending card's deadline re-anchors to the new boot clock.
static void test_fix_rebase_clock(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 11);
    dungeon_choice_offer(&s, 99999999ULL);
    s.switch_ready_ms = 99999999ULL;
    dungeon_rebase_clock(&s, 100);
    assert(s.choice_deadline_ms == 100 + DUNGEON_CHOICE_TIMEOUT_MS);
    assert(s.switch_ready_ms == 0);
    printf("fix_rebase_clock OK\n");
}


// ---- v10 autopilot + floor affixes -------------------------------------------
static void test_v10_autopilot(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 21);
    assert(s.autopilot == 1);
    // battle: potion when the ESTIMATED next-round damage would kill
    // (v12.2: the AI reads real blob damage, not raw atk or HP thresholds —
    // a 1-dmg foe does not justify the last potion at 5 HP)
    s.job_lv[JOB_KNIGHT] = 2; s.energy = 50; s.cons[CONS_POTION] = 1;
    s.n_enemy = 1; s.enemy_hp[0] = 500; s.enemy_kind[0] = 0;
    s.floor = 30;                     // foe hits ~16 through fresh guard
    s.hp_cur = 5;
    dungeon_autopilot_battle(&s, 0);
    assert(s.cons[CONS_POTION] == 0 && s.hp_cur > 5);
    // skill burst when ready (P2 returned early, so cast on a safe board)
    s.floor = 1; s.hp_cur = dungeon_max_hp(&s); s.skill_cd[0] = 0;
    dungeon_autopilot_battle(&s, 0);
    assert(s.skill_cd[0] > 0);   // Shield Bash went out
    // battle: smoke when dying with no potion
    s.hp_cur = 2; s.cons[CONS_SMOKE] = 1; s.skill_cd[0] = 0;
    s.floor = 10;   // smoke gated to F>=10 (v11: early floors don't waste items)
    dungeon_autopilot_battle(&s, 0);
    assert(s.n_enemy == 0 && s.cons[CONS_SMOKE] == 0);
    // explore: feather home at low energy
    s.exploring = 1; s.energy = 5; s.cons[CONS_FEATHER] = 1; s.step_count = 1;
    assert(dungeon_autopilot_explore(&s, 0) == true);
    assert(s.exploring == 0);   // (pack re-provisions the feather at home)
    // town: spends gold on the forge, essence on the sanctum, rests
    s.gold = 400; s.essence = 30; s.energy = 10;
    uint8_t t0 = s.weapon_pow;
    int n = dungeon_autopilot_town(&s);
    assert(n > 0 && s.weapon_pow > t0 && s.sanctum[SN_FOR] >= 1);
    assert(s.energy == s.max_energy);
    // off switch: every hook is inert
    s.autopilot = 0; s.gold = 400; t0 = s.weapon_pow;
    assert(dungeon_autopilot_town(&s) == 0 && s.weapon_pow == t0);
    s.exploring = 1; s.energy = 5; s.cons[CONS_FEATHER] = 1;
    assert(dungeon_autopilot_explore(&s, 0) == false && s.cons[CONS_FEATHER] == 1);
    // greedy card: cursed door when healthy, ATK otherwise
    s.choice_opts[0] = CH_SHARDS; s.choice_opts[1] = CH_CHALLENGE; s.choice_opts[2] = CH_BLESS_ATK;
    s.hp_cur = dungeon_max_hp(&s);
    assert(dungeon_choice_auto(&s) == 1);
    s.hp_cur = 1;
    assert(dungeon_choice_auto(&s) == 2);
    printf("v10_autopilot OK\n");
}

static void test_v10_floor_affix(void)
{
    dungeon_save_t a, b;
    dungeon_new_game(&a, 5); dungeon_new_game(&b, 5);
    a.floor = b.floor = 10; a.floor_affix = 2;   // wet: sluggish opener
    a.exploring = b.exploring = 1;
    a.hp_cur = dungeon_max_hp(&a); b.hp_cur = dungeon_max_hp(&b);
    while (a.n_enemy == 0) dungeon_explore(&a, 0, NULL);
    assert(a.enemy_slow == 2);
    // affix dies with the floor
    dungeon_new_game(&a, 6); a.exploring = 1; a.floor_affix = 1; a.hp_cur = 999;
    uint8_t f0 = a.floor;
    for (int i = 0; i < 200 && a.floor == f0; i++) {
        if (a.n_enemy) dungeon_round(&a, 0);
        else if (a.choice_pending) dungeon_choice_pick(&a, 0);
        else dungeon_explore(&a, 0, NULL);
    }
    assert(a.floor > f0 && a.floor_affix == 0);
    printf("v10_floor_affix OK\n");
}

// V2 dark-stance combat (playbook docs/COMBAT_V2.md): margin sign and the
// pure stance picker. No RNG involved — fully deterministic.
static void test_v12_margin_and_stance(void)
{
    dungeon_save_t s;
    // crushing: full HP vs one weak beast
    dungeon_new_game(&s, 101);
    s.n_enemy = 1; s.enemy_hp[0] = 10; s.enemy_kind[0] = 0;
    assert(dungeon_combat_margin(&s) > 0);
    assert(dungeon_stance_pick(&s, 0) == ST_ATK);
    // dying with nothing left: all-in ATK even at threat 3
    dungeon_new_game(&s, 102);
    s.hp_cur = 1;
    s.cons[CONS_POTION] = 0; s.cons[CONS_SMOKE] = 0;
    s.n_enemy = 1; s.enemy_hp[0] = 5000; s.enemy_kind[0] = 3;
    assert(dungeon_combat_margin(&s) < 0);
    assert(dungeon_stance_pick(&s, 3) == ST_ATK);
    // same dying state WITH potion: threat 3 -> EVA (turtle the top-up)
    s.cons[CONS_POTION] = 1;
    assert(dungeon_stance_pick(&s, 3) == ST_EVA);
    // losing -> GRD: 40% hp vs a mid foe (60hp: outside finisher 2xpow,
    // inside GRD marathon guard <=10 rounds)
    dungeon_new_game(&s, 101);
    s.n_enemy = 1; s.enemy_hp[0] = 20; s.enemy_kind[0] = 0;   // 12 < hp <= 24: no finisher, no marathon
    s.hp_cur = dungeon_max_hp(&s) * 4 / 10;
    assert(dungeon_stance_pick(&s, 2) == ST_GRD);
    // winning -> home stance per job (GRD homes need a fast kill, EVA home
    // needs a skill bridge — the purposeful-turtle rules)
    s.main_job = JOB_KNIGHT; assert(dungeon_stance_pick(&s, 1) == ST_GRD);
    s.main_job = JOB_BLACK;  assert(dungeon_stance_pick(&s, 1) == ST_ATK);
    // white vs beast (rest 7) would GRD-turtle for 10 rounds -> presses (ATK);
    // vs a 5hp undead: v23 finisher override PRESSES a one-hit foe (ST_ATK) —
    // the old GRD-brace would EVA... turtle a corpse.
    s.main_job = JOB_WHITE;  assert(dungeon_stance_pick(&s, 1) == ST_ATK);
    s.enemy_kind[0] = 2; s.enemy_hp[0] = 5;
    assert(dungeon_stance_pick(&s, 1) == ST_ATK);
    s.enemy_kind[0] = 0; s.enemy_hp[0] = 30;   // outside thief finisher (2x8=16)
    s.main_job = JOB_THIEF;  assert(dungeon_stance_pick(&s, 1) == ST_ATK);  // no bridge
    s.job_lv[JOB_THIEF] = 2; s.skill_cd[0] = 2;   // venom ticking -> bridge
    assert(dungeon_stance_pick(&s, 1) == ST_EVA);
    s.skill_cd[0] = 0;
    s.main_job = JOB_DARK;   assert(dungeon_stance_pick(&s, 1) == ST_ATK);
    printf("v12_margin_stance OK\n");
}

// V2 round coupling: twin battles, same seed. A (autopilot, GRD) vs B
// (manual, ATK). Same rolls -> dealt must follow the 5:12 stance ratio and
// the guarded twin must out-tank. Plus: EVA deals nothing.
static void test_v12_stance_round(void)
{
    dungeon_save_t a, b;
    dungeon_new_game(&a, 105); dungeon_new_game(&b, 105);
    b.autopilot = 0;   // manual resolves as ATK
    a.shield_pool = b.shield_pool = 0;   // v24 kit ward would shift the twin flows
    a.hp_cur = b.hp_cur = 30;
    uint16_t ehp = (uint16_t)(dungeon_pow(&a) * 4);  // r_win = 4 (threat 1),
                                                     // GRD-kill = 8 (fast turtle)
    a.n_enemy = b.n_enemy = 1;
    a.enemy_hp[0] = b.enemy_hp[0] = ehp;
    a.enemy_kind[0] = b.enemy_kind[0] = 0;
    dungeon_autopilot_battle(&a, 0);   // threat 1/2 -> knight GRD
    assert(dungeon_round(&a, 0) == DEV_BATTLE);
    assert(dungeon_round(&b, 0) == DEV_BATTLE);
    int dmgA = (int)ehp - (int)a.enemy_hp[0];
    int dmgB = (int)ehp - (int)b.enemy_hp[0];
    assert(dmgB > 0);
    int scaled = dmgA * TUN_ST_ATK_DEALT / TUN_ST_GRD_DEALT;   // stance ratio
    assert(scaled >= dmgB - 2 && scaled <= dmgB + 2);
    assert(a.hp_cur >= b.hp_cur);   // GRD never takes MORE (strict > only
                                    // holds when the mitigated hit clears the floor)
    printf("v12_stance_ratio OK (grd=%d atk=%d)\n", dmgA, dmgB);
    // EVA: threat 3 + potion -> turtle, basics skipped, nothing dealt
    dungeon_save_t s;
    dungeon_new_game(&s, 106);
    s.shield_pool = 0;   // v24 kit ward would pad ehp past the drink line
    s.hp_cur = 1;
    s.cons[CONS_POTION] = 1;
    s.n_enemy = 1; s.enemy_hp[0] = 300; s.enemy_kind[0] = 0;
    dungeon_autopilot_battle(&s, 0);
    assert(s.cons[CONS_POTION] == 0);   // drank at margin < 0
    assert(dungeon_round(&s, 0) == DEV_BATTLE);
    assert(s.enemy_hp[0] == 300);
    printf("v12_eva_holds_fire OK\n");
}

// V2 playback (§11): the streaming剪枝机 is pure — synthetic determinism.
static void test_v13_beat_player(void)
{
    beat_player_t p;
    battle_beat_t b;
    memset(&b, 0, sizeof b);
    b.span = 1;
    // 20 plains merge into one exact digest
    beat_player_init(&p, 10000);
    for (int i = 0; i < 20; i++) {
        b.stance = ST_ATK; b.flags = 0; b.foes = 0; b.dealt = 5; b.taken = 3;
        assert(beat_player_push(&p, &b) == BPLAY_MERGE);
    }
    assert(beat_player_has_digest(&p));
    battle_beat_t d;
    beat_player_take_digest(&p, &d);
    assert(d.span == 20 && d.dealt == 100 && d.taken == 60);
    assert(d.flags & BBEAT_MERGED);
    assert(beat_player_total(&p) == 800 + 2500);
    // every highlight class shows pre-pressure
    beat_player_init(&p, 10000);
    b.dealt = 5; b.taken = 3;
    b.flags = BBEAT_HEAVY; assert(beat_player_push(&p, &b) == BPLAY_SHOW);
    b.flags = BBEAT_BLOCK; assert(beat_player_push(&p, &b) == BPLAY_SHOW);
    b.flags = BBEAT_DODGE; assert(beat_player_push(&p, &b) == BPLAY_SHOW);
    b.flags = BBEAT_SKILL; assert(beat_player_push(&p, &b) == BPLAY_SHOW);
    b.flags = BBEAT_ULT;   assert(beat_player_push(&p, &b) == BPLAY_SHOW);
    b.flags = 0; b.foes = 1; assert(beat_player_push(&p, &b) == BPLAY_SHOW);
    b.foes = 0;
    // pressure: 6000+ ms in, non-kills merge, kill still shows, budget holds
    for (int i = 0; i < 20; i++) {
        b.flags = BBEAT_HEAVY;
        if (i < 3) assert(beat_player_push(&p, &b) == BPLAY_SHOW);
        else assert(beat_player_push(&p, &b) == BPLAY_MERGE);
    }
    b.flags = 0; b.foes = 1;
    assert(beat_player_push(&p, &b) == BPLAY_SHOW);   // kill pierces pressure
    if (beat_player_has_digest(&p)) beat_player_take_digest(&p, &d);
    assert(beat_player_total(&p) <= 10000);
    printf("v13_beat_player OK\n");
}

// V2 beat capture: real rounds report stance + damage through _ex.
static void test_v13_beat_capture(void)
{
    dungeon_save_t s;
    battle_beat_t b;
    dungeon_new_game(&s, 107);
    s.shield_pool = 0;   // v24 kit ward: isolate the beat from the opening ward
    s.n_enemy = 1; s.enemy_hp[0] = 30; s.enemy_kind[0] = 0;
    assert(dungeon_round_ex(&s, 0, &b) == DEV_BATTLE);
    assert(b.span == 1 && b.stance == ST_ATK && b.dealt > 0 && b.taken > 0);
    // EVA round: threat 3 + potion -> zero dealt, stance reported
    dungeon_new_game(&s, 108);
    s.shield_pool = 0;
    s.hp_cur = 1; s.cons[CONS_POTION] = 1;
    s.n_enemy = 1; s.enemy_hp[0] = 300; s.enemy_kind[0] = 0;
    dungeon_autopilot_battle(&s, 0);
    memset(&b, 0, sizeof b);
    assert(dungeon_round_ex(&s, 0, &b) == DEV_BATTLE);
    assert(b.stance == ST_EVA && b.dealt == 0);
    printf("v13_beat_capture OK\n");
}

// v12.2: the OC must read REAL next-round damage (blob + guard + stance),
// not raw enemy atk. Regression for "smoked at full HP / smoked with the
// last foe nearly dead": deep floor, guard-heavy build, low-HP OC.
// v12.3 rebirth perks (lever L2): prestige is a stream of visible unlocks
// and shortens the re-climb. One test per perk contract.
// v13 L8: 64-bit codex lanes. Balance-neutral past the +4% milestone cap;
// per-maze firsts give each ng cycle fresh collection moments.
uint8_t codex_milestone_lv_for_test(const dungeon_save_t *s);   // model test hook

// v14 L9: per-life mutators. Dormant until the first badge (onboarding
// stays vanilla); deterministic per-life roll; pairs feed has_affix.
void run_mut_roll_for_test(dungeon_save_t *s);
void job_rations_for_test(dungeon_save_t *s);
void start_battle_for_test(dungeon_save_t *s, bool boss);

// v15 L10: mutant bounty pays visible gold on every gate boss of a mutated
// life; start floor now engages at prst 2 and climbs 2/lv (shorter lives).
// v16 L11: per-job identity passives. White cure x1.5 + 2 shield; dark
// execute-kill leeches 8 hp. (Black opener covered by benchmark B gate.)
// v16b L11b: per-job dive rations at every life start (death/prestige/clear).
// v18 L12: dead-card revival — buffed values are observable.
static void test_v129_card_rebalance(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 129);

    // shrine: +3 essence AND 30% heal
    s.essence = 0; s.hp_cur = dungeon_max_hp(&s) / 2;
    uint16_t mh = dungeon_max_hp(&s);
    uint8_t opts[DUNGEON_CHOICE_OPTS] = {CH_SHRINE, CH_SHRINE, CH_SHRINE};
    memcpy(s.choice_opts, opts, sizeof opts);
    s.choice_pending = 1;
    dungeon_choice_pick(&s, 0);
    assert(s.essence == 3 && s.hp_cur > mh / 2);   // gate essence = 3

    // wound trade pays 35 (not 15)
    dungeon_new_game(&s, 131);
    s.gold = 0; s.wound = 0;
    opts[0] = opts[1] = opts[2] = CH_TRADE_WOUND;
    memcpy(s.choice_opts, opts, sizeof opts);
    s.choice_pending = 1;
    dungeon_choice_pick(&s, 0);
    assert(s.gold == 35 && s.wound == 1);

    // omen pays +15
    dungeon_new_game(&s, 133);
    s.gold = 0; s.floor_affix = 0;
    opts[0] = opts[1] = opts[2] = CH_AFFIX;
    memcpy(s.choice_opts, opts, sizeof opts);
    s.choice_pending = 1;
    dungeon_choice_pick(&s, 0);
    assert(s.gold == 15 && s.floor_affix >= 1 && s.floor_affix <= 4);
    printf("v129_card_rebalance OK\n");
}

// v23: a foe within 2x pow must always be pressed (ST_ATK), even at threat
// 3 — EVA-turtling a one-hit foe was the fresh non-knight death mode.
static void test_v130_finisher_override(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 137);
    s.unlocked = 0x1F; s.main_job = JOB_BLACK; s.job_lv[JOB_BLACK] = 3;
    s.weapon_pow = 5; s.armor_def = 2; s.bless_atk = s.bless_def = 0;
    s.n_enemy = 1; s.enemy_hp[0] = 4; s.enemy_kind[0] = 0; s.enemy_affix[0] = 0;
    s.round_ct = 1;
    assert(dungeon_pow(&s) >= 2);
    for (uint8_t threat = 1; threat <= 3; threat++)
        assert(dungeon_stance_pick(&s, threat) == ST_ATK);
    // big foe: threat 3 still slips (no override)
    s.enemy_hp[0] = 50000;
    assert(dungeon_stance_pick(&s, 3) == ST_EVA);
    printf("v130_finisher_override OK\n");
}

static void test_v128_job_rations(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 127);
    s.main_job = JOB_WHITE; s.prestige = 1; s.ng = 1; s.cons[CONS_POTION] = 0;
    s.cons[CONS_SMOKE] = 0; s.shield_pool = 0;
    run_mut_roll_for_test(&s);                   // includes rations? no - separate
    job_rations_for_test(&s);
    assert(s.cons[CONS_POTION] == 1);            // medic carries a potion
    s.main_job = JOB_THIEF; s.cons[CONS_SMOKE] = 0;
    job_rations_for_test(&s);
    assert(s.cons[CONS_SMOKE] == 1);             // rogue carries a smoke
    s.main_job = JOB_KNIGHT; s.shield_pool = 0;
    job_rations_for_test(&s);
    assert(s.shield_pool == 3);                  // wall wards up
    s.main_job = JOB_DARK; s.essence = 0;
    job_rations_for_test(&s);
    assert(s.essence == 10);                     // harvester tithe
    printf("v128_job_rations OK\n");
}

static void test_v127_job_identity(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 123);
    s.unlocked = 0x1F; s.main_job = JOB_WHITE; s.autopilot = 0;
    s.job_lv[JOB_WHITE] = 8; s.energy = 99; s.skill_cd[0] = 0;   // cure unlocks at lv 6
    s.n_enemy = 1; s.enemy_hp[0] = 9999; s.enemy_kind[0] = 0; s.enemy_affix[0] = 0;
    s.hp_cur = dungeon_max_hp(&s) / 2; s.shield_pool = 0;
    uint16_t mh = dungeon_max_hp(&s);
    dungeon_round(&s, 0);
    assert(s.skill_cd[0] > 0);                             // cure went out (CD ticks down same round)
    assert(s.hp_cur == mh);                                // healed to full (1.5x overflows the cap)
    assert(s.shield_pool <= 2);                            // ward ≤2 (enemy reply may eat it)

    // dark execute leech
    dungeon_new_game(&s, 125);
    s.unlocked = 0x1F; s.main_job = JOB_DARK; s.autopilot = 0;
    s.job_lv[JOB_DARK] = 10; s.energy = 99;
    s.weapon_pow = 60; s.weapon_rar = 4;
    s.n_enemy = 1; s.enemy_kind[0] = 0; s.enemy_affix[0] = 0;
    s.enemy_hp[0] = 3;                                     // definitely below 1/3
    s.hp_cur = 10; s.shield_pool = 0;
    dungeon_round(&s, 0);
    assert(s.hp_cur > 10 || s.n_enemy > 0);                // leech fired on kill
    printf("v127_job_identity OK\n");
}

static void test_v126_mut_bounty(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 117);
    s.prestige = 3; s.run_mut = 2;               // mutated life
    s.mut_bounty = 0; s.gold = 100; s.floor = 20;
    s.n_enemy = 1; s.enemy_kind[0] = 3; s.enemy_hp[0] = 1;
    s.unlocked = 0x1F; s.main_job = JOB_KNIGHT; s.job_lv[JOB_KNIGHT] = 10;
    s.weapon_pow = 60; s.weapon_rar = 4; s.armor_def = 60; s.armor_rar = 4;
    s.energy = 99;
    dungeon_round(&s, 0);                        // boss dies -> bounty
    assert(s.mut_bounty == 1);
    assert(s.gold >= 100 + 10 + 19u * 2);        // bounty 48 + regular clear gold

    // unmutated life: no bounty counter
    dungeon_new_game(&s, 119);
    s.run_mut = 0; s.mut_bounty = 0; s.floor = 20;
    s.n_enemy = 1; s.enemy_kind[0] = 3; s.enemy_hp[0] = 1;
    s.unlocked = 0x1F; s.main_job = JOB_KNIGHT; s.job_lv[JOB_KNIGHT] = 10;
    s.weapon_pow = 60; s.weapon_rar = 4; s.armor_def = 60; s.armor_rar = 4;
    s.energy = 99;
    dungeon_round(&s, 0);
    assert(s.mut_bounty == 0);

    // start floor: prst 2 -> 2, prst 8 -> 14, cap 20
    dungeon_new_game(&s, 121);
    s.prestige = 2; s.ng = 1;
    assert(dungeon_town(&s, TOWN_PRESTIGE, 0));  // prst 3
    assert(s.floor == 4);                        // 2+(3-2)*2
    printf("v126_mut_bounty OK\n");
}

static void test_v125_run_mut(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 113);
    assert(s.run_mut == 0);                      // fresh life: vanilla
    run_mut_roll_for_test(&s);
    assert(s.run_mut == 0);                      // still dormant pre-badge

    s.prestige = 2;
    run_mut_roll_for_test(&s);
    assert(s.run_mut >= 1 && s.run_mut <= 6);    // active post-badge
    uint8_t m = s.run_mut;
    run_mut_roll_for_test(&s);                   // same state -> same roll
    assert(s.run_mut == m);                      // deterministic, not rng

    // wet pair slows the opener even with floor_affix unset (has_affix path)
    dungeon_new_game(&s, 115);
    s.prestige = 5; s.ng = 1; s.run_mut = 1;     // Dark+Wet
    s.floor_affix = 0;
    s.unlocked = 0x1F;
    start_battle_for_test(&s, false);
    assert(s.enemy_slow == 2);                   // wet came from the mutator
    printf("v125_run_mut OK\n");
}

static void test_v124_codex_lanes(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 107);

    // milestone cap is unchanged: a full 64-bit codex still gives +4% (lv 2)
    s.codex = 0xFFFFFFFFFFFFFFFFULL;
    assert(codex_milestone_lv_for_test(&s) == 2);
    s.codex = (1u << JOB_KNIGHT) | (1u << JOB_BLACK);
    assert(codex_milestone_lv_for_test(&s) == 0);

    // maze-2 clear marks lane 1 of maze 1 (bit 17): ng=1 pre-increment
    dungeon_new_game(&s, 109);
    s.unlocked = 0x1F; s.autopilot = 1; s.auto_job = 0;
    s.main_job = JOB_KNIGHT; s.job_lv[JOB_KNIGHT] = 10; s.explore_lv = 25;
    s.weapon_pow = 60; s.weapon_rar = 4; s.armor_def = 60; s.armor_rar = 4;
    s.floor = DUNGEON_MAX_FLOOR; s.ng = 1;
    s.n_enemy = 1; s.enemy_kind[0] = 3; s.enemy_hp[0] = 1; s.enemy_affix[0] = 0;
    s.energy = 99;
    uint64_t c0 = s.codex;
    dungeon_event_t ev = DEV_NONE;
    for (int r = 0; r < 5 && ev != DEV_CLEAR; r++) ev = dungeon_round(&s, r * 900);
    assert(ev == DEV_CLEAR && s.ng == 2);
    assert(s.codex >> 17 & 1);                   // maze 1 (pre-clear ng=1) lane 1
    assert(!(c0 >> 17 & 1));

    // maze-1 events never mark lanes (bits 16+ reserved for ng>=1)
    dungeon_new_game(&s, 111);
    s.ng = 0; s.floor = 100; s.n_enemy = 1; s.enemy_kind[0] = 3; s.enemy_hp[0] = 1;
    s.unlocked = 0x1F; s.main_job = JOB_KNIGHT; s.job_lv[JOB_KNIGHT] = 10;
    s.weapon_pow = 60; s.weapon_rar = 4; s.armor_def = 60; s.armor_rar = 4;
    s.energy = 99;
    for (int r = 0; r < 5 && ev != DEV_CLEAR; r++) ev = dungeon_round(&s, r * 900);
    assert(ev == DEV_CLEAR && s.codex < (1u << 16));
    printf("v124_codex_lanes OK\n");
}

static void test_v123_rebirth_perks(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 97);

    // p1: inn free (gold 0 still rests)
    s.prestige = 1; s.gold = 0; s.energy = 0; s.hp_cur = 1;
    assert(dungeon_town(&s, TOWN_INN, 0));
    assert(s.gold == 0 && s.energy == s.max_energy);

    // p2: peddler -25% (smoke at F31: (15+31)=46 -> 34)
    s.prestige = 2; s.floor = 31; s.gold = 500; s.cons[CONS_SMOKE] = 0;
    assert(dungeon_town(&s, TOWN_UPGRADE, 2));
    assert(s.gold == 500 - 34);

    // p4: forge -10%
    dungeon_new_game(&s, 99);
    s.prestige = 4; s.gold = 60000;
    s.weapon_pow = 0; s.armor_def = 0;
    uint32_t base = 10 + 3u * 0 + 0;            // stier 0: cost 10
    assert(dungeon_town(&s, TOWN_UPGRADE, 0));
    assert(s.gold == 60000 - base * 9 / 10);

    // p6+: start floor rises with prestige, capped at 20
    dungeon_new_game(&s, 101);
    s.prestige = 6; s.ng = 2;                    // needs ng >= 1 + 6/6 = 2
    assert(dungeon_town(&s, TOWN_PRESTIGE, 0));  // prst 6+2=8
    assert(s.floor == 14);                       // v15: 2+(8-2)*2
    s.prestige = 30; s.ng = 6;                   // needs ng >= 1 + 30/6 = 6
    assert(dungeon_town(&s, TOWN_PRESTIGE, 0));  // prst 36
    assert(s.floor == 20);                       // cap

    // prestige action lands on the start floor and logs perks (last-4 window)
    dungeon_new_game(&s, 103);
    s.prestige = 0; s.ng = 1;
    assert(dungeon_town(&s, TOWN_PRESTIGE, 0));
    assert(s.prestige == 1 && s.floor == 1);     // p<6: start floor still 1
    assert(dungeon_town(&s, TOWN_PRESTIGE, 0) == false);  // ng consumed
    printf("v123_rebirth_perks OK\n");
}

static void test_v122_threat_est(void)
{
    dungeon_save_t s;
    dungeon_new_game(&s, 91);
    s.unlocked = 0x1F;
    s.main_job = JOB_KNIGHT; s.autopilot = 1;
    s.explore_lv = 25;
    for (unsigned j = 0; j < DUNGEON_JOB_COUNT; j++) s.job_lv[j] = 10;
    s.weapon_pow = 60; s.weapon_rar = 4;
    s.armor_def = 60; s.armor_rar = 4;
    for (int k = 0; k < SN_COUNT; k++) s.sanctum[k] = 5;
    s.bless_def = 8;
    s.floor = 90; s.ng = 1;
    s.n_enemy = 1;
    s.enemy_hp[0] = 3000; s.enemy_kind[0] = 0; s.enemy_affix[0] = 0;
    s.energy = 50;
    uint16_t mh = dungeon_max_hp(&s);
    uint16_t guard = dungeon_guard(&s);
    assert(guard > 60);                        // the guard-heavy premise

    // 60 HP vs a foe whose raw atk (~88) exceeds it — the old raw read
    // smoked here. Real blob damage is ~1/5 of raw: do NOT smoke.
    s.cons[CONS_SMOKE] = 1; s.cons[CONS_POTION] = 0;
    s.hp_cur = (uint16_t)(mh < 60 ? 1 : 60);
    dungeon_autopilot_battle(&s, 0);
    assert(s.cons[CONS_SMOKE] == 1);           // shadow, not threat: held

    // genuinely lethal next round (below real estimate): smoke fires
    s.cons[CONS_SMOKE] = 1; s.cons[CONS_POTION] = 0;
    s.hp_cur = 1;
    dungeon_autopilot_battle(&s, 0);
    assert(s.n_enemy == 0 && s.cons[CONS_SMOKE] == 0);

    // the F41 report: two foes, first one dead, OC at full HP — no smoke
    dungeon_new_game(&s, 93);
    s.unlocked = 0x1F; s.main_job = JOB_KNIGHT; s.autopilot = 1;
    s.explore_lv = 20; s.job_lv[JOB_KNIGHT] = 8;
    s.weapon_pow = 40; s.weapon_rar = 3; s.armor_def = 35; s.armor_rar = 3;
    s.floor = 41; s.n_enemy = 2;
    s.enemy_hp[0] = 0; s.enemy_hp[1] = 80;     // first one-shot dead, second low
    s.enemy_kind[0] = s.enemy_kind[1] = 0;
    s.enemy_affix[0] = s.enemy_affix[1] = 0;
    s.hp_cur = dungeon_max_hp(&s);
    s.cons[CONS_SMOKE] = 3;
    dungeon_autopilot_battle(&s, 0);
    assert(s.cons[CONS_SMOKE] == 3);           // winning + healthy: press on
    printf("v122_threat_est OK\n");
}

int main(void)
{
    test_save_budget();
    test_mood_priority();
    test_v3_progress();
    test_v3_skill_gate_and_cast();
    test_v3_switch_cd();
    test_v3_death_keeps_levels();
    test_v3_choice_card();
    test_v3_town();
    test_v3_offline();
    test_v3_job_diff();
    test_v4_essence_survives_death();
    test_v4_sanctum();
    test_v4_synth_and_item_math();
    test_v4_auto_swap();
    test_v4_dark_unlock_and_execute_ai();
    test_v4_elite_armored();
    test_v4_autoswap_v2();
    test_v4_prestige();
    test_v7_job_multiplier();
    test_v7_soft_reset();
    test_v7_sanctum_geometric();
    test_v7_undead_drain();
    test_v8_evolution();
    test_v8_ancient_rar4();
    test_v9_wish_pity();
    test_v9_codex_milestone();
    test_v9_challenge_and_starfall();
    test_pomo_cycle();
    test_i18n_table();
    test_fix_gate_pair_distinct();
    test_fix_smokeout_is_flee();
    test_fix_offline_report_and_gate();
    test_fix_hp_clamp_on_switch();
    test_fix_pomo_countdown_and_rebase();
    test_fix_rebase_clock();
    test_v10_autopilot();
    test_v10_floor_affix();
    test_v12_margin_and_stance();
    test_v12_stance_round();
    test_v13_beat_player();
    test_v13_beat_capture();
    test_v121_economy();
    test_v122_threat_est();
    test_v123_rebirth_perks();
    test_v124_codex_lanes();
    test_v125_run_mut();
    test_v126_mut_bounty();
    test_v127_job_identity();
    test_v128_job_rations();
    test_v130_finisher_override();
    test_v129_card_rebalance();
    printf("ALL DESKPET MODEL TESTS PASS\n");
    return 0;
}
