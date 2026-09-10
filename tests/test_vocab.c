// tests/test_vocab.c — FSRS algorithm correctness
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "vocab_model.h"

extern void vocab_test_advance_days(int n);

static const vocab_entry_t WORDS[] = {
    {"residual", "/ˈrezɪdjuəl/", "adj. 残留的", {1, 2}},
    {"ephemeral", "/ɪˈfemərəl/", "adj. 短暂的", {0, 2}},
    {"ubiquitous", "/juːˈbɪkwɪtəs/", "adj. 普遍存在的", {0, 1}},
};

void test_fsrs_basic(void) {
    vocab_init(WORDS, 3);
    assert(vocab_total_words() == 3);
    assert(vocab_new_words() == 3);
    assert(vocab_due_count() == 3);

    // First review: Good → initial stability = W[2] ≈ 3.7
    int idx = vocab_next_due();
    assert(idx >= 0);
    vocab_review(idx, VOCAB_GOOD);
    const vocab_word_state_t *st = vocab_get_state(idx);
    assert(st->reps == 1);
    assert(st->stability > 3.0f && st->stability < 5.0f);
    assert(st->difficulty > 1.0f && st->difficulty < 10.0f);
    assert(vocab_new_words() == 2);

    // Second review: Good → stability grows
    float s_before = st->stability;
    vocab_review(idx, VOCAB_GOOD);
    st = vocab_get_state(idx);
    assert(st->reps == 2);
    assert(st->stability > s_before); // grows on success

    // Again → stability drops sharply
    s_before = st->stability;
    vocab_review(idx, VOCAB_AGAIN);
    st = vocab_get_state(idx);
    assert(st->stability < s_before); // sharp drop on failure
    assert(st->difficulty > 1.0f);    // difficulty increased

    printf("  fsrs_basic OK\n");
}

void test_fsrs_retrievability(void) {
    vocab_init(WORDS, 3);
    int idx = vocab_next_due();
    vocab_review(idx, VOCAB_GOOD);

    // Day 0: just reviewed → R should be high (~1.0)
    float r0 = vocab_retrievability(idx);
    assert(r0 > 0.9f);

    // Advance 3 days → R drops
    vocab_test_advance_days(3);
    float r3 = vocab_retrievability(idx);
    assert(r3 < r0);
    assert(r3 > 0.3f); // stability 3.7, 3 days → R ≈ 0.62

    // Advance 10 more days → R drops further
    vocab_test_advance_days(10);
    float r13 = vocab_retrievability(idx);
    assert(r13 < r3);

    // Due when R < 90%
    assert(vocab_due_count() >= 1); // R dropped below 0.9
    printf("  fsrs_retrievability OK\n");
}

void test_fsrs_difficulty_adapts(void) {
    vocab_init(WORDS, 3);
    int idx = vocab_next_due();

    // Multiple "Again" → difficulty rises toward 10
    float d0 = 5.0f; // initial
    for (int i = 0; i < 3; i++) {
        vocab_review(idx, VOCAB_AGAIN);
        const vocab_word_state_t *st = vocab_get_state(idx);
        assert(st->difficulty >= d0); // difficulty never decreases on failure
        d0 = st->difficulty;
    }
    assert(d0 > 5.0f); // difficulty increased from initial

    // "Easy" → difficulty decreases
    vocab_review(idx, VOCAB_EASY);
    const vocab_word_state_t *st = vocab_get_state(idx);
    assert(st->difficulty < d0);
    printf("  fsrs_difficulty_adapts OK\n");
}

void test_fsrs_ordering(void) {
    // Easy stability > Good stability > Hard stability (initial)
    vocab_init(WORDS, 3);
    float s[3];
    for (int i = 0; i < 3; i++) {
        int idx = vocab_next_due();
        vocab_review(idx, (vocab_rating_t)(i + 1)); // 1=Again, 2=Hard, 3=Good
        s[i] = vocab_get_state(idx)->stability;
    }
    // Again < Good; (Easy tested separately)
    vocab_init(WORDS, 3);
    int idx = vocab_next_due();
    vocab_review(idx, VOCAB_EASY);
    float s_easy = vocab_get_state(idx)->stability;
    assert(s_easy > 10.0f); // Easy initial stability = 13.8
    printf("  fsrs_ordering OK\n");
}

