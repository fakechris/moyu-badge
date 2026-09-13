// main/audio_task.c —— BGM/SE render task (device only).
// Research contract: bsp_audio_write blocks, so synthesis lives here — never
// in the LVGL task or button callbacks. 16kHz/16bit/mono, format set once.
// Screen-off / other modes: BGM goes silent, queued SEs still play.
#include "chiptune.h"
#include "audio_task.h"

#ifdef ESP_PLATFORM

#include "app_shell.h"
#include "demo_deskpet.h"

#include "bsp_audio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"
#include <stdatomic.h>

#define CT_BUF_FRAMES 240          // 15 ms @ 16 kHz
#define AMBIENT_AFTER_MS 15000     // screen-on idle: bpm -8, vol 60%, counter off
#define SE_QUEUE_LEN 8

static const char *TAG = "audio_task";
static ct_state_t s_ct;
static TaskHandle_t s_task;
static QueueHandle_t s_se_queue;
static uint8_t s_vol_bgm = 15;     // device pass: quieter defaults, user-adjustable
static uint8_t s_vol_se = 40;      // in Settings (audio_set_volumes)
static _Atomic uint32_t s_last_key_ms;
static _Atomic uint8_t s_mute_all = 1, s_mute_game = 0, s_mute_pomo = 0, s_mute_other = 0;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void load_volumes(void)
{
    nvs_handle_t h;
    uint8_t v;
    if (nvs_open("audio", NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_u8(h, "bgm", &v) == ESP_OK) s_vol_bgm = v;
        if (nvs_get_u8(h, "se", &v) == ESP_OK) s_vol_se = v;
        if (nvs_get_u8(h, "mute", &v) == ESP_OK) s_mute_all = v ? 1 : 0;
        if (nvs_get_u8(h, "mgame", &v) == ESP_OK) s_mute_game = v ? 1 : 0;
        if (nvs_get_u8(h, "mpomo", &v) == ESP_OK) s_mute_pomo = v ? 1 : 0;
        if (nvs_get_u8(h, "mother", &v) == ESP_OK) s_mute_other = v ? 1 : 0;
        nvs_close(h);
    }
}

void audio_prefs_get(audio_prefs_t *out)
{
    out->mute_all = atomic_load(&s_mute_all);
    out->mute_game = atomic_load(&s_mute_game);
    out->mute_pomo = atomic_load(&s_mute_pomo);
    out->mute_other = atomic_load(&s_mute_other);
}

void audio_prefs_set(const audio_prefs_t *p)
{
    atomic_store(&s_mute_all, p->mute_all ? 1 : 0);
    atomic_store(&s_mute_game, p->mute_game ? 1 : 0);
    atomic_store(&s_mute_pomo, p->mute_pomo ? 1 : 0);
    atomic_store(&s_mute_other, p->mute_other ? 1 : 0);
    nvs_handle_t h;
    if (nvs_open("audio", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "mute", p->mute_all ? 1 : 0);
        nvs_set_u8(h, "mgame", p->mute_game ? 1 : 0);
        nvs_set_u8(h, "mpomo", p->mute_pomo ? 1 : 0);
        nvs_set_u8(h, "mother", p->mute_other ? 1 : 0);
        nvs_commit(h);
        nvs_close(h);
    }
}

// Is sound allowed right now for the active mode?
static bool scope_allowed(app_mode_t mode)
{
    if (atomic_load(&s_mute_all)) return false;
    if (mode == APP_MODE_GAME) return !atomic_load(&s_mute_game);
    if (mode == APP_MODE_POMO) return !atomic_load(&s_mute_pomo);
    return !atomic_load(&s_mute_other);   // shell UI blips, clicker, standby/agent cues
}

static void save_volumes(void)
{
    nvs_handle_t h;
    if (nvs_open("audio", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "bgm", s_vol_bgm);
        nvs_set_u8(h, "se", s_vol_se);
        nvs_commit(h);
        nvs_close(h);
    }
}

// ---- strong frontend (overrides the weak stubs in chiptune.c) --------------
void audio_bgm_scene(int scene) { (void)scene; }   // scenes are polled instead
void audio_se(int se)
{
    if (se < 0 || se >= CT_SE_N || !s_se_queue) return;
    if (!scope_allowed(app_shell_mode())) return;
    uint8_t id = (uint8_t)se;
    // UI, BLE and audio each have their own task. Queueing keeps ct_state_t
    // single-owner; direct ct_se() here races the sample renderer.
    if (xQueueSend(s_se_queue, &id, 0) != pdTRUE) {
        uint8_t dropped;
        xQueueReceive(s_se_queue, &dropped, 0);  // prefer the newest cue
        xQueueSend(s_se_queue, &id, 0);
    }
}
void audio_idle_feed(void) { s_last_key_ms = now_ms(); }

