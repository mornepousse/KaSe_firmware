#pragma once
#include <stdbool.h>
#include <stdint.h>
#define TAMA2_STAT_MAX 1000
typedef enum { TAMA2_STATE_IDLE, TAMA2_STATE_COUNT } tama2_state_t;
typedef struct {
    uint16_t hunger, happiness, energy, health;
    uint32_t total_keys, session_keys, max_kpm;
    uint16_t level, xp;
} tama2_stats_t;
tama2_state_t tama_engine_get_state(void);
const tama2_stats_t *tama_engine_get_stats(void);
uint8_t tama_engine_get_critter(void);
bool tama_engine_is_enabled(void);
