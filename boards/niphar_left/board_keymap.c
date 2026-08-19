/* Keymap par défaut — Niphargus moitié gauche.
 * 26 touches : rangées de 7/7/6/6. Les positions manquantes des deux dernières
 * rangées sont K_NO. La vraie keymap est provisionnée par USB (KS_CMD_*) ;
 * celle-ci n'est que le repli d'usine. */
#include "keymap.h"
#include "key_definitions.h"
#include "keyboard_config.h"

char default_layout_names[LAYERS][MAX_LAYOUT_NAME_LENGTH] = {
    "MAIN", "NAV", "LAYER 2", "LAYER 3", "LAYER 4",
    "LAYER 5", "LAYER 6", "LAYER 7", "LAYER 8", "LAYER 9",
};

#define _______ K_TRNS
#define XXXXXXX K_NO

uint16_t keymaps[LAYERS][MATRIX_ROWS][MATRIX_COLS] = {
    {   /* 0 — MAIN */
        {K_TAB,   K_Q, K_W, K_E, K_R, K_T, K_LBRC},
        {K_ESC,   K_A, K_S, K_D, K_F, K_G, K_RBRC},
        {K_LSHIFT,K_Z, K_X, K_C, K_V, K_B, XXXXXXX},
        {K_LCTRL, K_LWIN, K_LALT, MO_L1, K_SPACE, K_ENT, XXXXXXX},
    },
    {   /* 1 — NAV */
        {_______, K_1, K_2, K_3, K_4, K_5, XXXXXXX},
        {_______, K_F1, K_HOME, K_UP, K_END, XXXXXXX, XXXXXXX},
        {_______, XXXXXXX, K_LEFT, K_DOWN, K_RIGHT, XXXXXXX, XXXXXXX},
        {_______, _______, _______, _______, _______, XXXXXXX, XXXXXXX},
    },
    {{XXXXXXX}}, {{XXXXXXX}}, {{XXXXXXX}}, {{XXXXXXX}},
    {{XXXXXXX}}, {{XXXXXXX}}, {{XXXXXXX}}, {{XXXXXXX}},
};
