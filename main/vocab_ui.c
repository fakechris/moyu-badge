// vocab_ui.c — FSRS flashcard UI, product layer aligned with Anki/Maimemo:
//   * forward: EN word+IPA front, CN definition behind; three-tier rating
//     UP=忘记 / OK=模糊 / DOWN=认识 (Maimemo's three-button mapping)
//   * session requeue (Anki learning steps): 忘记 words re-appear at the
//     back of today's queue until answered 模糊/认识; lapses on committed
//     words record AGAIN immediately
//   * post-quota MENU: 超额再背 / 每日新词数 / reverse sessions. Reverse
//     practice is production training on the learned pool (reps>0), not the
//     due-review subset (Webb 2005: knowledge is direction-specific), in two flavors:
//     recall (see CN -> think EN -> flip -> self-rate) and 3-choice (CN ->
//     pick EN among precomputed confusables — Little & Bjork 2016: only
//     competitive distractors make MC ~= recall).
//   * modes never auto-switch: OK-long switcher is the only exit
// Chinese text and dictionary IPA render through zh_subset (PingFang) with
// zh_ipa_fix wired as the LVGL fallback for glyphs PingFang lacks.
#include "vocab_ui.h"
#include <stdio.h>
#include <string.h>
#include "chiptune.h"
#include "deskpet_i18n.h"

extern const lv_font_t zh_subset;

#define RQ_MAX 64            // session requeue ring capacity
#define EXTEND_STEP 10       // 超额再背 batch size
#define MENU_ROWS 4

typedef enum {
    ST_STUDY = 0, ST_MENU, ST_REV_RECALL, ST_REV_CHOICE, ST_REV_CHOICE_ANSWER,
} ui_state_t;

static lv_obj_t *s_scr;          // container
static lv_obj_t *s_stat_lbl;     // stats line
static lv_obj_t *s_term_lbl;     // word / menu / CN prompt
static lv_obj_t *s_phon_lbl;     // phonetic / forecast
static lv_obj_t *s_def_lbl;      // definition / choice candidates
static lv_obj_t *s_hint_lbl;     // per-state hint

static ui_state_t s_state;
static bool s_flipped;
static uint16_t s_current;
static int s_done_count;         // ratings this session
static bool s_initialized;
static uint8_t s_menu_row;
static uint8_t s_choice_answer;  // 0..2 button index of the right option

// Phase-aware status line: the header leads with the CURRENT stage and shows
// remaining work, so progress is readable instead of freezing on a stale
// "新词 15/15" (user-reported).
typedef enum { PK_REVIEW = 0, PK_NEW, PK_DRILL } phase_kind_t;
static phase_kind_t s_cur_kind = PK_REVIEW;
static int s_rev_total;          // due-review snapshot at session start
static int s_rev_done;           // first-sight review cards rated this session

// reverse session: ranked learned snapshot; UI windows VOCAB_REV_BATCH
// via s_rev_cursor. Does not drain the forward drill ring.
static uint16_t s_rev_q[64];
static uint16_t s_rev_n;
static uint16_t s_rev_i;
static uint16_t s_rev_cursor; // next reverse window into the learned pool
static uint16_t s_rq[RQ_MAX];
static uint8_t s_rq_head, s_rq_tail;
static uint8_t s_seen[64];       // bit per word: served this app run
_Static_assert(VOCAB_MAX_WORDS <= (int)(sizeof(s_seen) * 8),
               "s_seen bitmap smaller than VOCAB_MAX_WORDS");

static void show_front(void);
static void show_answer(void);
static void show_menu(void);
static void refresh_stats(void);
static void rev_show_prompt(void);
static void rev_show_answer(void);
static void rev_show_choice(void);
static void rev_start(ui_state_t mode);

static inline bool seen_get(uint16_t idx) { return (s_seen[idx >> 3] >> (idx & 7)) & 1; }
static inline void seen_set(uint16_t idx) { s_seen[idx >> 3] |= (uint8_t)(1u << (idx & 7)); }

static void rq_push(uint16_t idx)
{
    for (uint8_t i = s_rq_head; i != s_rq_tail; i = (uint8_t)((i + 1) % RQ_MAX)) {
        if (s_rq[i] == idx) return;          // already queued this session
    }
    uint8_t next = (uint8_t)((s_rq_tail + 1) % RQ_MAX);
    if (next == s_rq_head) return;           // full
    s_rq[s_rq_tail] = idx;
    s_rq_tail = next;
}

