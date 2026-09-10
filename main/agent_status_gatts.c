// main/agent_status_gatts.c —— custom status service (device only).
#ifdef ESP_PLATFORM

#include "agent_status_gatts.h"
#include "agent_status.h"
#include "app_shell.h"
#include "chiptune.h"

#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_hidd_gatts.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "agent_gatts";

// 128-bit service UUID: 7A3B2C10-5D4E-4F00-8A11-223344556677
static const uint8_t SVC_UUID128[16] = {
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x8A,
    0x00, 0x4F, 0x4E, 0x5D, 0x10, 0x2C, 0x3B, 0x7A,
};
// Char UUIDs (16-bit, vendor range under our service).
#define CHAR_STATUS_UUID 0xAA01u  // write: slot update
#define CHAR_SUMMARY_UUID 0xAA02u  // read+notify: 4x(state,progress)

static uint16_t s_svc_handle;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_status_handle;
static uint16_t s_summary_handle;
static uint16_t s_conn_id = 0xFFFFu;

// Write layout (52B): [slot u8][src u8][state u8][progress u8][name 16B][text 32B]
#define UPDATE_LEN (4u + AGENT_NAME_LEN + AGENT_TEXT_LEN)
#define UPDATE_MASK ((UINT64_C(1) << UPDATE_LEN) - 1u)

static uint8_t s_prepare[UPDATE_LEN];
static uint64_t s_prepare_mask;
static uint16_t s_prepare_conn = 0xFFFFu;
static uint16_t s_cccd_handle;
static bool s_notify_on;

static void write_response(esp_ble_gatts_cb_param_t *param, esp_gatt_status_t status)
{
    if (param->write.need_rsp)
        esp_ble_gatts_send_response(s_gatts_if, param->write.conn_id,
                                    param->write.trans_id, status, NULL);
}

static void summary_fill(uint8_t out[8])
{
    for (int i = 0; i < 4; i++) {
        agent_slot_t s;
        bool found = agent_status_get_copy((uint8_t)i, &s);
        out[i * 2] = (found && s.used) ? (uint8_t)s.state : 0;
        out[i * 2 + 1] = (found && s.used) ? s.progress : 0;
    }
}

static void summary_notify(void)
{
    if (s_conn_id == 0xFFFFu || !s_summary_handle || !s_notify_on) return;
    uint8_t sum[8];
    summary_fill(sum);
    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_summary_handle,
                                sizeof(sum), sum, false);
}

static esp_gatt_status_t apply_update(const uint8_t *v, uint16_t len)
{
    if (len != UPDATE_LEN) {
        ESP_LOGW(TAG, "bad update len %u", (unsigned)len);
        return ESP_GATT_INVALID_ATTR_LEN;
    }
    uint8_t slot = v[0];
    if (slot >= AGENT_SLOTS || v[2] >= AST_COUNT || v[1] > ASRC_GENERIC ||
        (v[3] > 100 && v[3] != 255)) {
        ESP_LOGW(TAG, "bad update slot/state/src/progress");
        return ESP_GATT_INVALID_PDU;
    }
    if (v[2] == AST_IDLE) {
        agent_status_clear(slot);
        summary_notify();
        return ESP_GATT_OK;
    }
    char name[AGENT_NAME_LEN], text[AGENT_TEXT_LEN];
    memcpy(name, v + 4, AGENT_NAME_LEN);
    memcpy(text, v + 4 + AGENT_NAME_LEN, AGENT_TEXT_LEN);
    name[AGENT_NAME_LEN - 1] = '\0';
    text[AGENT_TEXT_LEN - 1] = '\0';
    agent_slot_t previous;
    bool had_previous = agent_status_get_copy(slot, &previous);
    bool changed = !had_previous || !previous.used || previous.state != (agent_state_t)v[2];
    agent_status_set(slot, (agent_source_t)v[1], (agent_state_t)v[2], v[3],
                     name, text, (uint64_t)(esp_timer_get_time() / 1000));
    // Coding alerts belong to standby and never interrupt game/pomodoro.
    if (changed && app_shell_mode() == APP_MODE_STANDBY) {
        if (v[2] == AST_NEEDS_YOU) audio_se(CT_SE_AGENT_NEEDS);
        else if (v[2] == AST_CELEBRATE) audio_se(CT_SE_AGENT_DONE);
        else if (v[2] == AST_FAILED) audio_se(CT_SE_AGENT_ERROR);
    }
    ESP_LOGI(TAG, "slot %u -> %s", slot, agent_state_name_en((agent_state_t)v[2]));
    summary_notify();
    return ESP_GATT_OK;
}

