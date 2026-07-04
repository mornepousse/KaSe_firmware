/* screen_home.c — Écran HOME : icône chemin, statut BT, caps lock, layer, KPM.
 *
 * Layout 128×64 SSD1306 mono (lv_color_black() = pixel allumé) :
 *
 *   y=0..15  : barre de statut (card bordée arrondie)
 *                 ⚡  ᯡ  1                    CAP
 *              [icon_path][icon_bt][bt_slot]  [caps]
 *
 *   y=16..57 : carte layer (bordure, pleine largeur)
 *                 nom de la couche courante centré
 *
 *   y=59..62 : barre KPM (4 px, pleine largeur)
 *
 * NOTE : l'indicateur souris "M" est OMIS ici — son horodatage
 * (last_mouse_activity) vit dans l'ancien backend, pas accessible depuis un
 * module écran.  Il sera câblé en Task 13 (réécriture backend).
 *
 * Contrat build/update/destroy :
 *   build(parent)  — crée les objets ; parent = lv_scr_act(), lock déjà tenu.
 *   update()       — rafraîchit les valeurs dynamiques, pas de création.
 *   destroy()      — lv_obj_del de chaque objet non-NULL, puis met à NULL.
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
#if CONFIG_KASE_KBD_WIRELESS
#include "usb_presence.h"
#endif
#include <string.h>

LV_FONT_DECLARE(lv_font_montserrat_28);

/* ── Constantes de layout ─────────────────────────────────────────────── */

#define SH_STATUS_H      16
#define SH_STATUS_RADIUS  4
#define SH_ICON_PATH_X    4    /* icône chemin (flash/wifi/bt) */
#define SH_ICON_BT_X     22    /* icône BT connecté */
#define SH_BT_SLOT_X     42    /* label slot BT ("1"/"2"/"3"/"P") */
#define SH_CAPS_X        90    /* "CAP" top-right de la barre statut */
#define SH_STATUS_TEXT_Y  1    /* décalage vertical du texte dans la barre */
#define SH_LAYER_Y       16    /* début de la carte layer (après la barre) */
#define SH_LAYER_H       42    /* hauteur de la carte layer */
#define SH_LAYER_NAME_LEN 3    /* seuil court/long pour le choix de fonte */
#define SH_KPM_Y         59    /* barre KPM en bas */
#define SH_KPM_H          4    /* hauteur de la barre KPM (px) */
#define SH_KPM_MAX       400   /* plafond KPM */

/* ── Pointeurs statiques vers les objets LVGL ────────────────────────── */

static lv_obj_t *s_status_card  = NULL;
static lv_obj_t *s_icon_path    = NULL;
static lv_obj_t *s_icon_bt      = NULL;
static lv_obj_t *s_bt_slot      = NULL;
static lv_obj_t *s_caps         = NULL;
static lv_obj_t *s_layer_card   = NULL;
static lv_obj_t *s_layer_label  = NULL;
static lv_obj_t *s_kpm_bar      = NULL;

/* ── build ────────────────────────────────────────────────────────────── */