static bool rq_pop(uint16_t *idx)
{
    if (s_rq_head == s_rq_tail) return false;
    *idx = s_rq[s_rq_head];
    s_rq_head = (uint8_t)(s_rq_head + 1) % RQ_MAX;
    return true;
}

static void set_fonts_cjk(bool cjk)
{
    lv_obj_set_style_text_font(s_term_lbl, cjk ? &zh_subset
                                               : &lv_font_montserrat_20, 0);
}

static void show_front(void)
{
    s_flipped = false;
    const vocab_entry_t *e = vocab_get_entry(s_current);
    if (!e) return;
    set_fonts_cjk(false);
    lv_label_set_text(s_term_lbl, e->term);
    lv_label_set_text(s_phon_lbl, e->phonetic);
    lv_obj_add_flag(s_def_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_FLIP, deskpet_get_lang()));
}

static void show_answer(void)
{
    s_flipped = true;
    const vocab_entry_t *e = vocab_get_entry(s_current);
    if (!e) return;
    lv_label_set_text(s_def_lbl, e->definition);
    lv_obj_clear_flag(s_def_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_RATE, deskpet_get_lang()));
}

// ---- reverse sessions (production training on known words) ----

static bool rev_next(void)
{
    if (s_rev_i < s_rev_n) {
        s_current = s_rev_q[s_rev_i++];
        return true;
    }
    return false;                // reverse is snapshot-only; never drain s_rq
}

static void rev_show_prompt(void)
{
    s_flipped = false;
    const vocab_entry_t *e = vocab_get_entry(s_current);
    if (!e) { show_menu(); return; }
    set_fonts_cjk(true);
    lv_label_set_text(s_term_lbl, e->definition);
    lv_label_set_text(s_phon_lbl, "");
    lv_obj_add_flag(s_def_lbl, LV_OBJ_FLAG_HIDDEN);
    if (s_state == ST_REV_RECALL)
        lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_REV_HINT, deskpet_get_lang()));
    else
        lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_RATE, deskpet_get_lang()));
}

static void rev_show_answer(void)
{
    s_flipped = true;
    const vocab_entry_t *e = vocab_get_entry(s_current);
    if (!e) return;
    set_fonts_cjk(false);
    lv_label_set_text(s_term_lbl, e->term);
    lv_label_set_text(s_phon_lbl, e->phonetic);
    lv_label_set_text(s_def_lbl, e->definition);
    lv_obj_clear_flag(s_def_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_NEXT, deskpet_get_lang()));
}

static void rev_start(ui_state_t mode)
{
    s_state = mode;
    uint16_t nall = vocab_collect_learned(s_rev_q, 64);
    s_rev_i = 0;
    if (nall == 0) {
        audio_se(CT_SE_WARN);
        show_menu();
        lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_REV_EMPTY, deskpet_get_lang()));
        return;
    }
    if (s_rev_cursor >= nall) s_rev_cursor = 0;
    uint16_t take = nall < VOCAB_REV_BATCH ? nall : VOCAB_REV_BATCH;
    uint16_t window[VOCAB_REV_BATCH];
    for (uint16_t i = 0; i < take; i++)
        window[i] = s_rev_q[(s_rev_cursor + i) % nall];
    memcpy(s_rev_q, window, take * sizeof(uint16_t));
    s_rev_n = take;
    s_rev_cursor = (uint16_t)((s_rev_cursor + take) % nall);
    rev_next();
    if (s_state == ST_REV_RECALL) rev_show_prompt();
    else rev_show_choice();
    refresh_stats();
}

// deterministic 3-way shuffle of target + its two distractors
static void rev_show_choice(void)
{
    const vocab_entry_t *e = vocab_get_entry(s_current);
    if (!e) { show_menu(); return; }
    s_flipped = false;
    uint32_t rng = (uint32_t)s_current * 2654435761u + (uint32_t)vocab_day_number();
    rng ^= rng >> 15; rng *= 2246822519u; rng ^= rng >> 13;
    uint8_t pos = (uint8_t)(rng % 3);            // answer slot
    s_choice_answer = pos;
    const char *opts[3];
    opts[pos] = e->term;
    const char *others[2] = {
        vocab_get_entry(e->distractors[0])->term,
        vocab_get_entry(e->distractors[1])->term,
    };
    opts[(pos + 1) % 3] = others[0];
    opts[(pos + 2) % 3] = others[1];
    set_fonts_cjk(true);
    lv_label_set_text(s_term_lbl, e->definition);
    lv_label_set_text(s_phon_lbl, "");
    char buf[128];
    snprintf(buf, sizeof(buf), "1. %-14s 2. %-14s 3. %s", opts[0], opts[1], opts[2]);
    lv_label_set_text(s_def_lbl, buf);
    lv_obj_clear_flag(s_def_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_PICK, deskpet_get_lang()));
}

