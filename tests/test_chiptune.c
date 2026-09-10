// tests/test_chiptune.c —— host render tests for the chiptune engine.
// Build: cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_chiptune.c
//        main/chiptune.c main/chiptune_songs.c
// The engine is deterministic: same state -> same samples.
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "chiptune.h"

static ct_state_t fresh(ct_scene_t scene)
{
    ct_state_t st;
    ct_init(&st);
    ct_set_arrange(&st, chiptune_scene_arrange(scene));
    return st;
}

static void test_determinism(void)
{
    ct_state_t a = fresh(CT_SCENE_TOWN), b = fresh(CT_SCENE_TOWN);
    int16_t ba[480], bb[480];
    ct_render(&a, ba, 480);
    ct_render(&b, bb, 480);
    assert(memcmp(ba, bb, sizeof ba) == 0);
    printf("chiptune_determinism OK\n");
}

static void test_loop_wrap(void)
{
    // battle is a 2-bar loop: pass 1 and pass 2 must be identical
    // town: no noise lane, so sample-exact looping is assertable (the battle
    // track has a continuous LFSR - its noise never repeats exactly, by design)
    ct_state_t st = fresh(CT_SCENE_TOWN);
    const ct_arrange_t *a = chiptune_scene_arrange(CT_SCENE_TOWN);
    int spb = (60 * CT_SR + (a->bpm * a->steps_per_bar) / 2) / (a->bpm * a->steps_per_bar);
    int whole = spb * a->bars * a->steps_per_bar;
    assert(whole <= 160000);
    static int16_t first[160000], second[160000];
    ct_render(&st, first, whole);
    ct_render(&st, second, whole);
    // The output low-pass carries a little state across the loop point, so
    // compare with a 2-LSB tolerance after the first 64 samples settle.
    for (int i = 64; i < whole; i++) {
        int d = first[i] - second[i];
        assert(d >= -2 && d <= 2);
    }
    printf("chiptune_loop OK\n");
}

static void test_once_mode_ends_silent(void)
{
    ct_state_t st = fresh(CT_SCENE_FANFARE);
    int16_t buf[CT_SR];   // 1 s: fanfare is 4 bars at 140bpm (6.8s total)
    ct_render(&st, buf, CT_SR);
    st.arr = chiptune_scene_arrange(CT_SCENE_FANFARE);
    // render far past the end: once-mode must go silent, not loop
    int16_t tail[CT_SR / 2];
    for (int i = 0; i < 12; i++) ct_render(&st, buf, CT_SR);
    ct_render(&st, tail, sizeof tail / 2);
    for (size_t i = 0; i < sizeof tail / 2; i++) {
        if (tail[i] != 0) { assert(0); }
    }
    printf("chiptune_once OK\n");
}

static void test_se_rate_limit(void)
{
    ct_state_t c = fresh(CT_SCENE_TOWN);
    ct_se(&c, CT_SE_HIT);
    ct_state_t d = fresh(CT_SCENE_TOWN);
    ct_se(&d, CT_SE_HIT);
    ct_se(&d, CT_SE_HIT);   // inside the 8 Hz window: ignored
    int16_t x[960], y[960];
    ct_render(&c, x, 960);
    ct_render(&d, y, 960);
    assert(memcmp(x, y, sizeof x) == 0);   // second SE was a no-op
    printf("chiptune_rate_limit OK\n");
}

static void test_se_drains_in_silence(void)
{
    ct_state_t st;
    ct_init(&st);          // no arrange at all: pure silence
    ct_se(&st, CT_SE_OK);
    int16_t buf[480];
    ct_render(&st, buf, 480);
    int non_zero = 0;
    for (int i = 0; i < 480; i++)
        if (buf[i] != 0) non_zero++;
    assert(non_zero > 100);   // queued SEs still play when BGM is silent
    printf("chiptune_se_silence OK\n");
}

static int render_se_length(ct_scene_t scene, ct_se_t se)
{
    ct_state_t st = fresh(scene);
    ct_se(&st, se);
    int16_t sample;
    int frames = 0;
    while (st.se_active && frames < CT_SR * 3) {
        ct_render(&st, &sample, 1);
        frames++;
    }
    assert(!st.se_active);
    return frames;
}

