/* Maquettes HOME (sans KPM ni barres), pet 32×32 NATIF (pas de zoom → net).
 * Le nom/niveau du tama est montré en TEXTE à la place des barres. */
#include "lvgl.h"
#include "tama_sprites.h"
#include <stdio.h>
#include <string.h>

extern void oled_sim_init(void);
extern void oled_sim_write_png(const char *path);

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_28);

static lv_img_dsc_t pet_dsc;
static const uint8_t *PET = 0;

static void reset(void) {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
}
static void pet(int x, int y) {
    pet_dsc.header.always_zero = 0; pet_dsc.header.cf = LV_IMG_CF_ALPHA_1BIT;
    pet_dsc.header.w = 32; pet_dsc.header.h = 32; pet_dsc.data_size = 128; pet_dsc.data = PET;
    lv_obj_t *im = lv_img_create(lv_scr_act()); lv_img_set_src(im, &pet_dsc); lv_obj_set_pos(im, x, y);
}

/* Upscale nearest-neighbor propre : 32×32 1-bit → N×N 1-bit (pixel = bloc). */
static uint8_t big_buf[64 * 64 / 8];
static lv_img_dsc_t big_dsc;
static int src_bit(const uint8_t *s, int x, int y) { return (s[y*4 + x/8] >> (7 - (x%8))) & 1; }
static void set_bit(uint8_t *d, int stride, int x, int y, int v) { int i=y*stride+x/8, b=7-(x%8); if(v) d[i]|=(1<<b); else d[i]&=~(1<<b); }
static void pet_big(int x, int y, int N) {
    int stride = (N + 7) / 8;
    memset(big_buf, 0, sizeof(big_buf));
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++)
        set_bit(big_buf, stride, i, j, src_bit(PET, i*32/N, j*32/N));
    big_dsc.header.always_zero = 0; big_dsc.header.cf = LV_IMG_CF_ALPHA_1BIT;
    big_dsc.header.w = N; big_dsc.header.h = N; big_dsc.data_size = stride*N; big_dsc.data = big_buf;
    lv_obj_t *im = lv_img_create(lv_scr_act()); lv_img_set_src(im, &big_dsc); lv_obj_set_pos(im, x, y);
}
static lv_obj_t *lbl(const char *t, const lv_font_t *f, int x, int y) {
    lv_obj_t *l = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(l, f, 0); lv_label_set_text(l, t); lv_obj_set_pos(l, x, y);
    return l;
}
static lv_obj_t *lblc(const char *t, const lv_font_t *f, int y, int w) { /* centré sur largeur w */
    lv_obj_t *l = lbl(t, f, 0, y); lv_obj_set_width(l, w); lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    return l;
}
static void status(void) { lbl("USB 2", &lv_font_montserrat_14, 2, 0); lbl("CAP", &lv_font_montserrat_14, 100, 0); }
static void sep(int y) {
    lv_obj_t *s = lv_obj_create(lv_scr_act()); lv_obj_set_size(s, 128, 1); lv_obj_set_pos(s, 0, y);
    lv_obj_set_style_bg_color(s, lv_color_black(), 0); lv_obj_set_style_border_width(s, 0, 0); lv_obj_set_style_radius(s, 0, 0);
}
#define F14 &lv_font_montserrat_14
#define F28 &lv_font_montserrat_28

/* A — Épuré : statut + couche + pet coin bas-droite. */
static void mock_A(void) { reset(); status(); sep(16); lbl("GAMING", F14, 2, 22); pet(96, 30); oled_sim_write_png("sim/mock_A.png"); }

/* B — Pet + identité : pet centré-haut, couche + "Lv3 goob" (tama) en dessous. */
static void mock_B(void) {
    reset(); status(); pet(48, 14);
    lblc("GAMING", F14, 46, 128);
    oled_sim_write_png("sim/mock_B.png");
}

/* C — Équilibré : colonne texte à gauche (couche + niveau tama), pet à droite. */
static void mock_C(void) {
    reset(); status(); sep(16);
    lbl("GAMING", F14, 2, 19); lbl("L5", F14, 2, 33);
    lbl("Lv3 goob", F14, 2, 47);   /* tama en texte, pas de barres */
    pet(96, 20);
    oled_sim_write_png("sim/mock_C.png");
}

/* D — Couche géante empilée : "GAMING" font_28 en haut, pet en bas-centre (sans statut). */
static void mock_D(void) {
    reset();
    lblc("GAMING", F28, 4, 128);
    pet(48, 32);
    oled_sim_write_png("sim/mock_D.png");
}

/* B-grand : couche dans la barre du haut, GROS pet centré en dessous. */
static void mock_Bbig(int N, const char *path) {
    reset();
    lbl("GAMING", F14, 2, 0);
    lbl("2", F14, 92, 0); lbl("CAP", F14, 104, 0);
    sep(15);
    pet_big((128 - N) / 2, 16, N);
    oled_sim_write_png(path);
}

int main(void) {
    oled_sim_init();
    PET = tama_critters[8].main_frame;
    mock_A(); mock_B(); mock_C(); mock_D();
    mock_Bbig(44, "sim/mock_Bbig44.png");
    mock_Bbig(48, "sim/mock_Bbig48.png");
    printf("done\n"); return 0;
}
