// main/chiptune_songs.c —— one 16-bar musical cell, six arrange tables.
// Melodies follow the BGM research verbatim:
//   cell A "wind"  G4-A4-C5-D5   (town / victory, C mixolydian)
//   cell B "crystal" D4-F4-G4-E4 (dungeon / battle, D dorian)
// Rests matter: the badge speaker needs air to sound like a flute.
#include "chiptune.h"

// ---- helpers: tiny static builders would be overkill; write tables flat ----
#define N(n, d, du, v) {n, d, du, v}
#define R(d)           {0, d, 0, 0}

// ============================================================================
// Ambient set (device pass 5). The BGM loops for hours on a desk, so every
// scene is a quiet broken-chord piece in the "To Zanarkand" idiom: the
// arpeggio IS the music, the lead only places a few long notes on top, bass
// holds whole notes, no drums. bpm 56-63. Velocities 3-6 (of 15).
#define ARP8(a, b, c, d) N(a, 1, 50, 5), N(b, 1, 50, 4), N(c, 1, 50, 4), N(d, 1, 50, 5), \
                         N(c, 1, 50, 4), N(b, 1, 50, 4), N(a, 1, 50, 5), N(b, 1, 50, 3)

// ===================== TOWN (4/4, bpm63, D major, 8 bars) ==================
// chords: D | A | Bm | G | D | A | G | D
static const ct_step_t TOWN_COUNTER[] = {
    ARP8(50, 54, 57, 62), ARP8(45, 49, 52, 57), ARP8(47, 50, 54, 59), ARP8(43, 47, 50, 55),
    ARP8(50, 54, 57, 62), ARP8(45, 49, 52, 57), ARP8(43, 47, 50, 55), ARP8(50, 54, 57, 62),
};
// lead: a few long notes, mostly air
static const ct_step_t TOWN_LEAD[] = {
    R(4), N(66, 4, 50, 6),                 // F#4
    R(2), N(64, 2, 50, 5), N(61, 4, 50, 6), // E4 C#4
    N(62, 8, 50, 6),                       // D4
    R(4), N(67, 2, 50, 5), N(66, 2, 50, 5), // G4 F#4
    N(69, 6, 50, 6), N(66, 2, 50, 5),      // A4 F#4
    N(64, 8, 50, 6),                       // E4
    R(2), N(67, 4, 50, 5), N(64, 2, 50, 5), // G4 E4
    N(62, 8, 50, 6),                       // D4
};
static const ct_step_t TOWN_BASS[] = {
    N(38, 8, 25, 6), N(45, 8, 25, 6), N(47, 8, 25, 6), N(43, 8, 25, 6),
    N(38, 8, 25, 6), N(45, 8, 25, 6), N(43, 8, 25, 6), N(38, 8, 25, 6),
};
static const ct_step_t TOWN_NOISE[] = { R(64) };

// ===================== DUNGEON (4/4, bpm58, A minor, 8 bars) ===============
// chords: Am | F | Am | G | Am | F | G | Am — arpeggio and a low drone
static const ct_step_t DUNGEON_COUNTER[] = {
    ARP8(45, 48, 52, 57), ARP8(41, 45, 48, 53), ARP8(45, 48, 52, 57), ARP8(43, 47, 50, 55),
    ARP8(45, 48, 52, 57), ARP8(41, 45, 48, 53), ARP8(43, 47, 50, 55), ARP8(45, 48, 52, 57),
};
static const ct_step_t DUNGEON_LEAD[] = {
    R(8),
    R(4), N(60, 4, 50, 5),                 // C4
    N(59, 8, 50, 5),                       // B3
    R(8),
    R(4), N(64, 4, 50, 5),                 // E4
    N(62, 6, 50, 5), N(60, 2, 50, 4),      // D4 C4
    N(59, 8, 50, 5),                       // B3
    N(57, 8, 50, 5),                       // A3
};
static const ct_step_t DUNGEON_BASS[] = {
    N(33, 16, 25, 6), N(33, 8, 25, 6), N(31, 8, 25, 5),
    N(33, 16, 25, 6), N(31, 8, 25, 5), N(33, 8, 25, 6),
};
static const ct_step_t DUNGEON_NOISE[] = { R(64) };

