/* screen_home.c — Stub écran HOME (Task 9 flesh out). */
#include "oled_screen.h"
#include "lvgl.h"

static void build(lv_obj_t *parent)   { (void)parent; }
static void update(void)              {}
static void destroy(void)             {}

const oled_screen_t screen_home = { build, update, destroy };
