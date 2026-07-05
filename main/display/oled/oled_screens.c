/* oled_screens.c — Manager d'écrans OLED piloté par oled_nav.
 *
 * Registre statique des 5 écrans (SPLASH, HOME, LAYER, STATS, TAMA).
 * Sur chaque tick : compare l'écran demandé par oled_nav_active() avec
 * l'écran courant ; si différent → destroy + build ; puis update().
 * Le lock LVGL est tenu par l'appelant (pas de lvgl_port_lock ici).
 *
 * oled_make_card() est défini ici (repris de l'ancien oled_backend.c)
 * et exporté via screens/oled_screen.h pour que les écrans puissent
 * l'appeler dans leur build().
 */
#include "oled_screens.h"
#include "oled_nav.h"
#include "oled_kpm.h"
#include "screens/oled_screen.h"
#include "tama_engine.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

/* ── Externs des 5 écrans (définis dans Tasks 8-12) ────────────────── */
extern const oled_screen_t screen_splash;
extern const oled_screen_t screen_home;
extern const oled_screen_t screen_layer;
extern const oled_screen_t screen_stats;
extern const oled_screen_t screen_tama;

/* ── Registre statique ──────────────────────────────────────────────── */
static const oled_screen_t *s_screens[OLED_SCR_COUNT] = {
    [OLED_SCR_SPLASH] = &screen_splash,
    [OLED_SCR_HOME]   = &screen_home,
    [OLED_SCR_LAYER]  = &screen_layer,
    [OLED_SCR_STATS]  = &screen_stats,
    [OLED_SCR_TAMA]   = &screen_tama,
};

static oled_screen_id_t s_current = OLED_SCR_COUNT; /* aucun écran construit */

/* ── oled_make_card (déplacé depuis l'ancien oled_backend) ──────────── */

lv_obj_t *oled_make_card(lv_obj_t *parent, int x, int y, int w, int h, int radius)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_opa(card, LV_OPA_0, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0); /* black = pixel ON = lit */
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

/* ── Manager ────────────────────────────────────────────────────────── */

void oled_screens_reset(uint32_t now_ms)
{
    if (s_current < OLED_SCR_COUNT)
        s_screens[s_current]->destroy();
    s_current = OLED_SCR_COUNT;

    oled_kpm_reset();
    oled_nav_init(now_ms);
    oled_nav_set_tama_enabled(tama_engine_is_enabled());
}

void oled_screens_tick(uint32_t now_ms)
{
    oled_kpm_tick(now_ms);

    oled_screen_id_t id = oled_nav_active(now_ms);

    /* Rebuilder uniquement sur changement d'écran (§3.3). */
    if (id != s_current) {
        if (s_current < OLED_SCR_COUNT)
            s_screens[s_current]->destroy();
        /* Surface propre avant le build : certains renderers (tama_render) ne
         * suppriment pas eux-mêmes leurs objets LVGL dans destroy() → sans ce
         * clean, un reliquat de l'écran précédent resterait par-dessus le
         * nouveau. Garantit une table rase quel que soit le screen sortant. */
        lv_obj_clean(lv_scr_act());
        lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);  /* pas de scrollbar/pan */
        s_screens[id]->build(lv_scr_act());
        s_current = id;
    }

    s_screens[s_current]->update();
}

void oled_screens_layer_changed(uint32_t now_ms) { oled_nav_event(OLED_EV_LAYER_CHANGED, now_ms); }
void oled_screens_disp_key(uint32_t now_ms)      { oled_nav_event(OLED_EV_DISP_KEY,     now_ms); }
void oled_screens_activity(uint32_t now_ms)      { oled_nav_event(OLED_EV_ACTIVITY,      now_ms); }
