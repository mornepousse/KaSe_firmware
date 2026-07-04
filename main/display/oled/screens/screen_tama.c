/* screen_tama.c — Stub écran TAMA (Task 12 flesh out). */
#include "oled_screen.h"
#include "lvgl.h"

static void build(lv_obj_t *parent)   { (void)parent; }
static void update(void)              {}
static void destroy(void)             {}

const oled_screen_t screen_tama = { build, update, destroy };