static void show_menu(void)
{
    s_state = ST_MENU;
    s_current = (uint16_t)-1;
    static const deskpet_str_id_t ROW_KEY[MENU_ROWS] = {
        S_VOCAB_EXTRA, S_VOCAB_CAP, S_VOCAB_REV_RECALL, S_VOCAB_REV_CHOICE
    };
    char rows[MENU_ROWS][48];
    char buf[256];
    int off = 0;
    for (int r = 0; r < MENU_ROWS; r++) {
        const char *txt;
        if (r == 0) txt = deskpet_tr(S_VOCAB_EXTRA, deskpet_get_lang());
        else if (r == 1) {
            snprintf(rows[r], sizeof(rows[r]), "%s: %d",
                     deskpet_tr(S_VOCAB_CAP, deskpet_get_lang()),
                     vocab_new_daily_cap());
            txt = rows[r];
        } else txt = deskpet_tr(ROW_KEY[r], deskpet_get_lang());
        off += snprintf(buf + off, sizeof(buf) - (size_t)off, "%c %s\n",
                        r == s_menu_row ? '>' : ' ', txt);
    }
    off += snprintf(buf + off, sizeof(buf) - (size_t)off, "%s %d\n%s ~%d",
                    deskpet_tr(S_VOCAB_INTRO, deskpet_get_lang()),
                    vocab_introduced_words(),
                    deskpet_tr(S_VOCAB_TOMORROW, deskpet_get_lang()),
                    vocab_forecast_tomorrow());
    set_fonts_cjk(true);
    lv_label_set_text(s_term_lbl, buf);
    lv_label_set_text(s_phon_lbl, "");
    lv_label_set_text(s_def_lbl, "");
    lv_obj_add_flag(s_def_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_hint_lbl, deskpet_tr(S_VOCAB_EXIT, deskpet_get_lang()));
    refresh_stats();             // drop leftover「反向 10/10」after a session
}

static void refresh_stats(void)
{
    if (s_state == ST_REV_RECALL || s_state == ST_REV_CHOICE
        || s_state == ST_REV_CHOICE_ANSWER) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s %d/%d",
                 deskpet_tr(S_VOCAB_REVERSE, deskpet_get_lang()),
                 s_rev_i, s_rev_n);
        lv_label_set_text(s_stat_lbl, buf);
        return;
    }
    int rev_done = s_rev_done < s_rev_total ? s_rev_done : s_rev_total;
    int rev_total = s_rev_total > s_rev_done ? s_rev_total : s_rev_done;
    char rev[24], nu[24], drill[24];
    snprintf(rev, sizeof(rev), "%s %d/%d",
             deskpet_tr(S_VOCAB_REVIEW, deskpet_get_lang()), rev_done, rev_total);
    snprintf(nu, sizeof(nu), "%s %d/%d",
             deskpet_tr(S_VOCAB_NEW, deskpet_get_lang()),
             vocab_new_served_today(), vocab_new_daily_cap());
    drill[0] = 0;
    if (s_rq_head != s_rq_tail)
        snprintf(drill, sizeof(drill), "%s %d",
                 deskpet_tr(S_VOCAB_REDRILL, deskpet_get_lang()),
                 (int)((s_rq_tail - s_rq_head + RQ_MAX) % RQ_MAX));
    char buf[96];   // current stage leads; every segment shows remaining work
    if (s_cur_kind == PK_DRILL && drill[0])
        snprintf(buf, sizeof(buf), "%s · %s · %s", drill, rev, nu);
    else if (s_cur_kind == PK_NEW)
        snprintf(buf, sizeof(buf), "%s · %s%s%s", nu, rev,
                 drill[0] ? " · " : "", drill);
    else
        snprintf(buf, sizeof(buf), "%s · %s%s%s", rev, nu,
                 drill[0] ? " · " : "", drill);
    lv_label_set_text(s_stat_lbl, buf);
}

