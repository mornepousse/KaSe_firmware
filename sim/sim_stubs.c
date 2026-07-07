/* Données + impls stub pour rendre les écrans OLED dans le sim host. */
#include "lvgl.h"
#include <string.h>
#include "keyboard_config.h"
#include "keymap.h"
#include "hid_bluetooth_manager.h"
#include "usb_hid.h"
#include "usb_presence.h"
#include "key_stats.h"
#include "esp_app_desc.h"

/* ── Données ─────────────────────────────────────────────────────── */
uint8_t current_layout = 5;  /* GAMING (nom long, test débordement) */
char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH] = {
    "BASE", "NAV", "NUM", "SYM", "FN", "GAMING", "MOUSE", "MEDIA", "SYS", "META"
};
uint8_t  hid_led_state   = HID_LED_CAPS_LOCK;   /* caps ON pour voir l'indicateur */
uint32_t key_stats_total = 1234567;

/* ── HID / BT / route ────────────────────────────────────────────── */
bool hid_bluetooth_is_initialized(void) { return true; }
bool hid_bluetooth_is_connected(void)   { return true; }
bool hid_bluetooth_is_pairing(void)     { return false; }
uint8_t bt_get_active_slot(void)        { return 1; }
uint8_t keyboard_get_usb_bl_state(void) { return 0; }   /* 0 = USB */
kbd_route_t kbd_active_route(void)      { return KBD_OUT_USB; }

/* ── Version firmware ────────────────────────────────────────────── */
static const esp_app_desc_t s_desc = { .version = "v4.0.0" };
const esp_app_desc_t *esp_app_get_description(void) { return &s_desc; }

/* (Tamagotchi retiré — plus de stubs tama.) */

/* Icônes : on linke les VRAIS assets (main/display/assets/img_*.c, 16×16
 * ALPHA_1BIT) — pas de placeholder, pour voir la vraie taille/position. */

/* oled_make_card (copié d'oled_screens.c, helper LVGL pur). */
lv_obj_t *oled_make_card(lv_obj_t *parent, int x, int y, int w, int h, int radius)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_opa(card, LV_OPA_0, 0);
    lv_obj_set_style_border_color(card, lv_color_black(), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}
