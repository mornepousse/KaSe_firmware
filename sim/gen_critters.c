/* Générateur des 20 critters tama 32×32 REMPLIS (1-bit), paramétré.
 *   ./gen_critters          → rend une galerie PNG (revue)
 *   ./gen_critters header    → écrit un tama_sprites.h complet sur stdout
 * Mêmes noms/ordre que l'original (œuf→bébés→ados→adultes) pour ne pas casser
 * la sélection de critter par niveau dans tama_engine. */
#include "lvgl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern void oled_sim_init(void);
extern void oled_sim_write_png(const char *path);

static uint8_t spr[128];
static void px(int x, int y, int on) {
    if (x < 0 || x > 31 || y < 0 || y > 31) return;
    int b = y * 4 + x / 8, bit = 7 - (x % 8);
    if (on) spr[b] |= (1 << bit); else spr[b] &= ~(1 << bit);
}
static void ellipse(float cx, float cy, float rx, float ry, int on) {
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        float dx = (x - cx) / rx, dy = (y - cy) / ry;
        if (dx * dx + dy * dy <= 1.0f) px(x, y, on);
    }
}
static void disc(int cx, int cy, int r, int on) {
    for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++)
        if (x * x + y * y <= r * r) px(cx + x, cy + y, on);
}
static void tri_up(int cx, int by, int hw, int h, int on) {
    for (int i = 0; i < h; i++) { int w = hw * (h - i) / h; for (int x = -w; x <= w; x++) px(cx + x, by - i, on); }
}

/* egg=1 : coquille lisse sans visage. ears 0-4, eyes 0-3, mouth 0-3, feet 0-2, deco 0-3 */
typedef struct { const char *name; int egg, bw, bh, ears, eyes, mouth, feet, deco; } critter_t;

static const critter_t C[20] = {
 {"egg",     1, 11, 13, 0, 0, 0, 0, 0},
 {"goob",    0,  9, 10, 2, 0, 0, 1, 0},
 {"wibbur",  0,  9,  9, 3, 1, 1, 1, 0},
 {"xorby",   0, 10, 10, 1, 0, 2, 2, 0},
 {"snek",    0, 11, 10, 0, 0, 1, 0, 2},
 {"shansy",  0, 11, 11, 2, 1, 0, 1, 0},
 {"moops",   0, 12, 10, 4, 0, 2, 1, 0},
 {"hwooty",  0, 11, 12, 1, 1, 0, 2, 0},
 {"flip",    0, 12, 11, 2, 0, 1, 1, 1},
 {"lugerd",  0, 12, 12, 1, 3, 3, 2, 0},
 {"culu",    0, 13, 11, 2, 1, 0, 1, 0},
 {"shent",   0, 12, 12, 3, 0, 2, 1, 2},
 {"slorp",   0, 14, 10, 0, 2, 1, 1, 0},
 {"zeta",    0, 12, 13, 1, 1, 0, 2, 3},
 {"butters", 0, 13, 12, 2, 0, 0, 1, 1},
 {"tribbur", 0, 12, 12, 3, 1, 2, 2, 0},
 {"corine",  0, 13, 11, 4, 1, 0, 1, 0},
 {"pyre",    0, 12, 13, 1, 3, 3, 2, 3},
 {"rajur",   0, 13, 12, 1, 3, 0, 2, 2},
 {"crosh",   0, 14, 12, 2, 1, 1, 1, 3},
};