// ===================== BATTLE (4/4, bpm60, E minor, 8 bars) ================
// chords: Em | C | G | D | Em | C | D | Em. Same idiom, a shade darker.
static const ct_step_t BATTLE_COUNTER[] = {
    ARP8(52, 55, 59, 64), ARP8(48, 52, 55, 60), ARP8(55, 59, 62, 67), ARP8(50, 54, 57, 62),
    ARP8(52, 55, 59, 64), ARP8(48, 52, 55, 60), ARP8(50, 54, 57, 62), ARP8(52, 55, 59, 64),
};
static const ct_step_t BATTLE_LEAD[] = {
    R(4), N(71, 4, 50, 6),                 // B4
    R(2), N(72, 2, 50, 5), N(71, 4, 50, 6), // C5 B4
    N(67, 8, 50, 6),                       // G4
    N(69, 6, 50, 6), N(71, 2, 50, 5),      // A4 B4
    N(76, 4, 50, 6), N(74, 4, 50, 5),      // E5 D5
    N(72, 6, 50, 6), N(71, 2, 50, 5),      // C5 B4
    N(69, 4, 50, 6), N(66, 4, 50, 5),      // A4 F#4
    N(64, 8, 50, 6),                       // E4
};
static const ct_step_t BATTLE_BASS[] = {
    N(40, 8, 25, 6), N(36, 8, 25, 6), N(43, 8, 25, 6), N(38, 8, 25, 6),
    N(40, 8, 25, 6), N(36, 8, 25, 6), N(38, 8, 25, 6), N(40, 8, 25, 6),
};
static const ct_step_t BATTLE_NOISE[] = { R(64) };
static const ct_step_t BOSS_NOISE[] = { R(64) };

// ===================== FANFARE (4/4, bpm140, trans +2, once) ===============
// G4 A4 C5 D5 | E5-- D5 C5 | A4 G4 E4 F4 | G4 C5------
static const ct_step_t FANFARE_LEAD[] = {
    N(67, 2, 50, 13), N(69, 2, 50, 12), N(72, 2, 50, 13), N(74, 2, 50, 12),
    N(76, 4, 50, 13), N(74, 2, 50, 12), N(72, 2, 50, 12),
    N(69, 2, 50, 12), N(67, 2, 50, 12), N(64, 2, 50, 11), N(65, 2, 50, 12),
    N(67, 2, 50, 12), N(72, 6, 50, 14),                                  // land on C
};
static const ct_step_t FANFARE_BASS[] = {
    N(36, 8, 25, 11), N(43, 8, 25, 10), N(36, 8, 25, 11), N(36, 8, 25, 11),
};
static const ct_step_t FANFARE_NOISE[] = {
    N(1, 1, 0, 14), R(15), N(1, 1, 0, 11), R(13),                        // one snare roll
    N(1, 1, 0, 14), R(15), R(16),
};
static const ct_step_t FANFARE_COUNTER[] = { R(32) };

// ===================== FAIL (4/4, bpm80, trans -5, once) ===================
// G4 F4 E4 D4 | C4-- B3 A3 | G3-- F3-- | F3------   (stops on F: unresolved)
static const ct_step_t FAIL_LEAD[] = {
    N(67, 2, 50, 11), N(65, 2, 50, 10), N(64, 2, 50, 10), N(62, 2, 50, 10),
    N(60, 4, 50, 10), N(59, 2, 50, 9), N(57, 2, 50, 9),
    N(55, 4, 50, 9), N(53, 4, 50, 9),
    N(53, 8, 50, 9),
};
static const ct_step_t FAIL_BASS[] = {
    N(36, 8, 25, 9), N(43, 8, 25, 8), N(45, 8, 25, 9), N(41, 8, 25, 9),
};
static const ct_step_t FAIL_NOISE[] = { R(64) };
static const ct_step_t FAIL_COUNTER[] = { R(32) };

// ===================== arranges ============================================
#define ARR(name, bars_, spb, tr, mask, bpm_, once_, lead, cnt, bass, nois) \
    static const ct_arrange_t name = { (uint8_t)(bars_), (uint8_t)(spb), (tr), (mask), \
        (bpm_), (once_), { lead, cnt, bass, nois }, \
        { sizeof(lead) / sizeof(lead[0]), sizeof(cnt) / sizeof(cnt[0]), \
          sizeof(bass) / sizeof(bass[0]), sizeof(nois) / sizeof(nois[0]) } }

