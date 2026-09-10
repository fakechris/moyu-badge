// main/agent_status_gatts.h —— BLE custom service behind the clicker link.
// Same peripheral as HID ("DeskPet Click"); laptop sender writes slot updates,
// device stores + notifies a compact summary. Device-only (ESP_PLATFORM);
// sim drives agent_status_set() directly via script token.
#pragma once

#include "esp_gatts_api.h"

#define AGENT_GATTS_APP_ID 0x55AAu

// GATTS dispatcher: handles our app_id, forwards EVERYTHING to the HID
// handler (which safely ignores foreign app_ids). Registered instead of
// esp_hidd_gatts_event_handler in clicker_hid_ble.c.
void agent_gatts_dispatcher(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t *param);
