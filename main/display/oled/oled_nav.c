#include "oled_nav.h"

static oled_screen_id_t s_resting;
static uint32_t s_splash_until;
static uint32_t s_last_activity;
static bool     s_tama_enabled;

void oled_nav_init(uint32_t now_ms) {
    s_resting       = OLED_SCR_HOME;
    s_splash_until  = 0;   /* pas de splash au réveil — armé par OLED_EV_BOOT */
    s_last_activity = now_ms;
}

void oled_nav_set_tama_enabled(bool en) { s_tama_enabled = en; }

oled_screen_id_t oled_nav_active(uint32_t now_ms) {
    if (now_ms < s_splash_until) return OLED_SCR_SPLASH;
    if (s_tama_enabled && (now_ms - s_last_activity) >= OLED_NAV_IDLE_MS)
        return OLED_SCR_TAMA;
    return s_resting;
}

static oled_screen_id_t next_resting(oled_screen_id_t r) {
    switch (r) {
    case OLED_SCR_HOME:  return OLED_SCR_STATS;
    case OLED_SCR_STATS: return OLED_SCR_TAMA;
    default:             return OLED_SCR_HOME;   /* TAMA (ou autre) → HOME */
    }
}

void oled_nav_event(oled_nav_event_t ev, uint32_t now_ms) {
    switch (ev) {
    case OLED_EV_BOOT:          s_splash_until  = now_ms + OLED_NAV_SPLASH_MS; break;
    /* Changement de couche = activité : HOME affiche déjà la couche courante,
       donc pas de bascule d'écran — on remet juste le minuteur d'inactivité. */
    case OLED_EV_LAYER_CHANGED: s_last_activity = now_ms;                      break;
    case OLED_EV_ACTIVITY:      s_last_activity = now_ms;                      break;
    case OLED_EV_DISP_KEY:
        s_resting       = next_resting(s_resting);
        s_splash_until  = 0;   /* couper le splash */
        s_last_activity = now_ms;
        break;
    }
}
