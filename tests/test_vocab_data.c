// tests/test_vocab_data.c — validates the generated vocab_data.h deck:
// size bounds, field completeness, unique terms, sane phonetics.
// Catches import_anki.py regressions (empty fields, dup terms, broken ranks)
// before they reach the device flash.
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "vocab_model.h"
#include "vocab_data.h"

int main(void)
{
    printf("deck size: %d\n", VOCAB_DATA_COUNT);
    assert(VOCAB_DATA_COUNT >= 100);            // meaningful deck, not test stubs
    assert(VOCAB_DATA_COUNT <= VOCAB_MAX_WORDS); // FSRS NVS layout bound

    vocab_init(s_vocab_data, VOCAB_DATA_COUNT);
    assert(vocab_total_words() == VOCAB_DATA_COUNT);
    assert(vocab_due_count() == VOCAB_DATA_COUNT); // fresh install: all due
    assert(vocab_new_words() == VOCAB_DATA_COUNT);

    for (int i = 0; i < VOCAB_DATA_COUNT; i++) {
        const vocab_entry_t *e = vocab_get_entry((uint16_t)i);
        assert(e && e->term && e->phonetic && e->definition);
        assert(e->term[0] && e->phonetic[0] && e->definition[0]);
        assert(strlen(e->term) < 32);   // bytes; terms are ASCII
        assert(strlen(e->phonetic) <= 60);  // 英/美 CJK(3B) + IPA(2B) + ASCII
        assert(strlen(e->definition) <= 185); // 60-char cap -> 180 bytes CJK
        // terms are lowercase English words (proper nouns filtered at import)
        assert(e->term[0] >= 'a' && e->term[0] <= 'z');
        // distractors: in range, not self, distinct (reverse 3-choice data)
        assert(e->distractors[0] < VOCAB_DATA_COUNT);
        assert(e->distractors[1] < VOCAB_DATA_COUNT);
        assert(e->distractors[0] != (uint16_t)i);
        assert(e->distractors[1] != (uint16_t)i);
        assert(e->distractors[0] != e->distractors[1]);
    }

    // uniqueness (O(n^2) at 500 entries is fine on host)
    for (int i = 0; i < VOCAB_DATA_COUNT; i++)
        for (int j = i + 1; j < VOCAB_DATA_COUNT; j++)
            assert(strcmp(s_vocab_data[i].term, s_vocab_data[j].term) != 0);

    // alphabetical study order (import promise)
    for (int i = 1; i < VOCAB_DATA_COUNT; i++)
        assert(strcmp(s_vocab_data[i - 1].term, s_vocab_data[i].term) < 0);

    printf("VOCAB DATA GATES PASS\n");
    return 0;
}
