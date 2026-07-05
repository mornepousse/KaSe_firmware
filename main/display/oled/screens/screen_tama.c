/* screen_tama.c — Écran TAMA (128×64 mono SSD1306).
 *
 * IMPORTANT : sur OLED, tama_render (tama_render.c) dessine le sprite 32×32
 * FIXÉ en bas-droite à (96,20) → il occupe x=96..128, y=20..52, et ne dessine
 * PAS de barres. Donc nos barres/niveau vivent dans la COLONNE GAUCHE (x=2..90)
 * pour ne pas chevaucher le sprite.
 *
 * Layout :
 *   sprite    : x=96..128, y=20..52   (tama_render, à droite)
 *   y=24..30  : barre faim    (gauche, x=2..90)
 *   y=38..44  : barre bonheur (gauche, x=2..90)
 *   y=50..63  : label "Lv<n> <xp>xp" (gauche)
 *
 * Contrat build/update/destroy :
 *   build(parent)  — lock déjà tenu par l'appelant (manager).
 *   update()       — pas de création d'objets, lock déjà tenu.
 *   destroy()      — tama_render_destroy() + lv_obj_del barres/labels + NULL.
 */

#include "oled_screen.h"
#include "lvgl.h"
#include "board.h"
#include "tama_render.h"
#include "tama_engine.h"

/* ── Constantes de layout ────────────────────────────────────────────────── */

#define ST_BAR_X         2   /* colonne gauche — le sprite occupe x=96..128 */
#define ST_BAR_W        88   /* x=2..90, s'arrête avant le sprite (x=96)      */
#define ST_BAR_H         7
#define ST_BAR_RADIUS    2
#define ST_HUNGER_Y     24
#define ST_HAPPY_Y      38
#define ST_LABEL_Y      50   /* font_14 → 50..64 */
#define ST_STAT_MAX     TAMA2_STAT_MAX   /* 1000 */

/* ── Pointeurs statiques ─────────────────────────────────────────────────── */

static lv_obj_t *s_hunger_lbl  = NULL;
static lv_obj_t *s_hunger_bar  = NULL;
static lv_obj_t *s_happy_lbl   = NULL;
static lv_obj_t *s_happy_bar   = NULL;
static lv_obj_t *s_level_lbl   = NULL;

/* ── Helpers internes ────────────────────────────────────────────────────── */

static lv_obj_t *make_stat_bar(lv_obj_t *parent, int y)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, ST_BAR_W, ST_BAR_H);
    lv_obj_set_pos(bar, ST_BAR_X, y);
    lv_bar_set_range(bar, 0, ST_STAT_MAX);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    /* Fond : fond transparent, bordure allumée */
    lv_obj_set_style_bg_opa(bar,      LV_OPA_0,     LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1,             LV_PART_MAIN);
    lv_obj_set_style_border_opa(bar,  LV_OPA_COVER,  LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar,     0,             LV_PART_MAIN);
    lv_obj_set_style_radius(bar,      ST_BAR_RADIUS, LV_PART_MAIN);
    /* Indicateur : plein, allumé */
    lv_obj_set_style_bg_color(bar,    lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar,      LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar,      ST_BAR_RADIUS, LV_PART_INDICATOR);
    return bar;
}

static lv_obj_t *make_stat_label(lv_obj_t *parent, const char *text, int y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, UI_FONT, 0);
    lv_label_set_text(lbl, text);
    lv_obj_set_pos(lbl, 0, y);
    return lbl;
}

/* ── build ───────────────────────────────────────────────────────────────── */

static void build(lv_obj_t *parent)
{
    /* Sprite tama — prend toute la surface ; le renderer se positionne
     * lui-même dans les 44 premières lignes environ selon le critter. */
    tama_render_create(parent, BOARD_DISPLAY_WIDTH, BOARD_DISPLAY_HEIGHT);

    /* Barres faim / bonheur (pleine largeur, pas de label H/J — trop haut en
     * font_14 et écrasait la barre ; la faim est en haut, le bonheur en dessous). */
    s_hunger_bar = make_stat_bar(parent, ST_HUNGER_Y);
    s_happy_bar  = make_stat_bar(parent, ST_HAPPY_Y);

    /* Label niveau + XP */
    s_level_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(s_level_lbl, UI_FONT, 0);
    lv_label_set_text(s_level_lbl, "Lv0  0xp");
    lv_obj_set_pos(s_level_lbl, 0, ST_LABEL_Y);

    /* Première mise à jour immédiate pour ne pas afficher de vide */
    const tama2_stats_t *st = tama_engine_get_stats();
    tama_render_update(tama_engine_get_state(), st, tama_engine_get_critter());
    if (st) {
        if (s_hunger_bar)
            lv_bar_set_value(s_hunger_bar, (int32_t)st->hunger, LV_ANIM_OFF);
        if (s_happy_bar)
            lv_bar_set_value(s_happy_bar, (int32_t)st->happiness, LV_ANIM_OFF);
        if (s_level_lbl)
            lv_label_set_text_fmt(s_level_lbl, "Lv%u  %uxp",
                                  (unsigned)st->level, (unsigned)st->xp);
    }
}

/* ── update ──────────────────────────────────────────────────────────────── */

static void update(void)
{
    const tama2_stats_t *st = tama_engine_get_stats();
    tama_render_update(tama_engine_get_state(), st, tama_engine_get_critter());

    if (!st) return;

    if (s_hunger_bar)
        lv_bar_set_value(s_hunger_bar, (int32_t)st->hunger, LV_ANIM_OFF);
    if (s_happy_bar)
        lv_bar_set_value(s_happy_bar, (int32_t)st->happiness, LV_ANIM_OFF);
    if (s_level_lbl)
        lv_label_set_text_fmt(s_level_lbl, "Lv%u  %uxp",
                              (unsigned)st->level, (unsigned)st->xp);
}

/* ── destroy ─────────────────────────────────────────────────────────────── */

static void destroy(void)
{
    tama_render_destroy();

    if (s_level_lbl  && lv_obj_is_valid(s_level_lbl))  lv_obj_del(s_level_lbl);
    s_level_lbl = NULL;
    if (s_happy_bar  && lv_obj_is_valid(s_happy_bar))  lv_obj_del(s_happy_bar);
    s_happy_bar = NULL;
    if (s_happy_lbl  && lv_obj_is_valid(s_happy_lbl))  lv_obj_del(s_happy_lbl);
    s_happy_lbl = NULL;
    if (s_hunger_bar && lv_obj_is_valid(s_hunger_bar)) lv_obj_del(s_hunger_bar);
    s_hunger_bar = NULL;
    if (s_hunger_lbl && lv_obj_is_valid(s_hunger_lbl)) lv_obj_del(s_hunger_lbl);
    s_hunger_lbl = NULL;
}

const oled_screen_t screen_tama = { build, update, destroy };