static void handle_write(esp_ble_gatts_cb_param_t *param)
{
    if (!param->write.is_prep) {
        write_response(param, apply_update(param->write.value, param->write.len));
        return;
    }
    esp_gatt_status_t status = ESP_GATT_OK;
    uint16_t end = (uint16_t)(param->write.offset + param->write.len);
    if (end > UPDATE_LEN || end < param->write.offset) {
        status = ESP_GATT_INVALID_ATTR_LEN;
    } else {
        if (s_prepare_conn != param->write.conn_id) {
            s_prepare_conn = param->write.conn_id;
            s_prepare_mask = 0;
        }
        memcpy(s_prepare + param->write.offset, param->write.value, param->write.len);
        for (uint16_t i = param->write.offset; i < end; i++)
            s_prepare_mask |= UINT64_C(1) << i;
    }
    if (param->write.need_rsp) {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(rsp));
        rsp.attr_value.handle = param->write.handle;
        rsp.attr_value.offset = param->write.offset;
        rsp.attr_value.len = param->write.len;
        if (param->write.len <= sizeof(rsp.attr_value.value))
            memcpy(rsp.attr_value.value, param->write.value, param->write.len);
        esp_ble_gatts_send_response(s_gatts_if, param->write.conn_id,
                                    param->write.trans_id, status, &rsp);
    }
}

static void handle_exec_write(esp_ble_gatts_cb_param_t *param)
{
    esp_gatt_status_t status = ESP_GATT_OK;
    if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        if (s_prepare_conn != param->exec_write.conn_id || s_prepare_mask != UPDATE_MASK)
            status = ESP_GATT_INVALID_ATTR_LEN;
        else
            status = apply_update(s_prepare, UPDATE_LEN);
    }
    esp_ble_gatts_send_response(s_gatts_if, param->exec_write.conn_id,
                                param->exec_write.trans_id, status, NULL);
    s_prepare_conn = 0xFFFFu;
    s_prepare_mask = 0;
}

static void add_chars(void)
{
    esp_bt_uuid_t svc_uuid = {.len = ESP_UUID_LEN_128, };
    memcpy(svc_uuid.uuid.uuid128, SVC_UUID128, 16);
    esp_gatt_srvc_id_t svc_id = {.id = {.uuid = svc_uuid, .inst_id = 0, },
                                 .is_primary = true, };
    esp_ble_gatts_create_service(s_gatts_if, &svc_id, 8);
}

// ---- HID report CCC replay -------------------------------------------------
// esp_hid only sends input reports after the host has WRITTEN the report's
// CCC in this session. Bonded hosts (macOS/iOS/Windows) do not re-write it on
// reconnect — the spec says the server persists it — so after the first
// session every key press failed inside esp_hid ("Indicate Not Enabled"):
// LINKED on screen, no page turns. Remember the CCC handles the host
// subscribed to (NVS) and replay those writes into esp_hid when the host stays
// silent for a moment after (re)connecting.
#define CCC_MAX 8
#define CCC_REPLAY_MS 1500
static uint16_t s_ccc_handles[CCC_MAX];
static uint8_t s_ccc_n;
static esp_gatt_if_t s_hid_if = ESP_GATT_IF_NONE;
static uint16_t s_hid_conn = 0xFFFFu;
static esp_bd_addr_t s_hid_bda;
static bool s_host_wrote_ccc;
static esp_timer_handle_t s_ccc_timer;

