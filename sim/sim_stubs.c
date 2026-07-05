/* Données + impls stub pour rendre les écrans OLED dans le sim host. */
#include "lvgl.h"
#include "keyboard_config.h"
#include "keymap.h"
#include "hid_bluetooth_manager.h"
#include "usb_hid.h"
#include "usb_presence.h"
#include "key_stats.h"
#include "esp_app_desc.h"
#include "tama_engine.h"
#include "tama_render.h"
#include "tama_sprites.h"   /* vrais sprites (main/tama) pour un rendu fidèle */

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

/* ── Tama : stats + renderer placeholder (boîte "pet") ───────────── */
static const tama2_stats_t s_stats = {
    .hunger = 620, .happiness = 800, .energy = 500, .health = 640,
    .total_keys = 1234567, .session_keys = 4200, .max_kpm = 380,
    .level = 3, .xp = 42,
};
tama2_state_t tama_engine_get_state(void)      { return TAMA2_STATE_IDLE; }
const tama2_stats_t *tama_engine_get_stats(void) { return &s_stats; }
uint8_t tama_engine_get_critter(void)          { return 0; }
bool tama_engine_is_enabled(void)              { return true; }

/* Rend le VRAI sprite (32×32 1-bit) à la vraie position OLED (96,20), comme
 * tama_render.c. Critter de démo = index 8 (assez évolué pour montrer oreilles
 * /yeux/pattes). */
static lv_obj_t   *s_pet = NULL;
static lv_img_dsc_t s_pet_dsc;
void tama_render_create(lv_obj_t *parent, uint16_t w, uint16_t h)
{
    (void)w; (void)h;
    s_pet_dsc.header.always_zero = 0;
    s_pet_dsc.header.cf = LV_IMG_CF_ALPHA_1BIT;
    s_pet_dsc.header.w = TAMA_SPRITE_W;
    s_pet_dsc.header.h = TAMA_SPRITE_H;
    s_pet_dsc.data_size = TAMA_SPRITE_BYTES;
    s_pet_dsc.data = tama_critters[8].main_frame;
    s_pet = lv_img_create(parent);
    lv_img_set_src(s_pet, &s_pet_dsc);
    lv_obj_set_pos(s_pet, 96, 20);
}
void tama_render_update(tama2_state_t s, const tama2_stats_t *st, uint8_t c) { (void)s;(void)st;(void)c; }
void tama_render_destroy(void) { if (s_pet) { lv_obj_del(s_pet); s_pet = NULL; } }

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
