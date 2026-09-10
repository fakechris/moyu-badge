// main/chiptune.c —— 4-channel soft synth + sequencer (BGM research spec):
// pulse lead / pulse counter / bass / LFSR noise, 16kHz mono int16.
// Envelope = instant attack + release tail; phase resets per note for the
// chiptune articulation. SEs steal the noise lane at max 8 Hz.
#include "chiptune.h"

#include <stddef.h>

// ---- weak frontend: sim/host links these; audio_task.c overrides ----------
__attribute__((weak)) void audio_bgm_scene(int scene) { (void)scene; }
__attribute__((weak)) void audio_se(int se) { (void)se; }
__attribute__((weak)) void audio_idle_feed(void) {}

#define VOL_MAX 32000   // per-sample clip ceiling (leave headroom for SE + BGM)
// Gain staging (2026-09-05 device fix): the old units (vel*2600 per voice,
// three voices + noise summed) hit the clip ceiling on nearly every sample
// even at bgm_vol 45 — a -4.5 dBFS hard-clipped square wave that threw the
// badge's speaker amp into protection (the device made clicks, no music).
// With these units a full mix at bgm_vol 100 peaks near full scale and the
// default 45/70 sits around -12 dBFS.
#define BGM_ENV_UNIT 620    // per velocity step, tonal BGM voices (max 15)
#define SE_ENV_UNIT 1500    // SE tonal
#define SE_NOISE_UNIT 800   // SE noise burst (hits were harsh on the badge)

// per-channel state that does not belong in the public save struct
static uint32_t s_duty[CT_CH_N];   // duty threshold, Q16

// ---- helpers ---------------------------------------------------------------
static uint32_t note_inc(int midi)   // Q16 phase increment per sample
{
    // f = 440 * 2^((m-69)/12); inc = f * 65536 / CT_SR.
    // Soft float on C3 is fine: called once per note, never per sample.
    float f = 440.0f;
    int n = midi - 69;
    while (n > 0) { f *= 1.0594631f; n--; }
    while (n < 0) { f /= 1.0594631f; n++; }
    return (uint32_t)(f * 65536.0f / CT_SR);
}

static uint16_t step_samples(const ct_arrange_t *a, int idle)
{
    if (!a) return 250;   // no BGM arranged (SE-only): neutral ~31 ms step
    uint16_t bpm = a->bpm;
    if (idle && bpm > 8) bpm = (uint16_t)(bpm - 8);   // ambient: bpm - 8
    uint32_t spb = (uint32_t)bpm * a->steps_per_bar;
    return (uint16_t)((60u * CT_SR + spb / 2) / (spb ? spb : 1));
}

static void noise_tick(ct_state_t *st)
{
    st->lfsr = (uint16_t)((st->lfsr >> 1) |
                          (((st->lfsr) ^ (st->lfsr >> 2) ^ (st->lfsr >> 3) ^ (st->lfsr >> 5)) & 1) << 15);
}

// ---- lifecycle -------------------------------------------------------------
void ct_init(ct_state_t *st)
{
    for (int i = 0; i < CT_CH_N; i++) {
        st->idx[i] = 0;
        st->step_left[i] = 0;
        st->step_len[i] = 0;
        st->phase[i] = 0;
        st->inc[i] = 0;
        st->env[i] = 0;
        st->wrapped[i] = 0;
        st->tick[i] = 0;
        s_duty[i] = 32768;
    }
    st->arr = 0;
    st->lfsr = 0xACE1;
    st->lfsr_div = 0;
    st->se_active = 0;
    st->se_step = 0;
    st->se_left = 0;
    st->since_se = CT_SR;   // allow the first SE immediately
    st->idle = 0;
    st->bgm_vol = 45;       // research: small speaker distorts past ~50
    st->se_vol = 70;
}

