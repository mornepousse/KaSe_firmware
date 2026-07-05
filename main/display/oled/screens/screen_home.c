/* screen_home.c — HOME : tableau de bord DENSE avec le pet tama.
 *
 * Layout 128×64 SSD1306 mono (lv_color_black() = pixel allumé), font_14 texte :
 *
 *   y=0..15  : [path 16][bt 16] slot ........................ CAP
 *   y=16     : séparateur
 *   COLONNE GAUCHE (x=2..90, le sprite occupe x=96..128) :
 *     y=18..32 : nom de couche (tronqué …)
 *     y=34..48 : "142kpm"
 *     y=50..55 : barre faim tama
 *     y=57..62 : barre bonheur tama
 *   SPRITE tama : x=96..128, y=20..52 (tama_render, si tama activé)
 *
 * Le pet est sur l'écran PRINCIPAL (demande utilisateur). Tout en font_14,
 * colonne gauche bornée à x<90 pour ne pas chevaucher le sprite.
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
#include "tama_engine.h"
#include "tama_render.h"
#if CONFIG_KASE_KBD_WIRELESS
#include "usb_presence.h"
#endif

#define SH_STATUS_H     16
#define SH_ICON_PATH_X   0
#define SH_ICON_BT_X    18
#define SH_BT_SLOT_X    36
#define SH_LAYER_X       2
#define SH_LAYER_Y      18
#define SH_LAYER_W      88   /* < sprite (x=96) → pas de chevauchement, tronqué … */
#define SH_KPM_Y        34
#define SH_BAR_X         2
#define SH_BAR_W        88   /* colonne gauche, s'arrête avant le sprite (x=96)   */
#define SH_BAR_H         5
#define SH_HUNGER_Y     50
#define SH_HAPPY_Y      57

static lv_obj_t *s_sep         = NULL;
static lv_obj_t *s_icon_path   = NULL;
static lv_obj_t *s_icon_bt     = NULL;
static lv_obj_t *s_bt_slot     = NULL;
static lv_obj_t *s_caps        = NULL;
static lv_obj_t *s_layer_label = NULL;
static lv_obj_t *s_kpm_label   = NULL;
static lv_obj_t *s_hunger_bar  = NULL;
static lv_obj_t *s_happy_bar   = NULL;
static bool      s_has_pet     = false;

static lv_obj_t *make_bar(lv_obj_t *parent, int y)
{
    lv_obj_t *b = lv_bar_create(parent);
    lv_obj_set_size(b, SH_BAR_W, SH_BAR_H);
    lv_obj_set_pos(b, SH_BAR_X, y);
    lv_bar_set_range(b, 0, TAMA2_STAT_MAX);
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
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

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

    s_kpm_label = make_lbl(parent, SH_LAYER_X, SH_KPM_Y);

    /* Pet sur l'écran principal (sprite à droite) + barres tama à gauche. */
    s_has_pet = tama_engine_is_enabled();
    if (s_has_pet) {
        tama_render_create(parent, BOARD_DISPLAY_WIDTH, BOARD_DISPLAY_HEIGHT);
        s_hunger_bar = make_bar(parent, SH_HUNGER_Y);
        s_happy_bar  = make_bar(parent, SH_HAPPY_Y);
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
    if (s_kpm_label)   lv_label_set_text_fmt(s_kpm_label, "%lukpm", (unsigned long)oled_kpm_value());

    if (s_has_pet) {
        const tama2_stats_t *st = tama_engine_get_stats();
        tama_render_update(tama_engine_get_state(), st, tama_engine_get_critter());
        if (st) {
            if (s_hunger_bar) lv_bar_set_value(s_hunger_bar, st->hunger, LV_ANIM_OFF);
            if (s_happy_bar)  lv_bar_set_value(s_happy_bar,  st->happiness, LV_ANIM_OFF);
        }
    }
}

static void destroy(void)
{
    if (s_has_pet) { tama_render_destroy(); s_has_pet = false; }
#define DEL(p) do { if (p && lv_obj_is_valid(p)) lv_obj_del(p); p = NULL; } while (0)
    DEL(s_happy_bar); DEL(s_hunger_bar);
    DEL(s_kpm_label); DEL(s_layer_label);
    DEL(s_caps); DEL(s_bt_slot); DEL(s_icon_bt); DEL(s_icon_path); DEL(s_sep);
#undef DEL
}

const oled_screen_t screen_home = { build, update, destroy };
