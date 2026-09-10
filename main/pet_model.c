// main/pet_model.c —— priority: SLEEP > LOWBAT > FOCUS > CELEBRATE > BUSY > SAD > HAPPY > NORMAL.
#include "pet_model.h"

void pet_save_defaults(pet_save_t *s)
{
    s->version = PET_MODEL_VERSION;
    s->growth = 0;
    s->poke_count = 0;
    s->muted = 0;
    s->reserved[0] = s->reserved[1] = s->reserved[2] = 0;
}

pet_mood_t pet_mood_resolve(const pet_triggers_t *t)
{
    if (t->sleeping) return PET_MOOD_SLEEP;
    if (t->battery_low) return PET_MOOD_LOWBAT;
    if (t->pomo_running) return PET_MOOD_FOCUS;
    if (t->pomo_celebrate) return PET_MOOD_CELEBRATE;
    if (t->pc_busy) return PET_MOOD_BUSY;
    if (t->bad_news) return PET_MOOD_SAD;
    if (t->poked || t->idle_ms < 3000) return PET_MOOD_HAPPY;
    if (t->idle_ms >= PET_IDLE_SLEEP_MS) return PET_MOOD_SLEEP;
    return PET_MOOD_NORMAL;
}

const char *pet_mood_name_en(pet_mood_t m)
{
    static const char *const names[PET_MOOD_COUNT] = {
        "NORMAL", "HAPPY", "SAD", "BUSY",
        "CELEBRATE", "FOCUS", "LOWBAT", "SLEEP",
    };
    return (m < PET_MOOD_COUNT) ? names[m] : "?";
}