static void next_card(void)
{
    int next = vocab_next_due();
    if (next < 0) {
        uint16_t drill;
        if (rq_pop(&drill)) {
            s_current = drill;
            s_cur_kind = PK_DRILL;
            show_front();
            return;
        }
        show_menu();
        return;
    }
    s_current = (uint16_t)next;
    s_cur_kind = (vocab_get_state((uint16_t)next)->reps == 0) ? PK_NEW : PK_REVIEW;
    show_front();
}

static const uint8_t CAP_STEPS[] = {5, 10, 15, 20, 30};

static uint16_t cap_cycle(uint16_t cap)
{
    for (size_t i = 0; i < sizeof(CAP_STEPS); i++) {
        if (CAP_STEPS[i] == cap)
            return CAP_STEPS[(i + 1) % (sizeof(CAP_STEPS) / sizeof(CAP_STEPS[0]))];
        if (CAP_STEPS[i] > cap)     // off-grid cap: snap up to next step
            return CAP_STEPS[i];
    }
    return CAP_STEPS[0];
}

// commit rules for a rating on word idx:
//   first sight (or still-new reps==0): always write. 忘记 on first sight
//   also writes, so the stem enters learning. Requeue is drill-only:
//   first_sight is false and reps>=1, so HARD/GOOD do not "graduate".
static void rate_commit(uint16_t idx, vocab_rating_t rating, bool first_sight)
{
    uint8_t reps = vocab_get_state(idx)->reps;
    if (rating == VOCAB_AGAIN) {
        // First sight: commit AGAIN even for new words so they enter
        // learning (reps>0). Otherwise next_new_word() keeps returning the
        // same stem and increments today's new-word counter every pass.
        if (first_sight) vocab_review(idx, VOCAB_AGAIN);
        return;
    }
    if (first_sight || reps == 0) vocab_review(idx, rating);
}

// Reverse: miss always lapses. Success writes only if this run has not
// already rated the stem (no same-day double promote). 3-choice hits are
// capped at HARD — cued recognition is not uncued recall.
static void rev_rate(uint16_t idx, vocab_rating_t rating)
{
    bool already = seen_get(idx);
    seen_set(idx);
    if (rating == VOCAB_AGAIN || !already)
        rate_commit(idx, rating, true);
}

// btn: 0=忘记(UP) 1=模糊/confirm(OK) 2=认识/pick(DOWN)
void vocab_ui_button(int btn)
{
    if (!s_initialized) return;

    if (s_state == ST_MENU) {
        if (btn == 0) s_menu_row = (uint8_t)((s_menu_row + MENU_ROWS - 1) % MENU_ROWS);
        else if (btn == 2) s_menu_row = (uint8_t)((s_menu_row + 1) % MENU_ROWS);
        else if (btn == 1) {
            if (s_menu_row == 0) {          // 超额再背 -> straight back to study
                audio_se(CT_SE_OK);
                vocab_extend_budget(EXTEND_STEP);
                s_state = ST_STUDY;
                next_card();
                refresh_stats();
                return;
            }
            if (s_menu_row == 1) {          // cycle daily cap
                vocab_set_daily_cap(cap_cycle(vocab_new_daily_cap()));
                // Raising the cap used to leave the user on the menu with
                // leftover budget; they had to pick 超额再背 (fixed +10, not
                // cap-linked). Resume study when today's serve is still
                // under the new cap — lowering 30→5 stays on the menu.
                if (vocab_new_served_today() < vocab_new_daily_cap()) {
                    audio_se(CT_SE_OK);
                    s_state = ST_STUDY;
                    next_card();
                    refresh_stats();
                    return;
                }
            }
            else if (s_menu_row == 2) { audio_se(CT_SE_OK); rev_start(ST_REV_RECALL); return; }
            else { audio_se(CT_SE_OK); rev_start(ST_REV_CHOICE); return; }
        }
        audio_se(CT_SE_OK);
        show_menu();
        return;
    }

    if (s_state == ST_REV_RECALL) {
        uint16_t idx = s_current;
        if (!s_flipped) {
            if (btn == 1) { audio_se(CT_SE_OK); rev_show_answer(); }
            return;
        }
        rev_rate(idx, btn == 0 ? VOCAB_AGAIN : (btn == 1 ? VOCAB_HARD : VOCAB_GOOD));
        // One pass: 忘记 still lapses FSRS, but does not requeue. Reverse
        // used the forward drill ring and never drained (device: 没完没了).
        audio_se(CT_SE_OK);
        s_done_count++;
        if (rev_next()) {
            rev_show_prompt();
            refresh_stats();
        } else {
            show_menu();
        }
        return;
    }

    if (s_state == ST_REV_CHOICE) {
        uint16_t idx = s_current;
        bool right = ((uint8_t)btn == s_choice_answer);
        rev_rate(idx, right ? VOCAB_HARD : VOCAB_AGAIN);
        audio_se(CT_SE_OK);
        s_done_count++;
        refresh_stats();
        rev_show_answer();                  // always show EN + IPA + def
        s_state = ST_REV_CHOICE_ANSWER;
        return;
    }

    if (s_state == ST_REV_CHOICE_ANSWER) {
        // any button dismisses the verdict page. MUST return to
        // ST_REV_CHOICE: staying in ANSWER made every later press skip
        // scoring and the verdict screen (device: 只有第一次提示, then
        // endless taps with no end).
        if (rev_next()) {
            s_state = ST_REV_CHOICE;
            rev_show_choice();
            refresh_stats();
        } else {
            show_menu();
        }
        return;
    }

    // ---- ST_STUDY (forward) ----
    if (s_current == (uint16_t)-1) return;
    if (!s_flipped) {
        if (btn == 1) {                     // OK: flip
            audio_se(CT_SE_OK);
            show_answer();
        }
        return;
    }
    uint16_t idx = s_current;
    bool first_sight = !seen_get(idx);
    seen_set(idx);
    if (first_sight && s_cur_kind == PK_REVIEW) s_rev_done++;
    rate_commit(idx, btn == 0 ? VOCAB_AGAIN : (btn == 1 ? VOCAB_HARD : VOCAB_GOOD),
                first_sight);
    if (btn == 0) rq_push(idx);             // 忘记: drill again this session
    audio_se(CT_SE_OK);
    s_done_count++;
    refresh_stats();
    next_card();
}

