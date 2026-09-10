// main/deskpet_time.h —— wall-clock seed (device has no RTC battery / NTP).
#pragma once

// Call once at boot, before any UI: fixes the timezone to Beijing (UTC+8) and
// seeds the clock from the build-time epoch when the clock is older than it.
// Never moves time backward (a future NTP/BLE sync will simply win).
void deskpet_time_init(void);

// Call right before entering deep sleep: records the wall clock against the
// RTC counter (which keeps counting while asleep, battery-powered). The next
// deskpet_time_init() then replays the elapsed sleep into the clock.
void deskpet_time_sleep_stamp(void);
