/* OLED I2C (SSD1306) backend implementation — thin driver.
 *
 * Toute l'UI (status card, tama, layer label, barre KPM, indicateurs) vit
 * désormais dans le manager multi-écrans : oled_nav (machine à états),
 * oled_screens (registre + build/destroy/update), oled_kpm (fenêtre KPM) et
 * les modules screens/screen_*.c. Ce backend ne fait plus que :
 *   - configurer le hardware (oled_init) ;
 *   - piloter le manager sous le lock LVGL (refresh/tick/layer) ;
 *   - forwarder les événements (keypress/mouse/disp_key/activity) ;
 *   - gérer sleep/wake (power panel) et l'écran DFU.
 */
#include "display_backend.h"
#include "status_display.h"
#include "board.h"
#include "i2c_oled_display.h"
#include "oled_nav.h"
#include "oled_screens.h"
#include "oled_kpm.h"
#include "tama_engine.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

LV_FONT_DECLARE(lv_font_montserrat_28);

/* Horloge millisecondes dérivée du tick FreeRTOS (base commune au manager). */
static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

/* true une fois qu'un reset du manager a construit la nav ; garde oled_update()
 * de tourner avant le premier refresh_all. */
static bool oled_initialized = false;

/* ── Backend interface ───────────────────────────────────────────── */

static bool oled_init(void)
{
    display_hw_config_t cfg = {
        .bus_type       = BOARD_DISPLAY_BUS,
        .width          = BOARD_DISPLAY_WIDTH,
        .height         = BOARD_DISPLAY_HEIGHT,
        .pixel_clock_hz = BOARD_DISPLAY_CLK_HZ,
        .reset_pin      = BOARD_DISPLAY_RESET,
        .i2c = {
            .host                   = BOARD_DISPLAY_I2C_HOST,
            .sda                    = BOARD_DISPLAY_I2C_SDA,
            .scl                    = BOARD_DISPLAY_I2C_SCL,
            .address                = BOARD_DISPLAY_I2C_ADDR,
            .enable_internal_pullups = BOARD_DISPLAY_I2C_PULLUPS,
        },
    };
    display_set_hw_config(&cfg);
    init_display();
    return display_available;
}

/* Full (re)build : table rase + reset du manager. oled_screens_reset() appelle
 * oled_nav_init() (qui arme le splash à now+SPLASH_MS → splash au boot), remet
 * le KPM à zéro et re-synchronise tama_enabled. Lock LVGL tenu (le manager y
 * touche des objets LVGL via destroy()). */
static void oled_refresh_all(void)
{
    if (!display_available) return;
    if (!lvgl_port_lock(200)) return;
    display_clear_screen();
    oled_screens_reset(now_ms());
    oled_initialized = true;
    lvgl_port_unlock();
}

/* Tick périodique : le manager avance le KPM, résout l'écran actif, build/clean/
 * destroy si besoin puis update(). oled_screens_tick() exige le lock tenu. */
static void oled_update(void)
{
    if (!display_available || !oled_initialized) return;
    if (!lvgl_port_lock(50)) return;
    oled_screens_tick(now_ms());
    lvgl_port_unlock();
}

/* Changement de couche : arme le flash LAYER puis force un tick pour que
 * l'écran LAYER apparaisse immédiatement. */
static void oled_update_layer(void)
{
    if (!display_available) return;
    if (!oled_initialized) { oled_refresh_all(); return; }
    oled_screens_layer_changed(now_ms());
    oled_update();   /* rebâtit tout de suite l'écran actif (LAYER) */
}

static void oled_sleep(void)
{
    if (!lvgl_port_lock(100)) return;
    /* Détruit l'écran courant via le manager (reset → destroy + nav_init) et
     * blanchit le buffer LVGL. */
    oled_screens_reset(now_ms());
    display_clear_screen();
    oled_initialized = false;
    /* Power le panneau OFF (SSD1306 display-off) tant qu'on tient le lock LVGL
     * (pas de flush I2C concurrent). Le clear ne blanchit que le buffer LVGL ;
     * en light-sleep la tâche LVGL est gelée et ne flush jamais, donc sans ceci
     * la dernière frame resterait figée à l'écran. */
    i2c_oled_display_power(false);
    lvgl_port_unlock();
}

static void oled_wake(void)
{
    i2c_oled_display_power(true);   /* panneau rallumé */
    request_wake_request = true;    /* déclenche un refresh_all → reset + splash */
}

static void oled_notify_mouse(void)
{
    /* L'ancien indicateur souris "M" du HOME a été retiré dans la refonte ;
     * on ne forwarde plus que l'activité (réveil idle-tama). */
    oled_screens_activity(now_ms());
}

static void oled_notify_keypress(void)
{
    oled_kpm_notify_keypress();
    if (tama_engine_is_enabled())
        tama_engine_keypress(oled_kpm_value());
    oled_screens_activity(now_ms());
}

static void oled_notify_display_key(void)
{
    oled_screens_disp_key(now_ms());
}

static void oled_show_dfu(void)
{
    /* display_clear_screen() fait un lv_obj_clean (écriture LVGL) → doit être
     * SOUS le lock, comme les autres chemins du backend (cohérence lock). */
    if (lvgl_port_lock(0)) {
        display_clear_screen();
        lv_obj_t *label = lv_label_create(lv_scr_act());
        lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
        lv_label_set_text(label, "DFU");
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        lvgl_port_unlock();
    }
}

const display_backend_t oled_display_backend = {
    .init               = oled_init,
    .update_layer       = oled_update_layer,
    .update             = oled_update,
    .refresh_all        = oled_refresh_all,
    .sleep              = oled_sleep,
    .wake               = oled_wake,
    .notify_mouse       = oled_notify_mouse,
    .notify_keypress    = oled_notify_keypress,
    .notify_display_key = oled_notify_display_key,
    .show_dfu           = oled_show_dfu,
};