static void test_se_timing_independent_of_scene(void)
{
    int silent = render_se_length(CT_SCENE_NONE, CT_SE_POMO_FOCUS);
    assert(silent == render_se_length(CT_SCENE_TOWN, CT_SE_POMO_FOCUS));
    assert(silent == render_se_length(CT_SCENE_BATTLE, CT_SE_POMO_FOCUS));
    // Three 150 ms notes plus two 50 ms rests, plus the final drain sample.
    assert(silent >= 8800 && silent <= 8810);
    printf("chiptune_se_timing OK (%d frames)\n", silent);
}

static void test_no_clip(void)
{
    ct_state_t st = fresh(CT_SCENE_BATTLE);
    ct_se(&st, CT_SE_HIT);
    int16_t buf[CT_SR];
    ct_render(&st, buf, CT_SR);
    for (int i = 0; i < CT_SR; i++)
        assert(buf[i] <= 32000 && buf[i] >= -32000);
    printf("chiptune_no_clip OK\n");
}

static void test_song_budget(void)
{
    // Every cue resolves and respects the badge-speaker articulation contract:
    // a 25 ms sequencer step and no individual tone longer than 150 ms.
    uint16_t n;
    for (int se = 0; se < CT_SE_N; se++) {
        const ct_step_t *steps = chiptune_se_steps((ct_se_t)se, &n);
        assert(steps != 0 && n > 0);
        for (uint16_t i = 0; i < n; i++)
            assert(steps[i].dur > 0 && steps[i].dur <= 6);
    }
    // every scene must resolve to an arrange with 4 channels
    for (int sc = 1; sc < CT_SCENE_N; sc++) {
        const ct_arrange_t *a = chiptune_scene_arrange((ct_scene_t)sc);
        assert(a);
        for (int c = 0; c < CT_CH_N; c++)
            assert(a->ch[c] && a->nstep[c] > 0);
    }
    printf("chiptune_tables OK\n");
}


// Gain staging guard (device fix 2026-09-05): a full arrangement at the default
// volumes must never touch the clip ceiling — the badge amp cuts out on hard
// clipped square waves and the device played clicks instead of music.
static void test_headroom(void)
{
    static ct_state_t st;
    static int16_t buf[CT_SR];
    for (int scene = CT_SCENE_TOWN; scene < CT_SCENE_N; scene++) {
        ct_init(&st);
        ct_set_arrange(&st, chiptune_scene_arrange((ct_scene_t)scene));
        int clipped = 0, peak = 0;
        for (int sec = 0; sec < 3; sec++) {
            ct_render(&st, buf, CT_SR);
            for (int i = 0; i < CT_SR; i++) {
                int v = buf[i] < 0 ? -buf[i] : buf[i];
                if (v >= 31000) clipped++;
                if (v > peak) peak = v;
            }
        }
        assert(clipped == 0);
        assert(peak <= 16000);   // <= -6 dBFS: SE on top still has room
    }
    printf("headroom OK\n");
}


// Pitch guard (device fix 2026-09-05 #2): the oscillator must actually
// oscillate. A lone A4 (midi 69, 50% duty) at 16 kHz crosses zero ~880x/s.
// Before the fix the pulse compared the wrong phase bits and every note was
// a DC step: clicks on the badge, no music.
static void test_pitch(void)
{
    static const ct_step_t lead[] = {{69, 64, 50, 12}};
    static const ct_arrange_t arr = {
        .bars = 1, .steps_per_bar = 4, .transpose = 0, .mute_mask = 0x0E,
        .bpm = 120, .once = 1,
        .ch = {lead, NULL, NULL, NULL}, .nstep = {1, 0, 0, 0},
    };
    static ct_state_t st;
    static int16_t buf[CT_SR];
    ct_init(&st);
    ct_set_arrange(&st, &arr);
    ct_render(&st, buf, CT_SR);
    int zc = 0;
    for (int i = 1; i < CT_SR; i++)
        if ((buf[i - 1] < 0) != (buf[i] < 0)) zc++;
    printf("pitch: %d zero crossings/s (A4 expects ~880)\n", zc);
    assert(zc > 840 && zc < 920);
}

int main(void)
{
    test_determinism();
    test_loop_wrap();
    test_once_mode_ends_silent();
    test_se_rate_limit();
    test_se_drains_in_silence();
    test_se_timing_independent_of_scene();
    test_no_clip();
    test_song_budget();
    test_headroom();
    test_pitch();
    printf("ALL CHIPTUNE TESTS PASS\n");
    return 0;
}
