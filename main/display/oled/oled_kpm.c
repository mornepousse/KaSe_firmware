/* oled_kpm.c — Fenêtre glissante KPM (extrait de l'ancien oled_backend). */
#include "oled_kpm.h"
#include <string.h>

static uint32_t s_count;                        /* frappes depuis le dernier échantillon */
static uint32_t s_history[OLED_KPM_WINDOW];    /* historique circulaire (valeurs/s) */
static int      s_idx;                          /* prochain index d'écriture */
static uint32_t s_current_kpm;                 /* somme des OLED_KPM_WINDOW derniers */
static uint32_t s_last_sample_ms;              /* horodatage du dernier échantillon */

void oled_kpm_reset(void)
{
    s_count          = 0;
    s_idx            = 0;
    s_current_kpm    = 0;
    s_last_sample_ms = 0;
    memset(s_history, 0, sizeof(s_history));
}

void oled_kpm_notify_keypress(void)
{
    s_count++;
}

void oled_kpm_tick(uint32_t now_ms)
{
    /* Premier appel après reset : ancrer le compteur de temps. */
    if (s_last_sample_ms == 0) {
        s_last_sample_ms = now_ms;
        return;
    }

    if ((now_ms - s_last_sample_ms) < OLED_KPM_SAMPLE_MS) return;

    /* Pousser l'échantillon courant dans la fenêtre circulaire. */
    s_history[s_idx] = s_count;
    s_idx            = (s_idx + 1) % (int)OLED_KPM_WINDOW;
    s_count          = 0;
    s_last_sample_ms = now_ms;

    /* Recalcul du KPM : somme de toute la fenêtre. */
    uint32_t total = 0;
    for (int i = 0; i < (int)OLED_KPM_WINDOW; i++) total += s_history[i];
    s_current_kpm = total;
}

uint32_t oled_kpm_value(void)
{
    return s_current_kpm;
}

const uint32_t *oled_kpm_history(void)
{
    return s_history;
}
