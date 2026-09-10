// main/deskpet_store.c —— one NVS namespace, blob per struct + u8 lang.
#include "deskpet_store.h"

#include "nvs.h"
#include "nvs_flash.h"

#define STORE_NS "deskpet"

static esp_err_t read_blob(nvs_handle_t h, const char *key, void *out, size_t len)
{
    size_t need = len;
    esp_err_t err = nvs_get_blob(h, key, out, &need);
    return (err == ESP_OK && need == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t deskpet_store_load(pet_save_t *pet, dungeon_save_t *dun,
                             pomo_t *pomo, deskpet_lang_t *lang)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition layout changed upstream; demo_radio owns shared init policy,
        // but a minimal erase here keeps the demo page self-healing (no user data loss
        // beyond deskpet saves).
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;
    nvs_handle_t h;
    err = nvs_open(STORE_NS, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    // Each blob loads on its own: a version bump of one model must not wipe
    // the others (callers pre-fill defaults). Return value reports the game
    // save, which is what "loaded" means to the shell.
    pet_save_t p; dungeon_save_t d; pomo_t m; uint8_t l = LANG_EN;
    if (read_blob(h, "pet", &p, sizeof(p)) == ESP_OK && p.version == PET_MODEL_VERSION) *pet = p;
    bool dun_ok = read_blob(h, "dun", &d, sizeof(d)) == ESP_OK && d.version == DUNGEON_MODEL_VERSION;
    if (dun_ok) *dun = d;
    if (read_blob(h, "pomo", &m, sizeof(m)) == ESP_OK && m.version == POMO_LITE_VERSION) *pomo = m;
    if (nvs_get_u8(h, "lang", &l) != ESP_OK) l = LANG_EN;
    nvs_close(h);
    *lang = (l == LANG_ZH) ? LANG_ZH : LANG_EN;
    return dun_ok ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t deskpet_store_save(const pet_save_t *pet, const dungeon_save_t *dun,
                             const pomo_t *pomo, deskpet_lang_t lang)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORE_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "pet", pet, sizeof(*pet));
    if (err == ESP_OK) err = nvs_set_blob(h, "dun", dun, sizeof(*dun));
    if (err == ESP_OK) err = nvs_set_blob(h, "pomo", pomo, sizeof(*pomo));
    if (err == ESP_OK) err = nvs_set_u8(h, "lang", (uint8_t)lang);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
