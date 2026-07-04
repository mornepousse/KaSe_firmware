/* screen_layer.c — Stub écran LAYER flash (Task 10 flesh out). */
#include "oled_screen.h"
#include "lvgl.h"

static void build(lv_obj_t *parent)   { (void)parent; }
static void update(void)              {}
static void destroy(void)             {}

const oled_screen_t screen_layer = { build, update, destroy };