// UI volume control (persists). Called from a settings page later.
void audio_set_volumes(uint8_t bgm, uint8_t se)
{
    if (bgm > 100) bgm = 100;
    if (se > 100) se = 100;
    s_vol_bgm = bgm;
    s_vol_se = se;
    save_volumes();
}

void audio_get_volumes(uint8_t *bgm, uint8_t *se)
{
    if (bgm) *bgm = s_vol_bgm;
    if (se) *se = s_vol_se;
}

static void audio_task(void *arg)
{
    (void)arg;
    static int16_t buf[CT_BUF_FRAMES];
    if (bsp_audio_set_format(CT_SR, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "codec format failed; stopping audio task");
        s_task = NULL;
        vTaskDelete(NULL);
        return;
    }
    bsp_audio_set_volume(70);             // engine now owns the headroom; 85 overdrove the amp
    load_volumes();
    ct_init(&s_ct);
    ct_volumes(&s_ct, s_vol_bgm, s_vol_se);
    s_last_key_ms = now_ms();

    ct_scene_t scene = CT_SCENE_NONE;
    for (;;) {
        // Only the game mode sings; pomo/clicker/standby are silent, but
        // queued SEs still drain (report/death blips work everywhere).
        ct_scene_t want = (app_shell_mode() == APP_MODE_GAME && scope_allowed(APP_MODE_GAME))
                              ? demo_deskpet_audio_scene()
                              : CT_SCENE_NONE;
        if (want != scene) {
            scene = want;
            ct_set_arrange(&s_ct, chiptune_scene_arrange(scene));
        }
        int idle = (now_ms() - atomic_load(&s_last_key_ms)) > AMBIENT_AFTER_MS;
        ct_set_idle(&s_ct, idle);
        // ambient: volume 60%, counter muted inside the engine
        ct_volumes(&s_ct, idle ? (uint8_t)(s_vol_bgm * 60 / 100) : s_vol_bgm,
                   s_vol_se);
        if (!s_ct.se_active && s_ct.since_se >= CT_SR / 8) {
            uint8_t se;
            if (xQueueReceive(s_se_queue, &se, 0) == pdTRUE)
                ct_se(&s_ct, (ct_se_t)se);
        }
        ct_render(&s_ct, buf, CT_BUF_FRAMES);
        if (bsp_audio_write(buf, CT_BUF_FRAMES * 2) != ESP_OK) {
            ESP_LOGE(TAG, "codec write failed; stopping audio task");
            s_task = NULL;
            vTaskDelete(NULL);
            return;
        }
    }
}

void audio_task_start(void)
{
    if (s_task) return;
    if (!s_se_queue) s_se_queue = xQueueCreate(SE_QUEUE_LEN, sizeof(uint8_t));
    if (!s_se_queue) {
        ESP_LOGE(TAG, "SE queue allocation failed; audio disabled");
        return;
    }
    if (xTaskCreate(audio_task, "bgm", 4096, NULL, 4, &s_task) != pdPASS) {
        s_task = NULL;
        ESP_LOGE(TAG, "audio task allocation failed; audio disabled");
    }
}

#else  // host build: no-op frontend (MSVC has no weak symbols)
static audio_prefs_t s_prefs = {1, 0, 0, 0};
static uint8_t s_hv_bgm = 15, s_hv_se = 40;
void audio_task_start(void) {}
void audio_bgm_scene(int scene) { (void)scene; }
void audio_se(int se) { (void)se; }
void audio_idle_feed(void) {}
void audio_set_volumes(uint8_t bgm, uint8_t se) { s_hv_bgm = bgm; s_hv_se = se; }
void audio_get_volumes(uint8_t *bgm, uint8_t *se) { if (bgm) *bgm = s_hv_bgm; if (se) *se = s_hv_se; }
void audio_prefs_get(audio_prefs_t *out) { *out = s_prefs; }
void audio_prefs_set(const audio_prefs_t *p) { s_prefs = *p; }
#endif
