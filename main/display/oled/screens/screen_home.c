/* screen_home.c — HOME : tableau de bord DENSE (tout visible d'un coup).
 *
 * Layout 128×64 SSD1306 mono (lv_color_black() = pixel allumé), font_14 texte :
 *
 *   y=0..15  : [path 16×16][bt 16×16] slot ................... CAP
 *   y=16     : séparateur (ligne)
 *   y=18..32 : nom de couche (gauche, tronqué …)  ......... "L5" (droite)
 *   y=34..48 : "520kpm  104wpm"
 *   y=50..55 : barre faim tama    (si tama activé, pleine largeur)
 *   y=57..62 : barre bonheur tama (si tama activé)
 *
 * Icônes = vrais assets 16×16 (flash/USB, wifi/signal, bluetooth). Noms longs
 * tronqués (LONG_DOT + largeur bornée). Rien ne dépasse des 64 px.
 */
#include "oled_screen.h"
#include "lvgl.h"
#include "board.h"
#include "imgs.h"
#include "keyboard_config.h"
#include "keymap.h"
#include "hid_bluetooth_manager.h"
#include "hid_report.h"
#include "usb_hid.h"
#include "oled_kpm.h"
#include "oled_stats.h"
#include "tama_engine.h"
#if CONFIG_KASE_KBD_WIRELESS
#include "usb_presence.h"
#endif

#define SH_STATUS_H     16   /* icônes 16×16 → barre de 16px, séparateur dessous */
#define SH_ICON_PATH_X   0
#define SH_ICON_BT_X    18   /* 0 + 16 + 2 gap : pas de chevauchement d'icônes    */
#define SH_BT_SLOT_X    36
#define SH_LAYER_X       2
#define SH_LAYER_Y      18
#define SH_LAYER_W      88   /* largeur bornée → noms longs tronqués (…)         */
#define SH_SUB_Y        34
#define SH_BAR_X         2
#define SH_BAR_W       (BOARD_DISPLAY_WIDTH - SH_BAR_X - 2)
#define SH_BAR_H         5
#define SH_HUNGER_Y     50
#define SH_HAPPY_Y      57

static lv_obj_t *s_sep         = NULL;
static lv_obj_t *s_icon_path   = NULL;
static lv_obj_t *s_icon_bt     = NULL;
static lv_obj_t *s_bt_slot     = NULL;
static lv_obj_t *s_caps        = NULL;
static lv_obj_t *s_layer_label = NULL;
static lv_obj_t *s_sub_label   = NULL;
static lv_obj_t *s_kpm_label   = NULL;
static lv_obj_t *s_wpm_label   = NULL;
static lv_obj_t *s_hunger_bar  = NULL;
static lv_obj_t *s_happy_bar   = NULL;

static lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h, int max)
{
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_pos(b, x, y);
    lv_bar_set_range(b, 0, max);
    lv_bar_set_value(b, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(b, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(b, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(b, lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(b, 0, LV_PART_INDICATOR);
    return b;
}

static lv_obj_t *make_lbl(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, UI_FONT, 0);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, "");
    return l;
}

static void build(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);   /* pas de scrollbar */

    s_sep = lv_obj_create(parent);
    lv_obj_set_size(s_sep, BOARD_DISPLAY_WIDTH, 1);
    lv_obj_set_pos(s_sep, 0, SH_STATUS_H);
    lv_obj_set_style_bg_color(s_sep, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_sep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_sep, 0, 0);
    lv_obj_set_style_radius(s_sep, 0, 0);

    s_icon_path = lv_img_create(parent);
    lv_obj_set_pos(s_icon_path, SH_ICON_PATH_X, 0);
    s_icon_bt = lv_img_create(parent);
    lv_obj_set_pos(s_icon_bt, SH_ICON_BT_X, 0);
    lv_obj_add_flag(s_icon_bt, LV_OBJ_FLAG_HIDDEN);

    s_bt_slot = make_lbl(parent, SH_BT_SLOT_X, 0);

    s_caps = make_lbl(parent, 0, 0);
    lv_label_set_text(s_caps, "CAP");
    lv_obj_align(s_caps, LV_ALIGN_TOP_RIGHT, -1, 0);
    lv_obj_add_flag(s_caps, LV_OBJ_FLAG_HIDDEN);

    s_layer_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_layer_label, UI_FONT, 0);
    lv_label_set_long_mode(s_layer_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_layer_label, SH_LAYER_W);
    lv_obj_set_pos(s_layer_label, SH_LAYER_X, SH_LAYER_Y);

    s_sub_label = make_lbl(parent, 0, SH_LAYER_Y);   /* "L5" aligné à droite, ligne 1 */
    s_kpm_label = make_lbl(parent, SH_LAYER_X, SH_SUB_Y);  /* "NNN KPM  MMM WPM" ligne 2 */
    s_wpm_label = NULL;

    if (tama_engine_is_enabled()) {
        s_hunger_bar = make_bar(parent, SH_BAR_X, SH_HUNGER_Y, SH_BAR_W, SH_BAR_H, TAMA2_STAT_MAX);
        s_happy_bar  = make_bar(parent, SH_BAR_X, SH_HAPPY_Y,  SH_BAR_W, SH_BAR_H, TAMA2_STAT_MAX);
    }
}

