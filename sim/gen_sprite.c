/* Générateur d'essai : dessine des créatures 32×32 REMPLIES (1-bit) par code
 * géométrique (corps plein + yeux/bouche évidés + pattes) et les rend en PNG,
 * pour juger une alternative à l'art contour. */
#include "lvgl.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern void oled_sim_init(void);
extern void oled_sim_write_png(const char *path);

#define SW 32
#define SH 32
static uint8_t spr[128];

static void px(int x, int y, int on) {
    if (x < 0 || x > 31 || y < 0 || y > 31) return;
    int byte = y * 4 + x / 8, bit = 7 - (x % 8);
    if (on) spr[byte] |= (1 << bit); else spr[byte] &= ~(1 << bit);
}
static void fill_ellipse(float cx, float cy, float rx, float ry, int on) {
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        float dx = (x - cx) / rx, dy = (y - cy) / ry;
        if (dx * dx + dy * dy <= 1.0f) px(x, y, on);
    }
}
static void disc(int cx, int cy, int r, int on) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++)
        if (x * x + y * y <= r * r) px(cx + x, cy + y, on);
}

/* Un pet « blob » : corps plein, 2 yeux (avec pupille lit), sourire, 2 pattes. */
static void make_blob(int happy) {
    memset(spr, 0, sizeof(spr));
    fill_ellipse(16, 17, 12, 11, 1);          /* corps plein */
    /* yeux : blancs évidés + pupille lit au centre */
    disc(11, 14, 3, 0); disc(21, 14, 3, 0);
    disc(11, 14, 1, 1); disc(21, 14, 1, 1);
    /* bouche : sourire (arc évidé) ou trait */
    if (happy) { for (int x = 12; x <= 20; x++) px(x, 22 + (abs(x - 16) > 2 ? -1 : 0), 0); disc(16,22,1,0);}
    else       for (int x = 13; x <= 19; x++) px(x, 22, 0);
    /* pattes */
    disc(11, 28, 2, 1); disc(21, 28, 2, 1);
    /* petites antennes/oreilles */
    px(9,6,1);px(9,7,1);px(10,7,1);px(10,8,1);
    px(23,6,1);px(23,7,1);px(22,7,1);px(22,8,1);
}

static lv_img_dsc_t dsc;
static void show(const char *path) {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    dsc.header.always_zero = 0; dsc.header.cf = LV_IMG_CF_ALPHA_1BIT;
    dsc.header.w = SW; dsc.header.h = SH; dsc.data_size = 128; dsc.data = spr;
    /* rendu centré ET à la vraie position OLED (96,20) côte à côte */
    lv_obj_t *a = lv_img_create(lv_scr_act()); lv_img_set_src(a, &dsc); lv_obj_set_pos(a, 8, 16);
    lv_obj_t *b = lv_img_create(lv_scr_act()); lv_img_set_src(b, &dsc); lv_obj_set_pos(b, 96, 20);
    oled_sim_write_png(path);
}

int main(void) {
    oled_sim_init();
    make_blob(1); show("sim/gen_blob_happy.png");
    make_blob(0); show("sim/gen_blob_neutral.png");
    printf("done\n");
    return 0;
}
