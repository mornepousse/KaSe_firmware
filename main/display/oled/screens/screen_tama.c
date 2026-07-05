/* screen_tama.c — Écran TAMA : gros pet centré + niveau/xp en haut.
 *
 * Layout 128×64 (le sprite est upscalé+centré à y=16 par tama_render, OLED) :
 *   y=0..14  : "Lv<n>  <xp>xp"  (haut-gauche)
 *   y=16..62 : PET tama en grand (tama_render)
 *
 * Plus de barres de progression (retirées à la demande). Écran de "focus" du
 * pet avec son niveau ; le sprite est géré par tama_render.
 */
#include "oled_screen.h"
#include "lvgl.h"
#include "board.h"
#include "tama_render.h"
#include "tama_engine.h"

static lv_obj_t *s_level_lbl = NULL;

static void build(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    s_level_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(s_level_lbl, UI_FONT, 0);
    lv_label_set_text(s_level_lbl, "Lv0  0xp");
    lv_obj_set_pos(s_level_lbl, 2, 0);

    tama_render_create(parent, BOARD_DISPLAY_WIDTH, BOARD_DISPLAY_HEIGHT);

    const tama2_stats_t *st = tama_engine_get_stats();
    tama_render_update(tama_engine_get_state(), st, tama_engine_get_critter());
    if (st && s_level_lbl)
        lv_label_set_text_fmt(s_level_lbl, "Lv%u  %uxp", (unsigned)st->level, (unsigned)st->xp);
}

static void update(void)
{
    const tama2_stats_t *st = tama_engine_get_stats();
    tama_render_update(tama_engine_get_state(), st, tama_engine_get_critter());
    if (st && s_level_lbl)
        lv_label_set_text_fmt(s_level_lbl, "Lv%u  %uxp", (unsigned)st->level, (unsigned)st->xp);
}

static void destroy(void)
{
    tama_render_destroy();
    if (s_level_lbl && lv_obj_is_valid(s_level_lbl)) lv_obj_del(s_level_lbl);
    s_level_lbl = NULL;
}

const oled_screen_t screen_tama = { build, update, destroy };
