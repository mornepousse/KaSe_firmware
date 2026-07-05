/* screen_home.c — HOME : gros pet centré, infos en barre du haut.
 *
 * Layout 128×64 SSD1306 mono (lv_color_black() = pixel allumé), font_14 :
 *
 *   y=0..15  : [icône connexion 16] NOM_COUCHE(tronqué) .. slot ...... CAP
 *   y=15     : séparateur
 *   y=16..62 : PET tama en grand (46×46 upscalé, centré) — tama_render
 *
 * Plus de KPM ni de barres de progression (retirés à la demande) : les stats
 * de frappe vivent sur l'écran STATS. Le pet est la vedette de l'écran principal.
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
#include "tama_engine.h"
#include "tama_render.h"
#if CONFIG_KASE_KBD_WIRELESS
#include "usb_presence.h"
#endif

#define SH_STATUS_H     15
#define SH_ICON_PATH_X   0
#define SH_LAYER_X      18
#define SH_LAYER_W      60   /* tronqué (…) — laisse la place au slot + CAP       */
#define SH_BT_SLOT_X    80

static lv_obj_t *s_sep         = NULL;
static lv_obj_t *s_icon_path   = NULL;
static lv_obj_t *s_bt_slot     = NULL;
static lv_obj_t *s_caps        = NULL;
static lv_obj_t *s_layer_label = NULL;
static bool      s_has_pet     = false;

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

    s_layer_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_layer_label, UI_FONT, 0);
    lv_label_set_long_mode(s_layer_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_layer_label, SH_LAYER_W);
    lv_obj_set_pos(s_layer_label, SH_LAYER_X, 0);

    s_bt_slot = make_lbl(parent, SH_BT_SLOT_X, 0);

    s_caps = make_lbl(parent, 0, 0);
    lv_label_set_text(s_caps, "CAP");
    lv_obj_align(s_caps, LV_ALIGN_TOP_RIGHT, -1, 0);   /* collé à droite → jamais de débordement */
    lv_obj_add_flag(s_caps, LV_OBJ_FLAG_HIDDEN);

    s_has_pet = tama_engine_is_enabled();
    if (s_has_pet)
        tama_render_create(parent, BOARD_DISPLAY_WIDTH, BOARD_DISPLAY_HEIGHT);
}

static void update(void)
{
#if CONFIG_KASE_KBD_WIRELESS
    if (s_icon_path) lv_img_set_src(s_icon_path, (kbd_active_route() == KBD_OUT_RF) ? &wifi : &flash);
#else
    if (s_icon_path) lv_img_set_src(s_icon_path, (keyboard_get_usb_bl_state() != 0) ? &bluetooth_16px : &flash);
#endif

    bool bt_init = hid_bluetooth_is_initialized();
    bool bt_pair = hid_bluetooth_is_pairing();
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

    if (s_has_pet)
        tama_render_update(tama_engine_get_state(), tama_engine_get_stats(), tama_engine_get_critter());
}

static void destroy(void)
{
    if (s_has_pet) { tama_render_destroy(); s_has_pet = false; }
#define DEL(p) do { if (p && lv_obj_is_valid(p)) lv_obj_del(p); p = NULL; } while (0)
    DEL(s_caps); DEL(s_bt_slot); DEL(s_layer_label); DEL(s_icon_path); DEL(s_sep);
#undef DEL
}

const oled_screen_t screen_home = { build, update, destroy };
