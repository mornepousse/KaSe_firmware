#pragma once
/* oled_screens.h — Manager d'écrans OLED piloté par oled_nav.
 *
 * Utilisation (depuis status_display / oled_backend — câblé en Task 13) :
 *   oled_screens_reset(now_ms)        — à l'init / au réveil
 *   oled_screens_tick(now_ms)         — chaque frame, lock LVGL tenu par l'appelant
 *   oled_screens_layer_changed(now)   — forwarde OLED_EV_LAYER_CHANGED à oled_nav
 *   oled_screens_disp_key(now)        — forwarde OLED_EV_DISP_KEY à oled_nav
 *   oled_screens_activity(now)        — forwarde OLED_EV_ACTIVITY à oled_nav
 */
#include <stdint.h>

/* Initialise la navigation et détruit l'écran courant s'il existe.
 * À appeler avec le lock LVGL tenu. */
void oled_screens_reset(uint32_t now_ms);

/* Avance le KPM, détecte un changement d'écran actif (nav), build/destroy si besoin,
 * puis appelle update() sur l'écran courant.
 * Doit être appelé avec le lock LVGL tenu. */
void oled_screens_tick(uint32_t now_ms);

/* Événements de navigation (forwarded à oled_nav_event). */
void oled_screens_layer_changed(uint32_t now_ms);
void oled_screens_disp_key(uint32_t now_ms);
void oled_screens_activity(uint32_t now_ms);
