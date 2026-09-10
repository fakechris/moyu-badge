// main/clicker_hid_ble.c —— BLE HID keyboard device (Bluedroid + esp_hidd).
// Starts at boot because coding status shares the peripheral in every mode.
// Keys: PageUp / PageDown / F5-Esc / B on keyboard report 1 (consumer report 2 unused).
// Pairing: LE Secure Connections (no IO -> unauthenticated but eavesdrop-
// safe), bonded for reconnect. New bonds are only accepted while a pairing
// window is open (clicker mode opens one) or before the first bond exists,
// so a stranger's phone cannot pair and spoof agent status on the desk.
// Hardware test steps: docs/SHELL.md.
#include "clicker_hid.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_hid_common.h"
#include "esp_hidd.h"
#include "esp_hidd_gatts.h"
#include "agent_status_gatts.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdatomic.h>
#include <string.h>

static const char *TAG = "clicker_hid";

#define USB_HID_PAGEUP 0x4B
#define USB_HID_PAGEDOWN 0x4E
#define USB_HID_F5 0x3E
#define USB_HID_ESC 0x29
#define USB_HID_B 0x05
#define USB_HID_ENTER 0x28
#define REPORT_KEYBOARD 1u
#define REPORT_CONSUMER 2u

static const unsigned char keyboardReportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x05,
    0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07,
    0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
    // One-bit Consumer Control report: standard Play/Pause usage 0xCD.
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, REPORT_CONSUMER,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01,
    0x09, 0xCD, 0x81, 0x02,
    0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0xC0,
};

static esp_hid_raw_report_map_t s_maps[] = {
    {.data = keyboardReportMap, .len = sizeof(keyboardReportMap)},
};

static esp_hid_device_config_t s_cfg = {
    .vendor_id = 0x16C0,
    .product_id = 0x05DF,
    .version = 0x0100,
    .device_name = "DeskPet Click",
    .manufacturer_name = "DeskPet",
    .serial_number = "0001",
    .report_maps = s_maps,
    .report_maps_len = 1,
};

static esp_hidd_dev_t *s_dev;
static _Atomic bool s_connected;
static bool s_init_attempted;
static _Atomic bool s_adv_ready;
static _Atomic bool s_hid_ready;
static uint32_t s_sent;

static uint64_t s_pair_until_us;

void clicker_hid_open_pairing(uint32_t window_ms)
{
    s_pair_until_us = esp_timer_get_time() + (uint64_t)window_ms * 1000u;
    ESP_LOGI(TAG, "pairing window %u ms", (unsigned)window_ms);
}

static int s_bond_count = -1;   // bonds known before the current pairing

static void advertise_when_ready(void);

void clicker_hid_repair(uint32_t window_ms)
{
    // Forget every bonded host: without this, an already-bonded laptop
    // reconnects instantly and a new laptop can never win the window.
    int n = esp_ble_get_bond_device_num();
    if (n > 0) {
        esp_ble_bond_dev_t list[8];
        if (n > 8) n = 8;
        if (esp_ble_get_bond_device_list(&n, list) == ESP_OK) {
            for (int i = 0; i < n; i++)
                esp_ble_remove_bond_device(list[i].bd_addr);
        }
    }
    s_bond_count = 0;
    s_connected = false;
    clicker_hid_open_pairing(window_ms);
    advertise_when_ready();   // re-open advertising for the new host
}

bool clicker_hid_pairing_open(void)
{
    return (uint64_t)esp_timer_get_time() < s_pair_until_us;
}


static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min = 0x20, .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND, .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void advertise_when_ready(void)
{
    if (atomic_load(&s_adv_ready) && atomic_load(&s_hid_ready) &&
        !atomic_load(&s_connected)) {
        esp_err_t err = esp_ble_gap_start_advertising(&s_adv_params);
        if (err != ESP_OK) ESP_LOGW(TAG, "advertising start failed: %s", esp_err_to_name(err));
    }
}

static void hidd_cb(void *handler_args, esp_event_base_t base, int32_t id, void *data)
{
    (void)handler_args;
    (void)base;
    (void)data;
    if (id == ESP_HIDD_START_EVENT) {
        s_hid_ready = true;
        advertise_when_ready();
    } else if (id == ESP_HIDD_CONNECT_EVENT) {
        s_connected = true;
        ESP_LOGI(TAG, "host connected");
    } else if (id == ESP_HIDD_DISCONNECT_EVENT) {
        s_connected = false;
        ESP_LOGI(TAG, "host disconnected, re-advertising");
        advertise_when_ready();
    }
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT) {
        s_adv_ready = param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS;
        advertise_when_ready();
    } else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
            ESP_LOGI(TAG, "advertising (DeskPet Click)");
        else
            ESP_LOGW(TAG, "advertising rejected: %d", param->adv_start_cmpl.status);
    } else if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
        // Always let the stack proceed: hosts connect with resolvable private
        // addresses, so an address check here cannot tell a bonded laptop
        // from a stranger (that check caused a connect/drop loop). The
        // window policy is enforced on the RESULT instead (below).
        if (s_bond_count < 0) s_bond_count = esp_ble_get_bond_device_num();
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
    } else if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
        bool ok = param->ble_security.auth_cmpl.success;
        int n = esp_ble_get_bond_device_num();
        ESP_LOGI(TAG, "auth %s (bonds %d)", ok ? "ok" : "failed", n);
        if (ok && s_bond_count >= 0 && n > s_bond_count && s_bond_count > 0
            && !clicker_hid_pairing_open()) {
            // A NEW bond outside the pairing window: not our laptop. Forget
            // it and drop the link; the known host is unaffected.
            ESP_LOGW(TAG, "new bond refused (open clicker mode to pair)");
            esp_ble_remove_bond_device(param->ble_security.auth_cmpl.bd_addr);
            esp_ble_gap_disconnect(param->ble_security.auth_cmpl.bd_addr);
        } else if (ok) {
            s_bond_count = n;
        }
    }
}

