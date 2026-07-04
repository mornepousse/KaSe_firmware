/* screen_stats.c — Stub écran STATS (Task 11 flesh out). */
#include "oled_screen.h"
#include "lvgl.h"

static void build(lv_obj_t *parent)   { (void)parent; }
static void update(void)              {}
static void destroy(void)             {}

const oled_screen_t screen_stats = { build, update, destroy };
