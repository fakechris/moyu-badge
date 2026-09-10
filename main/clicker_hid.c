// main/clicker_hid.c —— simulator transport: log-only, auto-linked.
// Firmware compiles clicker_hid_ble.c instead; see main/CMakeLists.txt.
#include "clicker_hid.h"

#include <stdio.h>

static uint32_t s_sent;
static bool s_linked;

void clicker_hid_init(void)
{
    s_sent = 0;
    s_linked = true;  // sim: link is virtual
    printf("[HID] init linked=1\n");
}

void clicker_hid_deinit(void)
{
    s_linked = false;
    printf("[HID] deinit\n");
}

bool clicker_hid_connected(void) { return s_linked; }

bool clicker_hid_send(clicker_key_t key)
{
    static const char *const NAMES[] = {"PageUp", "PageDown", "F5/Esc", "Blank"};
    if (!s_linked) return false;
    s_sent++;
    printf("[HID] report %s (#%u)\n", NAMES[key < 4 ? key : 0], (unsigned)s_sent);
    return true;
}

uint32_t clicker_hid_sent_total(void) { return s_sent; }
static bool s_pair_open;
void clicker_hid_open_pairing(uint32_t window_ms) { s_pair_open = window_ms > 0; }
bool clicker_hid_pairing_open(void) { return s_pair_open; }