ARR(ARR_TOWN,    8, 8, 0,  0, 63, 0, TOWN_LEAD, TOWN_COUNTER, TOWN_BASS, TOWN_NOISE);
ARR(ARR_DUNGEON, 8, 8, 0,  0, 58, 0, DUNGEON_LEAD, DUNGEON_COUNTER, DUNGEON_BASS, DUNGEON_NOISE);
ARR(ARR_BATTLE,  8, 8, 0,  0, 60, 0, BATTLE_LEAD, BATTLE_COUNTER, BATTLE_BASS, BATTLE_NOISE);
ARR(ARR_BOSS,    8, 8, -3, 0, 56, 0, BATTLE_LEAD, BATTLE_COUNTER, BATTLE_BASS, BOSS_NOISE);
ARR(ARR_FANFARE, 4, 8, 2,  0, 140, 1, FANFARE_LEAD, FANFARE_COUNTER, FANFARE_BASS, FANFARE_NOISE);
ARR(ARR_FAIL,    4, 8, -5, 0, 80,  1, FAIL_LEAD, FAIL_COUNTER, FAIL_BASS, FAIL_NOISE);

const ct_arrange_t *chiptune_scene_arrange(ct_scene_t scene)
{
    switch (scene) {
    case CT_SCENE_TOWN:    return &ARR_TOWN;
    case CT_SCENE_DUNGEON: return &ARR_DUNGEON;
    case CT_SCENE_BATTLE:  return &ARR_BATTLE;
    case CT_SCENE_BOSS:    return &ARR_BOSS;
    case CT_SCENE_FANFARE: return &ARR_FANFARE;
    case CT_SCENE_FAIL:    return &ARR_FAIL;
    default:               return 0;
    }
}

// ===================== SE (duty 0 = noise) =================================
// SE timing uses a fixed 25 ms step in chiptune.c. R(2) is the required
// 50 ms breathing gap; the longest individual note below is 150 ms.
static const ct_step_t SE_OK[]     = { N(72, 1, 50, 12), R(1) };
static const ct_step_t SE_BACK[]   = { N(64, 2, 50, 10) };
static const ct_step_t SE_HIT[]    = { N(1, 2, 0, 14) };
static const ct_step_t SE_CRIT[]   = { N(1, 2, 0, 15), N(84, 3, 25, 14) };
static const ct_step_t SE_JOB[]    = { N(72, 2, 50, 13), N(76, 2, 50, 13),
                                       N(79, 2, 50, 13), N(84, 3, 50, 14) };
static const ct_step_t SE_WARN[]   = { N(62, 2, 50, 10), R(2), N(62, 2, 50, 10) };
static const ct_step_t SE_FAIL_SE[] = { N(67, 3, 50, 11), N(64, 3, 50, 10),
                                        N(60, 5, 50, 10) };
static const ct_step_t SE_LOOT[]   = { N(76, 2, 25, 12), N(79, 2, 25, 13) };
static const ct_step_t SE_STAIR[]  = { N(60, 2, 50, 10), N(67, 2, 50, 11),
                                       N(72, 3, 50, 12) };
static const ct_step_t SE_RETURN[] = { N(72, 2, 50, 11), N(67, 2, 50, 10),
                                       N(60, 3, 50, 10) };
static const ct_step_t SE_CHOICE[] = { N(69, 2, 25, 11), R(2), N(74, 3, 50, 12) };
static const ct_step_t SE_ULT_K[]  = { N(48, 3, 25, 13), N(55, 3, 25, 13), N(72, 4, 50, 14) };
static const ct_step_t SE_ULT_B[]  = { N(50, 2, 25, 13), N(57, 2, 25, 14), N(74, 4, 50, 15), N(1, 2, 0, 13) };
static const ct_step_t SE_ULT_W[]  = { N(65, 2, 50, 11), N(69, 2, 50, 12), N(72, 2, 50, 13), N(77, 4, 50, 14) };
static const ct_step_t SE_ULT_T[]  = { N(67, 1, 25, 12), N(71, 1, 25, 13), N(74, 1, 25, 13), N(79, 3, 25, 15) };
static const ct_step_t SE_ULT_D[]  = { N(64, 3, 12, 13), N(60, 3, 12, 13), N(57, 5, 12, 14) };
static const ct_step_t SE_POMO_F[] = { N(76, 6, 50, 12), R(2), N(79, 6, 50, 13),
                                       R(2), N(84, 6, 50, 14) };
static const ct_step_t SE_POMO_B[] = { N(79, 6, 50, 12), R(2), N(76, 6, 50, 12) };
static const ct_step_t SE_AG_NEED[] = { N(84, 3, 50, 13), R(2), N(84, 3, 50, 13),
                                        R(2), N(84, 3, 50, 13) };
