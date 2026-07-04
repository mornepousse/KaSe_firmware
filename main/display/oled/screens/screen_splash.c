/* screen_splash.c — Stub écran SPLASH (Task 8 flesh out). */
#include "oled_screen.h"
#include "lvgl.h"

static void build(lv_obj_t *parent)   { (void)parent; }
static void update(void)              {}
static void destroy(void)             {}

const oled_screen_t screen_splash = { build, update, destroy };
