// main/pet_sleep.h —— USB-host power detection + deep-sleep entry.
#pragma once

#include <stdbool.h>

// True when a live USB host is attached (SOF packets seen: a computer).
// Chargers/power banks report false — the pet may sleep while charging there.
bool pet_sleep_powered(void);

// Panel off, stamp wall clock for wake restore, arm any-button GPIO wake,
// enter deep sleep. Never returns.
void pet_sleep_now(void);