static const ct_step_t SE_AG_DONE[] = { N(72, 4, 50, 11), N(76, 4, 50, 12), N(79, 4, 50, 13) };
static const ct_step_t SE_AG_ERR[]  = { N(67, 4, 50, 12), N(64, 4, 50, 11), N(60, 4, 50, 11) };
static const ct_step_t SE_MIMIC[]   = { N(1, 3, 0, 14), N(48, 3, 25, 13), N(60, 3, 25, 14) };
static const ct_step_t SE_RARE[]    = { N(84, 2, 25, 12), N(88, 2, 25, 13), N(91, 4, 25, 14) };

const ct_step_t *chiptune_se_steps(ct_se_t id, uint16_t *nstep)
{
    switch (id) {
    case CT_SE_OK:   *nstep = sizeof(SE_OK) / sizeof(SE_OK[0]);     return SE_OK;
    case CT_SE_BACK: *nstep = sizeof(SE_BACK) / sizeof(SE_BACK[0]); return SE_BACK;
    case CT_SE_HIT:  *nstep = sizeof(SE_HIT) / sizeof(SE_HIT[0]);   return SE_HIT;
    case CT_SE_CRIT: *nstep = sizeof(SE_CRIT) / sizeof(SE_CRIT[0]); return SE_CRIT;
    case CT_SE_FANFARE: *nstep = sizeof(SE_JOB) / sizeof(SE_JOB[0]); return SE_JOB;
    case CT_SE_WARN: *nstep = sizeof(SE_WARN) / sizeof(SE_WARN[0]); return SE_WARN;
    case CT_SE_FAIL: *nstep = sizeof(SE_FAIL_SE) / sizeof(SE_FAIL_SE[0]); return SE_FAIL_SE;
    case CT_SE_LOOT: *nstep = sizeof(SE_LOOT) / sizeof(SE_LOOT[0]); return SE_LOOT;
    case CT_SE_STAIR: *nstep = sizeof(SE_STAIR) / sizeof(SE_STAIR[0]); return SE_STAIR;
    case CT_SE_RETURN: *nstep = sizeof(SE_RETURN) / sizeof(SE_RETURN[0]); return SE_RETURN;
    case CT_SE_CHOICE: *nstep = sizeof(SE_CHOICE) / sizeof(SE_CHOICE[0]); return SE_CHOICE;
    case CT_SE_ULT_KNIGHT: *nstep = sizeof(SE_ULT_K) / sizeof(SE_ULT_K[0]); return SE_ULT_K;
    case CT_SE_ULT_BLACK: *nstep = sizeof(SE_ULT_B) / sizeof(SE_ULT_B[0]); return SE_ULT_B;
    case CT_SE_ULT_WHITE: *nstep = sizeof(SE_ULT_W) / sizeof(SE_ULT_W[0]); return SE_ULT_W;
    case CT_SE_ULT_THIEF: *nstep = sizeof(SE_ULT_T) / sizeof(SE_ULT_T[0]); return SE_ULT_T;
    case CT_SE_ULT_DARK: *nstep = sizeof(SE_ULT_D) / sizeof(SE_ULT_D[0]); return SE_ULT_D;
    case CT_SE_POMO_FOCUS: *nstep = sizeof(SE_POMO_F) / sizeof(SE_POMO_F[0]); return SE_POMO_F;
    case CT_SE_POMO_BREAK: *nstep = sizeof(SE_POMO_B) / sizeof(SE_POMO_B[0]); return SE_POMO_B;
    case CT_SE_AGENT_NEEDS: *nstep = sizeof(SE_AG_NEED) / sizeof(SE_AG_NEED[0]); return SE_AG_NEED;
    case CT_SE_AGENT_DONE: *nstep = sizeof(SE_AG_DONE) / sizeof(SE_AG_DONE[0]); return SE_AG_DONE;
    case CT_SE_AGENT_ERROR: *nstep = sizeof(SE_AG_ERR) / sizeof(SE_AG_ERR[0]); return SE_AG_ERR;
    case CT_SE_MIMIC: *nstep = sizeof(SE_MIMIC) / sizeof(SE_MIMIC[0]); return SE_MIMIC;
    case CT_SE_RARE: *nstep = sizeof(SE_RARE) / sizeof(SE_RARE[0]); return SE_RARE;
    default: *nstep = 0; return 0;
    }
}