void vocab_ui_tick(void)
{
    // reserved for animations (OC reaction, progress bar)
}

void vocab_ui_create(lv_obj_t *parent, const vocab_entry_t *entries, uint16_t count)
{
    vocab_init(entries, count);
    s_done_count = 0;
    s_rev_total = vocab_review_due_count();
    s_rev_done = 0;
    s_cur_kind = vocab_review_due_count() ? PK_REVIEW : PK_NEW;
    s_rq_head = s_rq_tail = 0;
    s_rev_n = s_rev_i = 0;
    s_rev_cursor = 0;
    s_menu_row = 0;
    memset(s_seen, 0, sizeof(s_seen));
    s_state = ST_STUDY;
    s_flipped = false;
    s_initialized = true;

    s_scr = lv_obj_create(parent);
    lv_obj_set_size(s_scr, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_stat_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_stat_lbl, &zh_subset, 0);
    lv_obj_set_style_text_color(s_stat_lbl, lv_color_hex(0x8888AA), 0);
    lv_obj_set_pos(s_stat_lbl, 10, 8);

    s_term_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_term_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_term_lbl, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_width(s_term_lbl, 220);
    lv_label_set_long_mode(s_term_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_term_lbl, 10, 60);

    s_phon_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_phon_lbl, &zh_subset, 0);
    lv_obj_set_style_text_color(s_phon_lbl, lv_color_hex(0x88AACC), 0);
    lv_obj_set_width(s_phon_lbl, 220);
    lv_label_set_long_mode(s_phon_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_phon_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_phon_lbl, 10, 100);

    s_def_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_def_lbl, &zh_subset, 0);
    lv_obj_set_style_text_color(s_def_lbl, lv_color_hex(0xA0C0A0), 0);
    lv_obj_set_width(s_def_lbl, 220);
    lv_label_set_long_mode(s_def_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(s_def_lbl, 10, 150);

    s_hint_lbl = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_hint_lbl, &zh_subset, 0);
    lv_obj_set_style_text_color(s_hint_lbl, lv_color_hex(0x666688), 0);
    lv_obj_set_pos(s_hint_lbl, 10, 280);

    next_card();
    refresh_stats();
}

void vocab_ui_destroy(void)
{
    if (s_scr) { lv_obj_del(s_scr); s_scr = NULL; }
    s_initialized = false;
}

int vocab_ui_due(void) { return vocab_due_count(); }
int vocab_ui_done(void) { return s_done_count; }
