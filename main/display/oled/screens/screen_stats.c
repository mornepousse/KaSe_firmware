/* screen_stats.c — Écran STATS : KPM / WPM / sparkline / total frappes.
 *
 * Layout 128×64 SSD1306 mono (lv_color_black() = pixel allumé) :
 *
 *   y=0..13  : top — "KPM NNN" (gauche) + "NN wpm" (droite)
 *   y=15..50 : sparkline — SS_N_BARS barres fines (fond allumé, croît vers le haut)
 *   y=52..63 : bottom — total frappes "1.2M keys"
 *
 * Contrat build/update/destroy :
 *   build(parent)  — crée les objets ; parent = lv_scr_act(), lock déjà tenu.
 *   update()       — rafraîchit les valeurs dynamiques, pas de création d'objets.
 *   destroy()      — lv_obj_del de chaque objet non-NULL (labels + les N barres), NULL.
 *
 * Le lock LVGL est tenu par l'appelant (oled_screens_tick) — pas d'auto-lock ici.
 */

#include "oled_screen.h"
#include "lvgl.h"
#include "board.h"
#include "oled_kpm.h"
#include "oled_stats.h"
#include "key_stats.h"
#include <stdio.h>
#include <stdint.h>

/* ── Constantes de layout ─────────────────────────────────────────────── */

#define SS_N_BARS           20     /* nombre de barres de la sparkline         */
#define SS_BAR_W             5     /* largeur d'une barre (px)                 */
#define SS_BAR_GAP           1     /* gap entre barres (px)                    */
#define SS_SPARK_X           4     /* x de départ de la première barre         */
/* Largeur totale sparkline = SS_N_BARS*SS_BAR_W + (SS_N_BARS-1)*SS_BAR_GAP
 *   = 20*5 + 19*1 = 119 px  →  end x = 4+119 = 123 < 128  ✓               */

#define SS_TOP_H            14     /* hauteur zone top (px)                    */
#define SS_SPARK_Y          15     /* y de départ de la zone sparkline         */
#define SS_SPARK_H          36     /* hauteur de la zone sparkline (px)        */
#define SS_SPARK_BOTTOM     (SS_SPARK_Y + SS_SPARK_H) /* = 51, ligne de base  */
#define SS_PX_PER_UNIT       5     /* px par unité barre (0..7 → 0..35 px)    */
#define SS_BOTTOM_Y         52     /* y de la zone total keystrokes            */

/* ── Pointeurs statiques vers les objets LVGL ────────────────────────── */

static lv_obj_t *s_kpm_label   = NULL;
static lv_obj_t *s_wpm_label   = NULL;
static lv_obj_t *s_total_label = NULL;
static lv_obj_t *s_bars[SS_N_BARS];  /* barres de la sparkline */

/* ── Helper : formate le total en K/M sans virgule flottante ─────────── */

static void format_total(char *buf, size_t n, uint32_t v)
{
    if (v >= 1000000u) {
        uint32_t m = v / 1000000u;
        uint32_t d = (v % 1000000u) / 100000u;  /* premier chiffre décimal */
        snprintf(buf, n, "%lu.%luM keys", (unsigned long)m, (unsigned long)d);
    } else if (v >= 1000u) {
        snprintf(buf, n, "%luK keys", (unsigned long)(v / 1000u));
    } else {
        snprintf(buf, n, "%lu keys", (unsigned long)v);
    }
}

/* ── build ────────────────────────────────────────────────────────────── */

static void build(lv_obj_t *parent)
{
    /* Initialise le tableau avant remplissage (sécurité destroy partielle) */
    for (int i = 0; i < SS_N_BARS; i++) s_bars[i] = NULL;

    /* Zone top gauche : valeur KPM */
    s_kpm_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_kpm_label, UI_FONT, 0);
    lv_label_set_text(s_kpm_label, "KPM --");
    lv_obj_set_pos(s_kpm_label, 2, 0);

    /* Zone top droite : valeur WPM (alignée à droite) */
    s_wpm_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_wpm_label, UI_FONT, 0);
    lv_label_set_text(s_wpm_label, "-- wpm");
    /* Positionné à BOARD_DISPLAY_WIDTH - 58 pour laisser place à "400 wpm" */
    lv_obj_set_pos(s_wpm_label, BOARD_DISPLAY_WIDTH - 58, 0);

    /* Sparkline : SS_N_BARS rectangles fins, fond allumé, croissant vers le haut */
    for (int i = 0; i < SS_N_BARS; i++) {
        lv_obj_t *bar = lv_obj_create(parent);
        lv_obj_remove_style_all(bar);
        lv_obj_set_style_bg_color(bar, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        /* Taille initiale : 1 px de hauteur (pas de création dans update) */
        lv_obj_set_size(bar, SS_BAR_W, 1);
        int bx = SS_SPARK_X + i * (SS_BAR_W + SS_BAR_GAP);
        lv_obj_set_pos(bar, bx, SS_SPARK_BOTTOM - 1);
        s_bars[i] = bar;
    }

    /* Zone bottom : total frappes */
    s_total_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_total_label, UI_FONT, 0);
    lv_label_set_text(s_total_label, "0 keys");
    lv_obj_set_pos(s_total_label, 2, SS_BOTTOM_Y);
}

/* ── update ───────────────────────────────────────────────────────────── */

static void update(void)
{
    uint32_t kpm = oled_kpm_value();
    uint32_t wpm = oled_wpm_from_kpm(kpm);

    /* 1. Labels KPM / WPM ─────────────────────────────────────────────── */
    if (s_kpm_label)
        lv_label_set_text_fmt(s_kpm_label, "KPM %lu", (unsigned long)kpm);
    if (s_wpm_label)
        lv_label_set_text_fmt(s_wpm_label, "%lu wpm", (unsigned long)wpm);

    /* 2. Sparkline ────────────────────────────────────────────────────── */
    {
        const uint32_t *hist = oled_kpm_history();
        uint8_t heights[SS_N_BARS];
        oled_sparkline_bars(hist, (int)OLED_KPM_WINDOW, OLED_KPM_MAX,
                            heights, SS_N_BARS);
        for (int i = 0; i < SS_N_BARS; i++) {
            if (!s_bars[i]) continue;
            int bar_h = (int)heights[i] * SS_PX_PER_UNIT;
            if (bar_h < 1) bar_h = 1;  /* plancher 1 px (toujours visible) */
            int bx = SS_SPARK_X + i * (SS_BAR_W + SS_BAR_GAP);
            lv_obj_set_size(s_bars[i], SS_BAR_W, bar_h);
            lv_obj_set_pos(s_bars[i],  bx, SS_SPARK_BOTTOM - bar_h);
        }
    }

    /* 3. Total frappes ────────────────────────────────────────────────── */
    if (s_total_label) {
        char buf[24];
        format_total(buf, sizeof(buf), key_stats_total);
        lv_label_set_text(s_total_label, buf);
    }
}

/* ── destroy ──────────────────────────────────────────────────────────── */

static void destroy(void)
{
    if (s_kpm_label)   { lv_obj_del(s_kpm_label);   s_kpm_label   = NULL; }
    if (s_wpm_label)   { lv_obj_del(s_wpm_label);   s_wpm_label   = NULL; }
    if (s_total_label) { lv_obj_del(s_total_label); s_total_label = NULL; }
    for (int i = 0; i < SS_N_BARS; i++) {
        if (s_bars[i]) { lv_obj_del(s_bars[i]); s_bars[i] = NULL; }
    }
}

const oled_screen_t screen_stats = { build, update, destroy };
