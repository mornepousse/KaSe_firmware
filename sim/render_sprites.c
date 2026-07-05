/* Galerie des vrais sprites tama (tama_sprites.h) rendus 1-bit → PNG.
 * But : VOIR l'art des critters (32×32) tel qu'affiché sur l'OLED. */
#include "lvgl.h"
#include "tama_sprites.h"
#include <stdio.h>

extern void oled_sim_init(void);
extern void oled_sim_write_png(const char *path);

/* dscs persistants (lv_img garde le pointeur). */
static lv_img_dsc_t g_dsc[TAMA_CRITTER_COUNT * 2];

/* Rend 8 sprites (4 col × 2 lignes) de critters [start..start+4), main+idle. */
static void render_page(int start, const char *path)
{
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 4; i++) {
        int c = start + i;
        if (c >= TAMA_CRITTER_COUNT) break;
        const uint8_t *frames[2] = { tama_critters[c].main_frame, tama_critters[c].idle_frame };
        for (int f = 0; f < 2; f++) {
            int idx = (c * 2 + f);
            g_dsc[idx].header.always_zero = 0;
            g_dsc[idx].header.cf = LV_IMG_CF_ALPHA_1BIT;
            g_dsc[idx].header.w = TAMA_SPRITE_W;
            g_dsc[idx].header.h = TAMA_SPRITE_H;
            g_dsc[idx].data_size = TAMA_SPRITE_BYTES;
            g_dsc[idx].data = frames[f];
            lv_obj_t *img = lv_img_create(lv_scr_act());
            lv_img_set_src(img, &g_dsc[idx]);
            lv_obj_set_pos(img, i * 32, f * 32);   /* col = critter, ligne = main/idle */
        }
    }
    oled_sim_write_png(path);
}

int main(void)
{
    oled_sim_init();
    render_page(0,  "sim/sprites_0.png");   /* critters 0-3 */
    render_page(4,  "sim/sprites_1.png");   /* critters 4-7 */
    render_page(8,  "sim/sprites_2.png");   /* critters 8-11 */
    printf("done — %d critters\n", TAMA_CRITTER_COUNT);
    return 0;
}
