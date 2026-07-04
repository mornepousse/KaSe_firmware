#include "oled_nav.h"

static oled_screen_id_t s_resting;
static uint32_t s_splash_until;
static uint32_t s_flash_until;
static uint32_t s_last_activity;
static bool     s_tama_enabled;

void oled_nav_init(uint32_t now_ms) {
    s_resting       = OLED_SCR_HOME;
    s_splash_until  = now_ms + OLED_NAV_SPLASH_MS;
    s_flash_until   = 0;
    s_last_activity = now_ms;
}

void oled_nav_set_tama_enabled(bool en) { s_tama_enabled = en; }

oled_screen_id_t oled_nav_active(uint32_t now_ms) {
    if (now_ms < s_splash_until) return OLED_SCR_SPLASH;
    if (now_ms < s_flash_until)  return OLED_SCR_LAYER;
    if (s_tama_enabled && (now_ms - s_last_activity) >= OLED_NAV_IDLE_MS)
        return OLED_SCR_TAMA;
    return s_resting;
}

void oled_nav_event(oled_nav_event_t ev, uint32_t now_ms) {
    switch (ev) {
    case OLED_EV_BOOT:          s_splash_until = now_ms + OLED_NAV_SPLASH_MS; break;
    case OLED_EV_LAYER_CHANGED: s_flash_until  = now_ms + OLED_NAV_FLASH_MS;  break;
    case OLED_EV_ACTIVITY:      s_last_activity = now_ms;                     break;
    case OLED_EV_DISP_KEY:      /* filled in Task 3 */                        break;
    }
}