static void ccc_load(void)
{
    nvs_handle_t h;
    if (nvs_open("hid", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_ccc_handles);
    if (nvs_get_blob(h, "ccc", s_ccc_handles, &len) == ESP_OK)
        s_ccc_n = (uint8_t)(len / sizeof(uint16_t));
    nvs_close(h);
}

static void ccc_save(void)
{
    nvs_handle_t h;
    if (nvs_open("hid", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "ccc", s_ccc_handles, s_ccc_n * sizeof(uint16_t));
    nvs_commit(h);
    nvs_close(h);
}

static void ccc_replay(void *arg)
{
    (void)arg;
    if (s_host_wrote_ccc || s_hid_conn == 0xFFFFu || s_hid_if == ESP_GATT_IF_NONE) return;
    static uint8_t on[2] = {0x01, 0x00};   // notifications enabled
    for (uint8_t i = 0; i < s_ccc_n; i++) {
        esp_ble_gatts_cb_param_t p;
        memset(&p, 0, sizeof(p));
        p.write.conn_id = s_hid_conn;
        memcpy(p.write.bda, s_hid_bda, sizeof(esp_bd_addr_t));
        p.write.handle = s_ccc_handles[i];
        p.write.len = 2;
        p.write.value = on;
        esp_hidd_gatts_event_handler(ESP_GATTS_WRITE_EVT, s_hid_if, &p);
    }
    ESP_LOGI(TAG, "HID CCC replayed for %u report(s) (bonded host stayed silent)", s_ccc_n);
}

static void ccc_track(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                      esp_ble_gatts_cb_param_t *param)
{
    if (!s_ccc_timer) {
        const esp_timer_create_args_t a = {.callback = ccc_replay, .name = "hid_ccc"};
        esp_timer_create(&a, &s_ccc_timer);
        ccc_load();
    }
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id != AGENT_GATTS_APP_ID) s_hid_if = gatts_if;
        return;
    }
    if (gatts_if != s_hid_if) return;
    if (event == ESP_GATTS_CONNECT_EVT) {
        s_hid_conn = param->connect.conn_id;
        memcpy(s_hid_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        s_host_wrote_ccc = false;
        if (s_ccc_n) esp_timer_start_once(s_ccc_timer, (uint64_t)CCC_REPLAY_MS * 1000);
    } else if (event == ESP_GATTS_DISCONNECT_EVT) {
        s_hid_conn = 0xFFFFu;
        esp_timer_stop(s_ccc_timer);
    } else if (event == ESP_GATTS_WRITE_EVT && !param->write.is_prep && param->write.len == 2
               && (param->write.value[0] & 0x03) && param->write.value[1] == 0) {
        s_host_wrote_ccc = true;
        bool known = false;
        for (uint8_t i = 0; i < s_ccc_n; i++) if (s_ccc_handles[i] == param->write.handle) known = true;
        if (!known && s_ccc_n < CCC_MAX) {
            s_ccc_handles[s_ccc_n++] = param->write.handle;
            ccc_save();
        }
    }
}

void agent_gatts_dispatcher(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                            esp_ble_gatts_cb_param_t *param)
{
    ccc_track(event, gatts_if, param);
    // HID first (ignores foreign app_ids safely), then ours.
    esp_hidd_gatts_event_handler(event, gatts_if, param);

    // The callback is shared by the HID and custom apps. Only REG carries an
    // app_id; every later service-building event must be isolated by gatts_if
    // or our characteristic could be attached to a HID service.
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.app_id == AGENT_GATTS_APP_ID && param->reg.status == ESP_GATT_OK) {
            s_gatts_if = gatts_if;
            add_chars();
        }
        return;
    }
    // Connection-wide events may be broadcast with ESP_GATT_IF_NONE. Attribute
    // creation remains exact-interface-only below.
    if (gatts_if != s_gatts_if && gatts_if != ESP_GATT_IF_NONE) return;

    switch (event) {
    case ESP_GATTS_CREATE_EVT: {
        if (gatts_if != s_gatts_if) break;
        s_svc_handle = param->create.service_handle;
        esp_bt_uuid_t cuuid;
        cuuid.len = ESP_UUID_LEN_16;
        // status char (write)
        cuuid.uuid.uuid16 = CHAR_STATUS_UUID;
        esp_ble_gatts_add_char(s_svc_handle, &cuuid,
                               ESP_GATT_PERM_WRITE_ENCRYPTED,
                               ESP_GATT_CHAR_PROP_BIT_WRITE,
                               NULL, NULL);
        break;
    }
    case ESP_GATTS_ADD_CHAR_EVT: {
        if (gatts_if != s_gatts_if) break;
        uint16_t uuid = param->add_char.char_uuid.uuid.uuid16;
        if (uuid == CHAR_STATUS_UUID) {
            s_status_handle = param->add_char.attr_handle;
            // summary char (read + notify)
            esp_bt_uuid_t cuuid = {.len = ESP_UUID_LEN_16, };
            cuuid.uuid.uuid16 = CHAR_SUMMARY_UUID;
            esp_ble_gatts_add_char(s_svc_handle, &cuuid,
                                   ESP_GATT_PERM_READ_ENCRYPTED,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                   NULL, NULL);
        } else if (uuid == CHAR_SUMMARY_UUID) {
            s_summary_handle = param->add_char.attr_handle;
            // CCCD so hosts can subscribe to summary notifies.
            // Stack-owned value (AUTO_RSP): a subscribe/read never waits on
            // app code, so no 30 s ATT timeout for hosts that subscribe.
            esp_bt_uuid_t duuid = {.len = ESP_UUID_LEN_16, };
            duuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
            static uint8_t cccd_val[2] = {0, 0};
            esp_attr_value_t cccd = {
                .attr_max_len = sizeof(cccd_val),
                .attr_len = sizeof(cccd_val),
                .attr_value = cccd_val,
            };
            esp_attr_control_t ctrl = {.auto_rsp = ESP_GATT_AUTO_RSP};
            esp_ble_gatts_add_char_descr(s_svc_handle, &duuid,
                                         ESP_GATT_PERM_READ_ENCRYPTED |
                                             ESP_GATT_PERM_WRITE_ENCRYPTED,
                                         &cccd, &ctrl);
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        // Ours only (HID descriptors arrive here too via the shared dispatcher).
        if (gatts_if == s_gatts_if &&
            param->add_char_descr.service_handle == s_svc_handle && s_svc_handle) {
            s_cccd_handle = param->add_char_descr.attr_handle;
            esp_ble_gatts_start_service(s_svc_handle);
            ESP_LOGI(TAG, "agent status service up");
        }
        break;
    case ESP_GATTS_CONNECT_EVT:
        s_conn_id = param->connect.conn_id;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        s_conn_id = 0xFFFFu;
        s_prepare_conn = 0xFFFFu;
        s_prepare_mask = 0;
        s_notify_on = false;
        break;
    case ESP_GATTS_WRITE_EVT:
        if (param->write.handle == s_status_handle) handle_write(param);
        else if (param->write.handle == s_cccd_handle && s_cccd_handle && param->write.len >= 1)
            s_notify_on = (param->write.value[0] & 0x01) != 0;   // AUTO_RSP answered already
        break;
    case ESP_GATTS_EXEC_WRITE_EVT:
        if (s_prepare_conn == param->exec_write.conn_id) handle_exec_write(param);
        break;
    case ESP_GATTS_READ_EVT:
        if (param->read.handle == s_summary_handle) {
            uint8_t sum[8];
            summary_fill(sum);
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(rsp));
            rsp.attr_value.len = sizeof(sum);
            memcpy(rsp.attr_value.value, sum, sizeof(sum));
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id,
                                        param->read.trans_id, ESP_GATT_OK, &rsp);
        }
        break;
    default:
        break;
    }
}

#endif  // ESP_PLATFORM
