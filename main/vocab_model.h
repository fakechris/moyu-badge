// vocab_model.h — FSRS (Free Spaced Repetition Scheduler) for DeskPet vocab.
//
// Based on FSRS-4.5 (Three Component Model of Memory):
//   R = retrievability (probability of recall), decays as power curve
//   S = stability (days until R drops to 90%)
//   D = difficulty (1-10, per-word, adapts to user performance)
//
// Per-word state: 12 bytes (stability f32, difficulty f32, last_day u16, reps u8)
// Global parameters: 17 floats (pre-trained FSRS-4.5 defaults, not user-trained)
//
// References:
//   FSRS-4.5 algorithm: https://github.com/open-spaced-repetition/fsrs4anki
//   FSRS in 100 lines:  https://borretti.me/article/implementing-fsrs-in-100-lines
//   Benchmark: FSRS > SM-2 for 99.6% users, 20-30% fewer reviews
#ifndef VOCAB_MODEL_H
#define VOCAB_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define VOCAB_MAX_WORDS 500
#define VOCAB_NVS_NS    "vocab"

// Rating buttons (mapped to 3 physical buttons)
typedef enum {
    VOCAB_AGAIN = 1,   // UP: didn't remember (r < 60%)
    VOCAB_HARD  = 2,   // OK long-press: barely remembered
    VOCAB_GOOD  = 3,   // OK: remembered
    VOCAB_EASY  = 4,   // DOWN: remembered easily
} vocab_rating_t;

// Per-word FSRS state (persisted in NVS, 12 bytes)
typedef struct {
    float stability;      // days until 90% recall
    float difficulty;     // 1.0-10.0
    uint16_t last_day;    // day number of last review
    uint8_t reps;         // total review count
    uint8_t state;        // 0=new, 1=learning, 2=review, 3=relearning
} vocab_word_state_t;

// Word content (read-only, in flash)
typedef struct {
    const char *term;
    const char *phonetic;
    const char *definition;
    // Precomputed confusables for the reverse multiple-choice mode (deck
    // indices; ranked at import time by edit distance, shared definition
    // characters, POS match, deck adjacency — competitive distractors are
    // what makes multiple choice ≈ recall, Little & Bjork 2016).
    uint16_t distractors[2];    // uint8 wraps at 256 entries — deck is 500
} vocab_entry_t;

// --- Core API ---

void vocab_init(const vocab_entry_t *entries, uint16_t count);

// Number of words with retrievability < 90% (due for review)
uint16_t vocab_due_count(void);

// Get next word index with R < 90% (or -1 if none due)
int vocab_next_due(void);

// Get predicted retrievability for a word (0.0-1.0)
float vocab_retrievability(uint16_t idx);

// Accessors
const vocab_entry_t *vocab_get_entry(uint16_t idx);
const vocab_word_state_t *vocab_get_state(uint16_t idx);

// Submit a review: updates FSRS state, persists to NVS
void vocab_review(uint16_t idx, vocab_rating_t rating);

// Day number
uint16_t vocab_day_number(void);

// Stats
uint16_t vocab_total_words(void);
uint16_t vocab_new_words(void);
uint16_t vocab_learning_words(void);
uint16_t vocab_mastered_words(void); // stability >= 21 days

// Daily new-word budget (product layer, aligned with Anki new/day + Maimemo
// 学习量): caps first-pass exposure; reviews are never capped. The cap is a
// user setting (5/10/15/20/30, default 15) persisted in NVS, and the user can
// extend today's budget post-session ("超额背词") by un-consuming serves.
#define VOCAB_NEW_DEFAULT 15
uint16_t vocab_new_served_today(void);
uint16_t vocab_new_daily_cap(void);          // current setting
void vocab_set_daily_cap(uint16_t cap);      // persists
void vocab_extend_budget(uint16_t n);        // un-consume n serves (超额)
// Words already introduced (reps>0) whose retrievability dropped below 90%.
uint16_t vocab_review_due_count(void);
// Words shown at least once (reps > 0) — the "learned" counter that moves
// as the user rates, unlike review-due (same-day it stays 0 by design).
uint16_t vocab_introduced_words(void);
// Words predicted to drop below 90% retrievability within the next day —
// the "明日预计复习" figure shown in the post-session menu.
uint16_t vocab_forecast_tomorrow(void);

// Snapshot of due REVIEWS only (reps>0, R<0.9), lowest retrievability first.
// The reverse-practice sessions train production on known words exclusively
// (industry rule: new words recognize first, produce later). Returns count.
uint16_t vocab_collect_due_reviews(uint16_t *out, uint16_t max);

#endif // VOCAB_MODEL_H
