// main/clicker_hid.h —— HID transport shared by clicker and agent status.
// Sim uses clicker_hid.c; device uses clicker_hid_ble.c (Bluedroid HOGP).
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CLICK_PREV = 0,  // PageUp
    CLICK_NEXT,      // PageDown
    CLICK_PLAY,      // F5 (start slideshow) / Esc (leave) — alternates
    CLICK_BLANK,     // 'b' (black screen toggle in PowerPoint/Keynote)
} clicker_key_t;

void clicker_hid_init(void);
void clicker_hid_deinit(void);
bool clicker_hid_connected(void);
// Returns false when not connected (UI counts it as dropped).
bool clicker_hid_send(clicker_key_t key);
uint32_t clicker_hid_sent_total(void);
// Accept NEW bonds only while a window is open (clicker mode opens one) or
// while no bond exists yet. Already-bonded hosts reconnect any time.
void clicker_hid_open_pairing(uint32_t window_ms);
bool clicker_hid_pairing_open(void);

// Re-pairing: forget every bonded host, then open a pairing window so a NEW
// laptop can bond (settings entry "翻页笔重新配对"). Old host must re-pair
// from its own Bluetooth menu afterwards.
void clicker_hid_repair(uint32_t window_ms);