void ct_set_arrange(ct_state_t *st, const ct_arrange_t *a)
{
    st->arr = a;
    for (int i = 0; i < CT_CH_N; i++) {
        st->idx[i] = 0;
        st->step_left[i] = 0;
        st->phase[i] = 0;
        st->env[i] = 0;
        st->wrapped[i] = 0;
        st->tick[i] = 0;
    }
}

void ct_set_idle(ct_state_t *st, int idle) { st->idle = idle ? 1 : 0; }

void ct_volumes(ct_state_t *st, uint8_t bgm, uint8_t se)
{
    st->bgm_vol = bgm > 100 ? 100 : bgm;
    st->se_vol = se > 100 ? 100 : se;
}

void ct_se(ct_state_t *st, ct_se_t id)
{
    if ((unsigned)id >= CT_SE_N) return;
    if (st->since_se < CT_SR / 8) return;   // 8 Hz cap: auto-battle is not a gun
    st->since_se = 0;
    st->se_id = (uint8_t)id;
    st->se_active = 1;
    st->se_step = 0;
    st->se_left = 0;
    st->se_env = 0;
}

// Start the current step of a BGM channel. ALWAYS consumes step_len samples
// (caller decrements); rests render env=0 — otherwise every rest skews the
// loop point by one sample and the music phase-wanders pass to pass.
static void bgm_note_start(ct_state_t *st, int ch)
{
    const ct_arrange_t *a = st->arr;
    // (idle no longer mutes the counter lane: in the ambient set the counter
    // carries the broken-chord accompaniment, i.e. the piece itself.)
    if (!a || st->wrapped[ch] || (a->mute_mask >> ch) & 1) {
        st->env[ch] = 0;
        return;
    }
    const ct_step_t *tab = a->ch[ch];
    uint16_t n = a->nstep[ch];
    if (!tab || !n) { st->env[ch] = 0; return; }
    const ct_step_t *e = &tab[st->idx[ch]];
    uint16_t sps = step_samples(a, st->idle);
    st->step_len[ch] = (uint16_t)(e->dur * sps);
    st->step_left[ch] = st->step_len[ch];
    st->tick[ch] = 0;
    // advance for the next start (loop, or stop after one pass)
    st->idx[ch]++;
    if (st->idx[ch] >= n) {
        if (a->once) st->wrapped[ch] = 1;
        else st->idx[ch] = 0;
    }
    if (e->note == 0) { st->env[ch] = 0; return; } // rest: the flute needs air
    st->inc[ch] = note_inc(e->note + a->transpose);
    st->phase[ch] = 0;                            // per-note reset = articulation
    s_duty[ch] = (uint32_t)(e->duty ? e->duty : 50) * 65536u / 100u;
    st->env[ch] = (uint16_t)(e->vel * BGM_ENV_UNIT);
}

