// sim/stubs.c —— host implementations: wall clock, file NVS, BSP no-ops,
// button callback registry fed by sim/main.c keyboard handling.
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "bsp_button.h"
#include "bsp_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

// ---- esp_timer ----
int64_t esp_timer_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// ---- NVS: tiny file-backed k/v (namespace.key -> blob), sim/.nvs.bin ----
static const char *nvs_path(void)
{
    static char p[512];
    const char *tmp = getenv("TMPDIR");
    snprintf(p, sizeof(p), "%sdeskpet-sim-nvs.bin", tmp ? tmp : "/tmp/");
    return p;
}

#define NVS_MAX 64
typedef struct {
    char key[80];
    uint8_t data[2048];
    size_t len;
} nvs_entry_t;

static nvs_entry_t s_nvs[NVS_MAX];
static int s_nvs_n = -1;  // -1 = not loaded

static void nvs_load(void)
{
    if (s_nvs_n >= 0) return;
    s_nvs_n = 0;
    FILE *f = fopen(nvs_path(), "rb");
    if (!f) return;
    int n = 0;
    if (fread(&n, sizeof(n), 1, f) == 1 && n > 0 && n <= NVS_MAX) {
        if (fread(s_nvs, sizeof(s_nvs[0]), (size_t)n, f) == (size_t)n) s_nvs_n = n;
    }
    fclose(f);
}

static void nvs_flush(void)
{
    FILE *f = fopen(nvs_path(), "wb");
    if (!f) return;
    fwrite(&s_nvs_n, sizeof(s_nvs_n), 1, f);
    fwrite(s_nvs, sizeof(s_nvs[0]), (size_t)s_nvs_n, f);
    fclose(f);
}

static nvs_entry_t *nvs_find(const char *ns, const char *key)
{
    char full[80];
    snprintf(full, sizeof(full), "%s.%s", ns, key);
    for (int i = 0; i < s_nvs_n; i++)
        if (strcmp(s_nvs[i].key, full) == 0) return &s_nvs[i];
    return NULL;
}

static char s_ns[32];

esp_err_t nvs_flash_init(void) { nvs_load(); return ESP_OK; }
esp_err_t nvs_flash_erase(void)
{
    s_nvs_n = 0;
    nvs_flush();
    return ESP_OK;
}

esp_err_t nvs_open(const char *ns, unsigned mode, nvs_handle_t *out)
{
    (void)mode;
    nvs_load();
    snprintf(s_ns, sizeof(s_ns), "%s", ns);
    *out = 1;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len)
{
    (void)h;
    nvs_entry_t *e = nvs_find(s_ns, key);
    if (!e || *len < e->len) return ESP_FAIL;
    memcpy(out, e->data, e->len);
    *len = e->len;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *v, size_t len)
{
    (void)h;
    nvs_entry_t *e = nvs_find(s_ns, key);
    if (!e) {
        if (s_nvs_n >= NVS_MAX || len > sizeof(e->data)) return ESP_FAIL;
        e = &s_nvs[s_nvs_n++];
        snprintf(e->key, sizeof(e->key), "%s.%s", s_ns, key);
    }
    if (len > sizeof(e->data)) return ESP_FAIL;
    memcpy(e->data, v, len);
    e->len = len;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *out)
{
    size_t len = 1;
    return nvs_get_blob(h, key, out, &len);
}

esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v)
{
    return nvs_set_blob(h, key, &v, 1);
}

esp_err_t nvs_commit(nvs_handle_t h)
{
    (void)h;
    nvs_flush();
    return ESP_OK;
}

void nvs_close(nvs_handle_t h) { (void)h; }

// ---- BSP ----
static bsp_btn_cb_t s_cb;
static void *s_user;

esp_err_t bsp_display_init(void) { return ESP_OK; }
void bsp_display_backlight(uint8_t percent) { (void)percent; }
struct _lv_display_t *bsp_lvgl_init(void)
{
    return (struct _lv_display_t *)1;  // SDL display is made by sim/main.c
}
bool bsp_lvgl_lock(int timeout_ms)
{
    (void)timeout_ms;
    return true;
}
void bsp_lvgl_unlock(void) {}

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user)
{
    s_cb = cb;
    s_user = user;
    return ESP_OK;
}

int bsp_button_read_mv(void) { return 3300; }

void sim_inject_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    printf("[SIM] key btn=%d ev=%d\n", (int)btn, (int)ev);
    if (s_cb) s_cb(btn, ev, s_user);
}

// bsp_i2c / audio / battery stubs used by app_main.
esp_err_t bsp_i2c_init(void) { return ESP_OK; }
void bsp_i2c_scan(void) {}
esp_err_t bsp_audio_init(void) { return ESP_OK; }
esp_err_t bsp_battery_init(void) { return ESP_OK; }
int bsp_battery_soc(void) { return 80; }

// clicker_hid_ble.c is firmware-only (NimBLE); settings only toggles a window.
void clicker_hid_repair(uint32_t window_ms) { (void)window_ms; }
