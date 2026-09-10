// main/mode_vocab.h —— vocabulary flashcard mode (FSRS + 3-button rating).
#pragma once

#include "bsp_button.h"

void mode_vocab_enter(void);
void mode_vocab_exit(void);
void mode_vocab_key(bsp_btn_t btn, bsp_btn_ev_t ev);