void ct_render(ct_state_t *st, int16_t *out, int frames)
{
    // NOTE: no early return on !arr — queued SEs must drain even when the
    // BGM is silent (pocket mode / report screen).
    for (int i = 0; i < frames; i++) {
        st->since_se++;
        int32_t acc = 0;

        // ---- BGM tonal channels (lead / counter / bass) ----
        for (int ch = CT_LEAD; ch <= CT_BASS; ch++) {
            if (st->step_left[ch] == 0) bgm_note_start(st, ch);
            if (st->step_left[ch] == 0) continue;  // true silence
            st->step_left[ch]--;                   // time always advances
            if (st->env[ch] == 0) continue;        // rest step
            st->phase[ch] += st->inc[ch];
            // phase is Q16 cycles: the LOW 16 bits are the position inside the
            // cycle. Comparing the high bits (the old code) flipped the pulse
            // once every 65536 cycles -> DC steps at note onsets, no pitch at all.
            acc += ((uint16_t)st->phase[ch] < (uint16_t)s_duty[ch])
                     ? st->env[ch] : -(int32_t)st->env[ch];
            // release tail in the last 40% of the note (flute breath)
            st->tick[ch]++;
            if (st->tick[ch] > (st->step_len[ch] * 3) / 5 &&
                (st->tick[ch] & 31) == 0 && st->env[ch] > 256)
                st->env[ch] = (uint16_t)(st->env[ch] - (st->env[ch] >> 3));
        }

        // ---- BGM noise lane (silent while an SE speaks) ----
        int32_t nval = 0;
        if (!st->se_active) {
            if (st->step_left[CT_NOISE] == 0) bgm_note_start(st, CT_NOISE);
            if (st->env[CT_NOISE] && st->step_left[CT_NOISE]) {
                st->step_left[CT_NOISE]--;
                if (++st->lfsr_div >= 4) { st->lfsr_div = 0; noise_tick(st); }
                nval = (st->lfsr & 1) ? st->env[CT_NOISE] : -(int32_t)st->env[CT_NOISE];
                st->tick[CT_NOISE]++;
                if ((st->tick[CT_NOISE] & 15) == 0 && st->env[CT_NOISE] > 64)
                    st->env[CT_NOISE] = (uint16_t)(st->env[CT_NOISE] - (st->env[CT_NOISE] >> 4));
            }
        }
        acc += nval * 3 / 2;

        // ---- SE voice (steals the noise lane) ----
        int32_t se_val = 0;
        if (st->se_active) {
            if (st->se_left == 0) {
                uint16_t n = 0;
                const ct_step_t *tab = chiptune_se_steps((ct_se_t)st->se_id, &n);
                if (!tab || st->se_step >= n) {
                    st->se_active = 0;         // SE finished: noise lane returns
                } else {
                    const ct_step_t *e = &tab[st->se_step++];
                    // SE timing is independent of the current BGM. One table
                    // step is always 25 ms, so a cue sounds identical in town,
                    // battle, pomodoro and the otherwise-silent standby mode.
                    uint16_t len = (uint16_t)(e->dur * (CT_SR / 40));
                    st->se_len = len < 40 ? 40 : len;
                    st->se_left = st->se_len;
                    st->se_note_is_noise = (e->duty == 0);   // duty 0 = noise SE
                    st->se_phase = 0;
                    if (st->se_note_is_noise)
                        st->se_env = (uint16_t)(e->vel * SE_NOISE_UNIT);
                    else {
                        st->se_inc = note_inc(e->note);
                        st->se_env = (uint16_t)(e->vel * SE_ENV_UNIT);
                        st->se_duty = (uint16_t)((e->duty ? e->duty : 50) * 65536u / 100u);
                    }
                }
            }
            if (st->se_active && st->se_left) {
                st->se_left--;
                if (st->se_note_is_noise) {
                    if (++st->lfsr_div >= 2) { st->lfsr_div = 0; noise_tick(st); }
                    se_val = (st->lfsr & 1) ? st->se_env : -(int32_t)st->se_env;
                    if ((st->se_left & 31) == 0 && st->se_env > 32)
                        st->se_env = (uint16_t)(st->se_env - (st->se_env >> 3));
                } else if (st->se_env) {
                    st->se_phase += st->se_inc;
                    se_val = ((uint16_t)st->se_phase < st->se_duty)
                               ? st->se_env : -(int32_t)st->se_env;
                }
            }
        }

        int32_t out_v = (acc * st->bgm_vol + se_val * st->se_vol) / 100;
        // Tone control: raw pulse waves are all odd harmonics, and the badge's
        // tiny speaker turns 2-6 kHz into a piercing buzz ("太吵、烦躁"). A
        // one-pole low-pass (~600 Hz @16 kHz) rounds every voice off; the
        // music keeps its notes, loses the edge.
        st->lp += ((out_v << 8) - st->lp) >> 2;
        if (out_v == 0 && st->lp > -512 && st->lp < 512) st->lp = 0;  // settle to true silence
        out_v = st->lp >> 8;
        if (out_v > VOL_MAX) out_v = VOL_MAX;
        if (out_v < -VOL_MAX) out_v = -VOL_MAX;
        *out++ = (int16_t)out_v;
    }
}