static void update(void)
{
#if CONFIG_KASE_KBD_WIRELESS
    if (s_icon_path) lv_img_set_src(s_icon_path, (kbd_active_route() == KBD_OUT_RF) ? &wifi : &flash);
#else
    if (s_icon_path) lv_img_set_src(s_icon_path, (keyboard_get_usb_bl_state() != 0) ? &bluetooth_16px : &flash);
#endif

    bool bt_init = hid_bluetooth_is_initialized();
    bool bt_conn = hid_bluetooth_is_connected();
    bool bt_pair = hid_bluetooth_is_pairing();
    if (s_icon_bt) {
        if (bt_init && bt_conn) { lv_obj_clear_flag(s_icon_bt, LV_OBJ_FLAG_HIDDEN); lv_img_set_src(s_icon_bt, &wifi); }
        else                      lv_obj_add_flag(s_icon_bt, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_bt_slot) {
        if (bt_init && bt_pair)  lv_label_set_text(s_bt_slot, "P");
        else if (bt_init)        lv_label_set_text_fmt(s_bt_slot, "%d", (int)(bt_get_active_slot() + 1));
        else                     lv_label_set_text(s_bt_slot, "");
    }
    if (s_caps) {
        if (hid_led_state & HID_LED_CAPS_LOCK) lv_obj_clear_flag(s_caps, LV_OBJ_FLAG_HIDDEN);
        else                                    lv_obj_add_flag(s_caps, LV_OBJ_FLAG_HIDDEN);
    }

    if (s_layer_label) lv_label_set_text(s_layer_label, default_layout_names[current_layout]);
    if (s_sub_label) {
        lv_label_set_text_fmt(s_sub_label, "L%d", (int)current_layout);
        lv_obj_align(s_sub_label, LV_ALIGN_TOP_RIGHT, -1, SH_LAYER_Y);   /* re-cale à droite */
    }

    uint32_t kpm = oled_kpm_value();
    if (s_kpm_label) lv_label_set_text_fmt(s_kpm_label, "%lukpm  %luwpm",
                                           (unsigned long)kpm, (unsigned long)oled_wpm_from_kpm(kpm));

    const tama2_stats_t *st = (s_hunger_bar || s_happy_bar) ? tama_engine_get_stats() : NULL;
    if (st) {
        if (s_hunger_bar) lv_bar_set_value(s_hunger_bar, st->hunger, LV_ANIM_OFF);
        if (s_happy_bar)  lv_bar_set_value(s_happy_bar,  st->happiness, LV_ANIM_OFF);
    }
}

static void destroy(void)
{
#define DEL(p) do { if (p && lv_obj_is_valid(p)) lv_obj_del(p); p = NULL; } while (0)
    DEL(s_happy_bar); DEL(s_hunger_bar);
    DEL(s_wpm_label); DEL(s_kpm_label); DEL(s_sub_label); DEL(s_layer_label);
    DEL(s_caps); DEL(s_bt_slot); DEL(s_icon_bt); DEL(s_icon_path); DEL(s_sep);
#undef DEL
}

const oled_screen_t screen_home = { build, update, destroy };
