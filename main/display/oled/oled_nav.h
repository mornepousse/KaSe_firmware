#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    OLED_SCR_SPLASH, OLED_SCR_HOME,
    OLED_SCR_STATS,  OLED_SCR_TAMA,  OLED_SCR_COUNT
} oled_screen_id_t;

typedef enum {
    OLED_EV_BOOT, OLED_EV_LAYER_CHANGED, OLED_EV_DISP_KEY, OLED_EV_ACTIVITY
} oled_nav_event_t;

#define OLED_NAV_SPLASH_MS  2000u
#define OLED_NAV_IDLE_MS    10000u   /* inactivité avant écran TAMA (screensaver).
                                        DOIT rester < BOARD_DISPLAY_SLEEP_MS, sinon
                                        l'écran s'éteint avant d'afficher le TAMA. */

/* Réinitialise la machine à états. resting=HOME. N'ARME PAS le splash (c'est
   OLED_EV_BOOT qui l'arme, uniquement au vrai démarrage) → pas de splash au
   réveil/refresh. Ne touche PAS l'état tama_enabled (piloté séparément). */
void oled_nav_init(uint32_t now_ms);
void oled_nav_event(oled_nav_event_t ev, uint32_t now_ms);
oled_screen_id_t oled_nav_active(uint32_t now_ms);
void oled_nav_set_tama_enabled(bool en);