static void draw(const critter_t *c, int frame) {
    memset(spr, 0, sizeof(spr));
    float cx = 16, cy = 17;
    ellipse(cx, cy, c->bw, c->bh, 1);

    if (c->egg) {                       /* œuf : coquille + fêlure discrète, pas de visage */
        for (int y = 10; y < 24; y += 4) { int x = (y / 4) % 2 ? 13 : 17; px(x, y, 0); px(x + 1, y + 1, 0); px(x + 2, y, 0); }
        if (frame) { for (int y = 24; y < 28; y++) px(15 + (y % 2), y, 0); }  /* idle : petite variation */
        return;
    }

    int ty = cy - c->bh;
    switch (c->ears) {
    case 1: tri_up(cx - 6, ty + 3, 3, 5, 1); tri_up(cx + 6, ty + 3, 3, 5, 1); break;
    case 2: disc(cx - 7, ty + 2, 3, 1); disc(cx + 7, ty + 2, 3, 1); break;
    case 3: px(cx-6,ty-2,1);px(cx-6,ty-1,1);px(cx-6,ty,1);disc(cx-6,ty-3,1,1);
            px(cx+6,ty-2,1);px(cx+6,ty-1,1);px(cx+6,ty,1);disc(cx+6,ty-3,1,1); break;
    case 4: ellipse(cx-6, ty-2, 2, 6, 1); ellipse(cx+6, ty-2, 2, 6, 1); break;
    }

    int ex = 5, ey = cy - 3;
    if (frame == 1 && c->eyes != 2) { for (int x=-1;x<=1;x++){px(cx-ex+x,ey,0);px(cx+ex+x,ey,0);} }
    else switch (c->eyes) {
    case 0: disc(cx-ex,ey,2,0);disc(cx+ex,ey,2,0);px(cx-ex,ey,1);px(cx+ex,ey,1); break;
    case 1: disc(cx-ex,ey,3,0);disc(cx+ex,ey,3,0);disc(cx-ex,ey,1,1);disc(cx+ex,ey,1,1); break;
    case 2: for(int x=-2;x<=2;x++){px(cx-ex+x,ey,0);px(cx+ex+x,ey,0);} break;
    case 3: for(int i=0;i<3;i++){px(cx-ex-1+i,ey-1+i,0);px(cx+ex+1-i,ey-1+i,0);} disc(cx-ex,ey+1,1,0);disc(cx+ex,ey+1,1,0); break;
    }

    int my = cy + 4;
    switch (c->mouth) {
    case 0: for(int x=-3;x<=3;x++) px(cx+x, my + (abs(x)>1?-1:0), 0); px(cx,my+1,0); break;
    case 1: for(int x=-2;x<=2;x++) px(cx+x, my, 0); break;
    case 2: disc(cx, my, 2, 0); break;
    case 3: for(int x=-2;x<=2;x++) px(cx+x,my,0); px(cx-1,my+1,1);px(cx+1,my+1,1); break;
    }

    if (c->deco == 1) { ellipse(cx, cy+5, c->bw-5, c->bh-6, 0); }                       /* ventre creux */
    if (c->deco == 2) { disc(cx-6,cy+2,1,0); disc(cx+5,cy+4,1,0); disc(cx+2,cy+6,1,0); } /* spots */
    if (c->deco == 3) { tri_up(cx-4,cy-c->bh,1,3,1); tri_up(cx,cy-c->bh-1,1,3,1); tri_up(cx+4,cy-c->bh,1,3,1); } /* spikes */

    int fy = cy + c->bh - 1;
    switch (c->feet) {
    case 1: disc(cx-5, fy+1, 2, 1); disc(cx+5, fy+1, 2, 1); break;
    case 2: tri_up(cx-5, fy+3, 2, 3, 1); tri_up(cx+5, fy+3, 2, 3, 1); break;
    }
}

static void emit_array(const char *name, const char *suffix) {
    printf("static const uint8_t sprite_%s_%s[128] = {\n", name, suffix);
    for (int r = 0; r < 32; r++) {
        printf("    0x%02X, 0x%02X, 0x%02X, 0x%02X,\n", spr[r*4], spr[r*4+1], spr[r*4+2], spr[r*4+3]);
    }
    printf("};\n");
}

static void emit_header(void) {
    printf("/* Tamagotchi sprites — GENERATED by sim/gen_critters.c (filled style).\n");
    printf("   32x32 monochrome, 128 bytes each (MSB first, ALPHA_1BIT). */\n#pragma once\n#include <stdint.h>\n\n");
    printf("#define TAMA_SPRITE_W 32\n#define TAMA_SPRITE_H 32\n#define TAMA_SPRITE_BYTES 128\n#define TAMA_CRITTER_COUNT 20\n\n");
    for (int i = 0; i < 20; i++) { draw(&C[i], 0); emit_array(C[i].name, "main"); draw(&C[i], 1); emit_array(C[i].name, "idle"); }
    printf("\ntypedef struct { const uint8_t *main_frame; const uint8_t *idle_frame; const char *name; } tama_critter_t;\n\n");
    printf("static const tama_critter_t tama_critters[TAMA_CRITTER_COUNT] = {\n");
    for (int i = 0; i < 20; i++)
        printf("    { sprite_%s_main, sprite_%s_idle, \"%s\" },\n", C[i].name, C[i].name, C[i].name);
    printf("};\n");
}

static lv_img_dsc_t dsc[40];
static uint8_t buf[40][128];
static void page(int start, const char *path) {
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_white(), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 4; i++) {
        int c = start + i; if (c >= 20) break;
        for (int f = 0; f < 2; f++) {
            int idx = c * 2 + f; draw(&C[c], f); memcpy(buf[idx], spr, 128);
            dsc[idx].header.always_zero = 0; dsc[idx].header.cf = LV_IMG_CF_ALPHA_1BIT;
            dsc[idx].header.w = 32; dsc[idx].header.h = 32; dsc[idx].data_size = 128; dsc[idx].data = buf[idx];
            lv_obj_t *im = lv_img_create(lv_scr_act()); lv_img_set_src(im, &dsc[idx]); lv_obj_set_pos(im, i*32, f*32);
        }
    }
    oled_sim_write_png(path);
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "header") == 0) { emit_header(); return 0; }
    oled_sim_init();
    page(0,  "sim/critters_0.png"); page(4,  "sim/critters_1.png");
    page(8,  "sim/critters_2.png"); page(12, "sim/critters_3.png");
    page(16, "sim/critters_4.png");
    printf("done — 20 critters\n");
    return 0;
}