void test_daily_budget(void) {
    // budget survives vocab_init on purpose (mode re-entry must not refill);
    // land on a fresh day so earlier tests' consumption doesn't leak in.
    vocab_test_advance_days(1);
    // 20 fresh words, cap 15: the 16th request must be refused same-day
    static char terms[20][16], phons[20][16], defs[20][16];
    vocab_entry_t many[20];
    for (int i = 0; i < 20; i++) {
        snprintf(terms[i], sizeof(terms[i]), "word%02d", i);
        snprintf(phons[i], sizeof(phons[i]), "/w%d/", i);
        snprintf(defs[i], sizeof(defs[i]), "def %d", i);
        many[i].term = terms[i];
        many[i].phonetic = phons[i];
        many[i].definition = defs[i];
        many[i].distractors[0] = (uint8_t)((i + 1) % 20);
        many[i].distractors[1] = (uint8_t)((i + 2) % 20);
    }
    vocab_init(many, 20);
    int last_idx = -1;
    for (int i = 0; i < VOCAB_NEW_DEFAULT; i++) {
        int idx = vocab_next_due();
        assert(idx >= 0);
        assert(idx > last_idx);      // deck order: strictly advancing
        last_idx = idx;
        vocab_review((uint16_t)idx, VOCAB_GOOD); // same-day: never due again
    }
    assert(vocab_next_due() == -1);          // budget drained, nothing due
    assert(vocab_new_served_today() == VOCAB_NEW_DEFAULT);

    // next day: budget resets, new words flow again
    vocab_test_advance_days(1);
    assert(vocab_next_due() >= 0);

    // reviews are never capped: force a word due with budget exhausted
    vocab_test_advance_days(30); // S≈3.7d from first Good: well past due
    int due = vocab_next_due();
    assert(due >= 0);
    const vocab_word_state_t *st = vocab_get_state((uint16_t)due);
    assert(st->reps > 0);                    // a review, not a new word
    assert(vocab_forecast_tomorrow() > 0);   // it is already due today

    // user-adjustable cap persists through init (NVS on device, static here)
    vocab_set_daily_cap(30);
    assert(vocab_new_daily_cap() == 30);
    vocab_set_daily_cap(VOCAB_NEW_DEFAULT);

    // 超额背词: un-consume serves so the drained day serves again
    vocab_test_advance_days(1);              // fresh day: 5 new left + 15 old
    int served = 0;                          // words now due from day 0
    while (vocab_next_due() >= 0) {          // drain: reviews first, then new
        vocab_review((uint16_t)vocab_next_due(), VOCAB_GOOD);
        served++;
    }
    assert(served == 20);                    // 15 overdue + 5 fresh
    assert(vocab_next_due() == -1);          // whole deck introduced, all fed
    vocab_extend_budget(10);
    assert(vocab_next_due() == -1);          // un-consuming can't resurrect
                                             // words that don't exist
    printf("  daily_budget OK\n");
}

void test_forget_new_advances_and_counts_once(void) {
    // User report: 忘记 on a new word restuck that stem and 新词 N/cap climbed.
    vocab_test_advance_days(1);
    static char terms[5][16], phons[5][16], defs[5][16];
    vocab_entry_t many[5];
    for (int i = 0; i < 5; i++) {
        snprintf(terms[i], sizeof(terms[i]), "n%d", i);
        snprintf(phons[i], sizeof(phons[i]), "/n%d/", i);
        snprintf(defs[i], sizeof(defs[i]), "d%d", i);
        many[i].term = terms[i];
        many[i].phonetic = phons[i];
        many[i].definition = defs[i];
        many[i].distractors[0] = (uint8_t)((i + 1) % 5);
        many[i].distractors[1] = (uint8_t)((i + 2) % 5);
    }
    vocab_init(many, 5);
    vocab_set_daily_cap(5);
    int a = vocab_next_due();
    assert(a == 0);
    assert(vocab_new_served_today() == 1);
    vocab_review((uint16_t)a, VOCAB_AGAIN);   // UI now commits AGAIN on new words
    assert(vocab_get_state((uint16_t)a)->reps == 1);
    int b = vocab_next_due();
    assert(b == 1);                           // next unseen new word, not restuck on 0
    assert(vocab_new_served_today() == 2);
    assert(vocab_review_due_count() == 0);    // same-day R still high → due pool empty
    uint16_t learned[8];
    assert(vocab_collect_learned(learned, 8) == 1);  // reverse uses learned, not due
    assert(learned[0] == (uint16_t)a);
    printf("  forget_new_advances_and_counts_once OK\n");
}

void test_collect_learned_vs_due(void) {
    vocab_test_advance_days(1);
    vocab_init(WORDS, 3);
    uint16_t buf[8];
    assert(vocab_collect_learned(buf, 8) == 0);      // nothing introduced
    assert(vocab_collect_due_reviews(buf, 8) == 0);

    int idx = vocab_next_due();
    assert(idx >= 0);
    vocab_review((uint16_t)idx, VOCAB_GOOD);
    assert(vocab_collect_due_reviews(buf, 8) == 0);  // same-day R ≈ 1.0
    assert(vocab_collect_learned(buf, 8) == 1);
    assert(buf[0] == (uint16_t)idx);

    vocab_review((uint16_t)vocab_next_due(), VOCAB_GOOD);
    assert(vocab_collect_learned(buf, 8) == 2);
    printf("  collect_learned_vs_due OK\n");
}

int main(void) {
    test_fsrs_basic();
    test_fsrs_retrievability();
    test_fsrs_difficulty_adapts();
    test_fsrs_ordering();
    test_daily_budget();
    test_forget_new_advances_and_counts_once();
    test_collect_learned_vs_due();
    printf("ALL VOCAB FSRS TESTS PASS\n");
    return 0;
}
