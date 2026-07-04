#pragma once
/* oled_screen.h — Interface commune pour tous les écrans OLED (SSD1306 mono).
 *
 * Convention mono : lv_color_black() = pixel allumé (lit).
 * Le lock LVGL est tenu par l'appelant (oled_screens_tick) avant build/update/destroy.
 */
#include "lvgl.h"

/* Vtable d'un écran OLED. */
typedef struct {
    void (*build)(lv_obj_t *parent);  /* crée les objets LVGL ; parent = lv_scr_act() */
    void (*update)(void);             /* rafraîchit le contenu (lock déjà tenu) */
    void (*destroy)(void);            /* supprime les objets et met les ptr à NULL */
} oled_screen_t;

/* Crée un objet carte LVGL (bordure lit, fond transparent, padding nul).
 * Doit être appelé avec le lock LVGL tenu. */
lv_obj_t *oled_make_card(lv_obj_t *parent, int x, int y, int w, int h, int radius);