void clicker_hid_init(void)
{
    if (s_init_attempted) return;
    s_init_attempted = true;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "bt controller init failed");
        return;
    }
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
        ESP_LOGE(TAG, "bt controller enable failed");
        return;
    }
    if (esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(TAG, "bluedroid init/enable failed");
        return;
    }
    if (esp_ble_gatt_set_local_mtu(128) != ESP_OK)
        ESP_LOGW(TAG, "local MTU remains at stack default; long writes still supported");
    if (esp_ble_gap_register_callback(gap_cb) != ESP_OK) {
        ESP_LOGE(TAG, "gap register failed");
        return;
    }
    // JustWorks + bond. NOT ESP_LE_AUTH_REQ_SC_BOND: with SC the stack tries
    // to derive a BR/EDR link key from the LTK (CTKD); this BLE-only build has
    // no classic security mode, the derivation fails and Bluedroid deletes
    // the fresh bond ("remove bond, rsn 85") -> link never encrypts -> host's
    // CCC writes are refused -> no reports (device log 2026-09-05).
    esp_ble_auth_req_t auth = ESP_LE_AUTH_BOND;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth, sizeof(uint8_t));
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    uint8_t key_size = 16;
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

    static const uint8_t adv_data[] = {
        0x02, 0x01, 0x06,             // flags: LE General Discoverable, no BR/EDR
        0x03, 0x03, 0x12, 0x18,       // complete 16-bit UUID: HID 0x1812
        0x03, 0x19, 0xC1, 0x03,       // appearance: keyboard 0x03C1
        0x0E, 0x09, 'D', 'e', 's', 'k', 'P', 'e', 't', ' ', 'C', 'l', 'i', 'c', 'k',
    };
    _Static_assert(sizeof(adv_data) <= 31, "BLE advertising packet too large");
    esp_ble_gap_set_device_name("DeskPet Click");
    if (esp_ble_gap_config_adv_data_raw((uint8_t *)adv_data, sizeof(adv_data)) != ESP_OK) {
        ESP_LOGE(TAG, "advertising data config failed");
        return;
    }

    if (esp_ble_gatts_register_callback(agent_gatts_dispatcher) != ESP_OK) {
        ESP_LOGE(TAG, "gatts register failed");
        return;
    }
    // Agent-status service shares this dispatcher (forwards HID events).
    if (esp_ble_gatts_app_register(AGENT_GATTS_APP_ID) != ESP_OK)
        ESP_LOGE(TAG, "agent status app registration failed");
    if (esp_hidd_dev_init(&s_cfg, ESP_HID_TRANSPORT_BLE, hidd_cb, &s_dev) != ESP_OK) {
        ESP_LOGE(TAG, "hidd dev init failed");
        s_dev = NULL;
        return;
    }
    ESP_LOGI(TAG, "HID device up, waiting for host");
}

void clicker_hid_deinit(void)
{
    // Keep the stack up once initialized (re-init cost + bonding churn).
    // UI treats exit as link-idle; reports gate on s_connected.
}

bool clicker_hid_connected(void) { return atomic_load(&s_connected) && s_dev; }

bool clicker_hid_send(clicker_key_t key)
{
    if (!clicker_hid_connected()) return false;
    static bool s_presenting;   // OK alternates start / Esc (leave)
    uint8_t code = 0, mods = 0;
    {
        switch (key) {
        case CLICK_PREV: code = USB_HID_PAGEUP; break;
        case CLICK_NEXT: code = USB_HID_PAGEDOWN; break;
        case CLICK_BLANK: code = USB_HID_B; break;
        default:
            if (s_presenting) {
                code = USB_HID_ESC;
            } else {
                // Start slideshow on either OS: F5 (Windows PowerPoint/WPS)
                // followed by Cmd+Shift+Return (macOS PowerPoint/WPS/Keynote
                // "play"). Whichever the host ignores is harmless.
                uint8_t f5[7] = {0, 0, USB_HID_F5, 0, 0, 0, 0};
                uint8_t up[7] = {0};
                if (esp_hidd_dev_input_set(s_dev, 0, REPORT_KEYBOARD, f5, sizeof(f5)) != ESP_OK)
                    return false;
                vTaskDelay(pdMS_TO_TICKS(40));
                esp_hidd_dev_input_set(s_dev, 0, REPORT_KEYBOARD, up, sizeof(up));
                vTaskDelay(pdMS_TO_TICKS(40));
                code = USB_HID_ENTER;
                mods = 0x02 | 0x08;   // Left Shift + Left GUI (Cmd)
            }
            s_presenting = !s_presenting;
            break;
        }
        // Report 1 per the map: 1 modifier byte + 1 reserved + 5-key array = 7 bytes.
        uint8_t buf[7] = {0};
        buf[0] = mods;
        buf[2] = code;
        if (esp_hidd_dev_input_set(s_dev, 0, REPORT_KEYBOARD, buf, sizeof(buf)) != ESP_OK)
            return false;
        vTaskDelay(pdMS_TO_TICKS(50));
        memset(buf, 0, sizeof(buf));
        esp_hidd_dev_input_set(s_dev, 0, REPORT_KEYBOARD, buf, sizeof(buf));
    }
    s_sent++;
    ESP_LOGI(TAG, "keyboard report 0x%02X (#%u)", code, (unsigned)s_sent);
    return true;
}

uint32_t clicker_hid_sent_total(void) { return s_sent; }
