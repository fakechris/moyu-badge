// vocab_ui.h — Vocabulary flashcard UI for DeskPet.
//
// Creates a full-screen vocab study view on the existing screen object.
// 3-button flow:
//   front shown: OK = flip to answer
//   answer shown: UP = Again, OK = Good, DOWN = Easy
// Session ends when no more due words.
#ifndef VOCAB_UI_H
#define VOCAB_UI_H

#include "lvgl.h"
#include "vocab_model.h"

// Create the vocab UI (call once on mode entry)
void vocab_ui_create(lv_obj_t *parent, const vocab_entry_t *entries, uint16_t count);

// Tick: call from main loop to refresh display
void vocab_ui_tick(void);

// Button handler, values are semantic not positional:
// 0=忘记(UP) 1=模糊/flip(OK) 2=认识(DOWN). mode_vocab maps bsp_btn_t
// (UP/DOWN/OK order) onto these.
void vocab_ui_button(int btn);

// Cleanup (call on mode exit)
void vocab_ui_destroy(void);

// Get current stats for the status bar
int vocab_ui_due(void);
int vocab_ui_done(void);


#endif // VOCAB_UI_H
