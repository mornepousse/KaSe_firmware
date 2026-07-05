/* Simulateur host LVGL du panneau OLED SSD1306 128×64 mono.
 * Rend un écran LVGL puis écrit un PNG (upscalé ×4, inversé pour imiter le
 * device : lv_color_black()=lit → blanc sur fond noir). But : VOIR les écrans
 * (positions, débordements, densité) sans flasher. */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

#define W 128
#define H 64
#define SCALE 4

static lv_color_t draw_buf[W * H];
static uint8_t    fb[W * H];   /* valeur LVGL capturée (0=noir=lit, 255=blanc) */

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++)
            fb[y * W + x] = (color_p++)->full;
    lv_disp_flush_ready(drv);
}

void oled_sim_write_png(const char *path)
{
    lv_refr_now(NULL);
    static uint8_t out[W * SCALE * H * SCALE * 3];
    for (int y = 0; y < H * SCALE; y++)
        for (int x = 0; x < W * SCALE; x++) {
            uint8_t v = 255 - fb[(y / SCALE) * W + (x / SCALE)];  /* invert = look OLED */
            int o = (y * W * SCALE + x) * 3;
            out[o] = out[o + 1] = out[o + 2] = v;
        }
    stbi_write_png(path, W * SCALE, H * SCALE, 3, out, W * SCALE * 3);
    printf("wrote %s (%dx%d)\n", path, W * SCALE, H * SCALE);
}

void oled_sim_init(void)
{
    lv_init();
    static lv_disp_draw_buf_t db;
    lv_disp_draw_buf_init(&db, draw_buf, NULL, W * H);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.draw_buf = &db;
    drv.flush_cb = flush_cb;
    drv.hor_res  = W;
    drv.ver_res  = H;
    lv_disp_drv_register(&drv);
    /* Fond blanc = pixel OFF sur le device (noir à l'affichage inversé). */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
}

#ifdef SIM_SELFTEST
int main(void)
{
    oled_sim_init();
    lv_obj_t *l = lv_label_create(lv_scr_act());
    lv_label_set_text(l, "SIM 128x64");
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
    oled_sim_write_png("sim/out.png");
    return 0;
}
#endif
