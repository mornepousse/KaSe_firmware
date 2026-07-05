#pragma once
#include "lvgl.h"
#include "tama_engine.h"
void tama_render_create(lv_obj_t *parent, uint16_t w, uint16_t h);
void tama_render_update(tama2_state_t s, const tama2_stats_t *st, uint8_t critter);
void tama_render_destroy(void);