static void build(lv_obj_t *parent)
{
    /* Barre de statut — card arrondie, fond transparent, bordure allumée */
    s_status_card = oled_make_card(parent, 0, 0,
                                   BOARD_DISPLAY_WIDTH, SH_STATUS_H,
                                   SH_STATUS_RADIUS);

    /* Icône chemin de connexion (flash=USB, wifi=RF, bluetooth_16px=BLE) */
    s_icon_path = lv_img_create(parent);
    lv_obj_set_pos(s_icon_path, SH_ICON_PATH_X, 0);

    /* Icône BT (statique : visible quand BT initialisé + connecté) */
    s_icon_bt = lv_img_create(parent);
    lv_obj_set_pos(s_icon_bt, SH_ICON_BT_X, 0);
    lv_obj_add_flag(s_icon_bt, LV_OBJ_FLAG_HIDDEN);

    /* Label slot BT : "1"–"3" ou "P" en mode pairing */
    s_bt_slot = lv_label_create(parent);
    lv_obj_set_style_text_font(s_bt_slot, UI_FONT, 0);
    lv_label_set_text(s_bt_slot, "");
    lv_obj_set_pos(s_bt_slot, SH_BT_SLOT_X, SH_STATUS_TEXT_Y);

    /* Indicateur caps lock (top-right de la barre de statut) */
    s_caps = lv_label_create(parent);
    lv_obj_set_style_text_font(s_caps, UI_FONT, 0);
    lv_label_set_text(s_caps, "CAP");
    lv_obj_set_pos(s_caps, SH_CAPS_X, SH_STATUS_TEXT_Y);
    lv_obj_add_flag(s_caps, LV_OBJ_FLAG_HIDDEN);

    /* Carte layer (bordure, pleine largeur) */
    s_layer_card = oled_make_card(parent, 0, SH_LAYER_Y,
                                  BOARD_DISPLAY_WIDTH, SH_LAYER_H, 2);

    /* Label nom de couche, centré dans la carte */
    s_layer_label = lv_label_create(s_layer_card);
    lv_label_set_long_mode(s_layer_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_layer_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_layer_label, BOARD_DISPLAY_WIDTH - 4);

    /* Barre KPM (4 px, fond transparent, fill allumé) */
    s_kpm_bar = lv_bar_create(parent);
    lv_obj_set_size(s_kpm_bar, BOARD_DISPLAY_WIDTH, SH_KPM_H);
    lv_obj_set_pos(s_kpm_bar, 0, SH_KPM_Y);
    lv_bar_set_range(s_kpm_bar, 0, SH_KPM_MAX);
    lv_bar_set_value(s_kpm_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_opa(s_kpm_bar,    LV_OPA_0,     LV_PART_MAIN);
    lv_obj_set_style_border_width(s_kpm_bar, 0,          LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_kpm_bar,   0,             LV_PART_MAIN);
    lv_obj_set_style_radius(s_kpm_bar,    0,             LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_kpm_bar,  lv_color_black(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_kpm_bar,    LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_kpm_bar,    0,             LV_PART_INDICATOR);
}

/* ── update ───────────────────────────────────────────────────────────── */

static void update(void)
{
    /* 1. Icône chemin ──────────────────────────────────────────────────── */
#if CONFIG_KASE_KBD_WIRELESS
    int path_state = (kbd_active_route() == KBD_OUT_RF) ? 1 : 0;
#else
    int path_state = (keyboard_get_usb_bl_state() == 0) ? 0 : 1;
#endif

    if (s_icon_path) {
#if CONFIG_KASE_KBD_WIRELESS
        lv_img_set_src(s_icon_path, (path_state == 0) ? &flash : &wifi);
#else
        lv_img_set_src(s_icon_path, (path_state == 0) ? &flash : &bluetooth_16px);
#endif
    }

    /* 2. Icône BT + slot ───────────────────────────────────────────────── */
    /* Indicateur statique : icône wifi quand BT initialisé + connecté.
     * La machine à état clignotant de l'ancien backend (timers statiques
     * réinitialisés à chaque build) est délibérément omise — fragile. */
    bool bt_init    = hid_bluetooth_is_initialized();
    bool bt_conn    = hid_bluetooth_is_connected();
    bool bt_pairing = hid_bluetooth_is_pairing();

    if (s_icon_bt) {
        if (bt_init && bt_conn) {
            lv_obj_clear_flag(s_icon_bt, LV_OBJ_FLAG_HIDDEN);
            lv_img_set_src(s_icon_bt, &wifi);
        } else {
            lv_obj_add_flag(s_icon_bt, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_bt_slot) {
        if (bt_init && bt_pairing)
            lv_label_set_text(s_bt_slot, "P");
        else if (bt_init)
            lv_label_set_text_fmt(s_bt_slot, "%d", (int)(bt_get_active_slot() + 1));
        else
            lv_label_set_text(s_bt_slot, "");
    }

    /* 3. Caps lock ─────────────────────────────────────────────────────── */
    if (s_caps) {
        if (hid_led_state & HID_LED_CAPS_LOCK)
            lv_obj_clear_flag(s_caps, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_caps, LV_OBJ_FLAG_HIDDEN);
    }

    /* 4. Nom de couche ─────────────────────────────────────────────────── */
    if (s_layer_label) {
        const char *name = default_layout_names[current_layout];
        lv_label_set_text(s_layer_label, name);
        /* Fonte : grande (28 px) pour noms courts ≤ 3 chars, petite sinon */
        if (strlen(name) <= SH_LAYER_NAME_LEN)
            lv_obj_set_style_text_font(s_layer_label, &lv_font_montserrat_28, 0);
        else
            lv_obj_set_style_text_font(s_layer_label, UI_FONT, 0);
        lv_obj_align(s_layer_label, LV_ALIGN_CENTER, 0, 0);
    }

    /* 5. Barre KPM ─────────────────────────────────────────────────────── */
    /* NOTE : l'indicateur souris "M" est omis ici (voir en-tête du fichier). */
    if (s_kpm_bar) {
        uint32_t v = oled_kpm_value();
        if (v > SH_KPM_MAX) v = SH_KPM_MAX;
        lv_bar_set_value(s_kpm_bar, (int32_t)v, LV_ANIM_OFF);
    }
}

/* ── destroy ──────────────────────────────────────────────────────────── */

static void destroy(void)
{
    if (s_kpm_bar     && lv_obj_is_valid(s_kpm_bar))     lv_obj_del(s_kpm_bar);
    s_kpm_bar = NULL;
    /* s_layer_label est enfant de s_layer_card → supprimé par son parent */
    if (s_layer_card  && lv_obj_is_valid(s_layer_card))  lv_obj_del(s_layer_card);
    s_layer_card  = NULL;
    s_layer_label = NULL;
    if (s_caps        && lv_obj_is_valid(s_caps))        lv_obj_del(s_caps);
    s_caps = NULL;
    if (s_bt_slot     && lv_obj_is_valid(s_bt_slot))     lv_obj_del(s_bt_slot);
    s_bt_slot = NULL;
    if (s_icon_bt     && lv_obj_is_valid(s_icon_bt))     lv_obj_del(s_icon_bt);
    s_icon_bt = NULL;
    if (s_icon_path   && lv_obj_is_valid(s_icon_path))   lv_obj_del(s_icon_path);
    s_icon_path = NULL;
    if (s_status_card && lv_obj_is_valid(s_status_card)) lv_obj_del(s_status_card);
    s_status_card = NULL;
}

const oled_screen_t screen_home = { build, update, destroy };
