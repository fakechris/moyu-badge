// main/chiptune.h —— GAMEPLAY_V5 audio: motif + sequencer + 3-osc soft synth.
// Pure C, no BSP deps (host-testable). 16kHz/16bit/mono, matches bsp_audio.
// Docs: docs/GAMEPLAY_V5.md §audio; design source = BGM research notes.
#pragma once

#include <stdint.h>

#define CT_SR 16000
#define CT_LEAD    0
#define CT_COUNTER 1
#define CT_BASS    2
#define CT_NOISE   3
#define CT_CH_N    4

// Scenes (docs GAMEPLAY_V5 audio state table).
typedef enum {
    CT_SCENE_NONE = 0,   // silence (screen off / report)
    CT_SCENE_TOWN,       // bpm96  C mixolydian, sparse counter
    CT_SCENE_DUNGEON,    // bpm112 D dorian, drone
    CT_SCENE_BATTLE,     // bpm132 6/8, 2-bar loop
    CT_SCENE_BOSS,       // bpm120 trans -2, counter off
    CT_SCENE_FANFARE,    // bpm140 trans +2, once
    CT_SCENE_FAIL,       // bpm80  trans -5, once
    CT_SCENE_N,
} ct_scene_t;

// Sound effects (steal the noise lane, 8 Hz rate-limited).
typedef enum {
    CT_SE_OK = 0,      // ui confirm,  short C5 blip
    CT_SE_BACK,        // ui cancel,   short E4 blip
    CT_SE_HIT,         // battle hit,  noise burst
    CT_SE_CRIT,        // emphatic hit, noise + high accent
    CT_SE_FANFARE,     // clear/job, rising motif
    CT_SE_WARN,        // unavailable/energy empty, low double-blip
    CT_SE_FAIL,        // death/error, descending phrase
    CT_SE_LOOT,        // treasure/purchase success
    CT_SE_STAIR,       // floor transition, rising steps
    CT_SE_RETURN,      // return to town, falling steps
    CT_SE_CHOICE,      // blessing/choice accepted
    CT_SE_ULT_KNIGHT,
    CT_SE_ULT_BLACK,
    CT_SE_ULT_WHITE,
    CT_SE_ULT_THIEF,
    CT_SE_ULT_DARK,
    CT_SE_POMO_FOCUS,  // E5-G5-C6, 150 ms notes
    CT_SE_POMO_BREAK,  // G5-E5, 150 ms notes
    CT_SE_AGENT_NEEDS, // three attention blips
    CT_SE_AGENT_DONE,  // rising completion cue
    CT_SE_AGENT_ERROR, // falling error cue
    CT_SE_MIMIC,
    CT_SE_RARE,
    CT_SE_N,
} ct_se_t;

typedef struct {
    uint8_t note;   // 0 = rest, otherwise MIDI pitch
    uint8_t dur;    // steps
    uint8_t duty;   // pulse duty % (12/25/50/75); tonal channels only
    uint8_t vel;    // 0..15
} ct_step_t;

typedef struct {
    uint8_t bars;
    uint8_t steps_per_bar;
    int8_t transpose;
    uint8_t mute_mask;         // bit per channel: 1 = silent
    uint16_t bpm;
    uint8_t once;              // 1 = play through once, then silence
    const ct_step_t *ch[CT_CH_N];
    uint16_t nstep[CT_CH_N];
} ct_arrange_t;

typedef struct {
    const ct_arrange_t *arr;
    // sequencer per channel
    uint16_t idx[CT_CH_N];     // current step index
    uint16_t step_left[CT_CH_N];
    uint16_t step_len[CT_CH_N];
    uint32_t phase[CT_CH_N];
    uint32_t inc[CT_CH_N];
    uint16_t env[CT_CH_N];
    uint8_t wrapped[CT_CH_N];
    uint16_t tick[CT_CH_N];
    uint16_t lfsr;
    uint8_t lfsr_div;
    // SE voice (steals the noise lane; melodic SEs play on this osc too)
    uint8_t se_id, se_active, se_step, se_note_is_noise;
    uint16_t se_left, se_len;
    uint32_t se_phase;
    uint32_t se_inc;
    uint16_t se_env, se_duty;
    uint32_t since_se;         // samples since last SE start (8 Hz limiter)
    uint8_t idle;              // 1 = ambient mode: bpm-8, counter muted
    uint8_t bgm_vol, se_vol;   // 0..100
    int32_t lp;                // output low-pass state (Q8)
} ct_state_t;

void ct_init(ct_state_t *st);
void ct_set_arrange(ct_state_t *st, const ct_arrange_t *a);
void ct_set_idle(ct_state_t *st, int idle);          // bpm -8, counter muted
void ct_se(ct_state_t *st, ct_se_t id);              // 8 Hz rate-limited
void ct_render(ct_state_t *st, int16_t *out, int frames);
void ct_volumes(ct_state_t *st, uint8_t bgm, uint8_t se);

// Music data + scene lookup (chiptune_songs.c).
const ct_arrange_t *chiptune_scene_arrange(ct_scene_t scene);
const ct_step_t *chiptune_se_steps(ct_se_t id, uint16_t *nstep);

// Audio frontend. Weak no-ops here; audio_task.c provides the strong
// device-backed versions. UI/game code calls only these three.
void audio_bgm_scene(int scene);   // CT_SCENE_x (0 = silence)
void audio_se(int se);             // CT_SE_x
void audio_idle_feed(void);        // note user activity (exits ambient mode)
