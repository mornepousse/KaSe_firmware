/* screen_layer.c — Écran LAYER flash : nom de la couche courante en grand.
 *
 * Affiché ~2.5 s lors d'un changement de couche.
 * Layout 128×64 mono :
 *
 *   y≈10 : grand label centré = default_layout_names[current_layout]
 *            (lv_font_montserrat_28 ; LV_LABEL_LONG_DOT si nom trop long)
 *   y≈48 : sous-titre = "> layer N" (UI_FONT)
 *
 * Contrat build/update/destroy : lock LVGL tenu par l'appelant → pas de
 * self-lock ici.  Compilé mais non câblé avant Task 13.
 */

#include "oled_screen.h"
#include "lvgl.h"
#include "board.h"
#include "keyboard_config.h"   /* extern uint8_t current_layout */
#include "keymap.h"             /* default_layout_names */

LV_FONT_DECLARE(lv_font_montserrat_28);

/* ── Constantes de layout ─────────────────────────────────────────────── */

#define SL_BIG_Y  10   /* décalage y du grand label depuis le haut (px) */
#define SL_SUB_Y  48   /* y absolu du sous-titre (px) */

/* ── Pointeurs statiques vers les objets LVGL ────────────────────────── */

static lv_obj_t *s_big_label = NULL;
static lv_obj_t *s_sub_label = NULL;

/* ── build ────────────────────────────────────────────────────────────── */

static void build(lv_obj_t *parent)
{
    /* Grand label : nom de couche, centré, police 28 px, troncature "…" */
    s_big_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_big_label, &lv_font_montserrat_28, 0);
    lv_label_set_long_mode(s_big_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_big_label, BOARD_DISPLAY_WIDTH);
    lv_obj_set_style_text_align(s_big_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_big_label, default_layout_names[current_layout]);
    lv_obj_align(s_big_label, LV_ALIGN_TOP_MID, 0, SL_BIG_Y);

    /* Sous-titre : "> layer N", police UI, centré */
    s_sub_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_sub_label, UI_FONT, 0);
    lv_obj_set_style_text_align(s_sub_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_sub_label, BOARD_DISPLAY_WIDTH);
    lv_label_set_text_fmt(s_sub_label, "> layer %d", current_layout);
    lv_obj_set_pos(s_sub_label, 0, SL_SUB_Y);
}

/* ── update ───────────────────────────────────────────────────────────── */

static void update(void)
{
    if (s_big_label) {
        lv_label_set_text(s_big_label, default_layout_names[current_layout]);
        lv_obj_align(s_big_label, LV_ALIGN_TOP_MID, 0, SL_BIG_Y);
    }
    if (s_sub_label) {
        lv_label_set_text_fmt(s_sub_label, "> layer %d", current_layout);
    }
}

/* ── destroy ──────────────────────────────────────────────────────────── */

static void destroy(void)
{
    if (s_big_label && lv_obj_is_valid(s_big_label)) lv_obj_del(s_big_label);
    s_big_label = NULL;
    if (s_sub_label && lv_obj_is_valid(s_sub_label)) lv_obj_del(s_sub_label);
    s_sub_label = NULL;
}

const oled_screen_t screen_layer = { build, update, destroy };
