/* Harness : rend chaque écran OLED réel dans le sim host → un PNG par écran. */
#include "lvgl.h"
#include "oled_screen.h"
#include "oled_kpm.h"
#include <stdio.h>

/* Simule ~60s de frappe pour peupler la sparkline / KPM (variation visible). */
static void sim_kpm(void)
{
    oled_kpm_reset();
    uint32_t t = 0;
    for (int s = 0; s < 60; s++) {
        int c = 3 + (s % 13);   /* ~3..15 frappes/s → KPM realiste */
        for (int k = 0; k < c; k++) oled_kpm_notify_keypress();
        t += 1000;
        oled_kpm_tick(t);
    }
}

extern void oled_sim_init(void);
extern void oled_sim_write_png(const char *path);

extern const oled_screen_t screen_splash;
extern const oled_screen_t screen_home;
extern const oled_screen_t screen_stats;
extern const oled_screen_t screen_tama;

static void render(const oled_screen_t *s, const char *path)
{
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);   /* pas de scrollbar */
    s->build(lv_scr_act());
    s->update();
    oled_sim_write_png(path);
    s->destroy();
}

int main(void)
{
    oled_sim_init();
    sim_kpm();
    render(&screen_splash, "sim/img_splash.png");
    render(&screen_home,   "sim/img_home.png");
    render(&screen_stats,  "sim/img_stats.png");
    render(&screen_tama,   "sim/img_tama.png");
    printf("done\n");
    return 0;
}
