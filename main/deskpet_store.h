// main/deskpet_store.h —— NVS persistence for pet/dungeon/pomo/lang. Thin wrapper.
#pragma once

#include "esp_err.h"
#include "pet_model.h"
#include "dungeon_model.h"
#include "pomo_lite.h"
#include "deskpet_i18n.h"

esp_err_t deskpet_store_load(pet_save_t *pet, dungeon_save_t *dun,
                             pomo_t *pomo, deskpet_lang_t *lang);
esp_err_t deskpet_store_save(const pet_save_t *pet, const dungeon_save_t *dun,
                             const pomo_t *pomo, deskpet_lang_t lang);
