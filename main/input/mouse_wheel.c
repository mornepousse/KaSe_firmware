/* Voir mouse_wheel.h. Aucune dépendance ESP-IDF : compilé tel quel sur l'hôte. */
#include "mouse_wheel.h"

/* Table indexée par (précédent << 2) | courant.
 *
 *   ligne prev=00 : 00->00 = 0, 00->01 = -1, 00->10 = +1, 00->11 = impossible
 *   ligne prev=01 : 01->00 = +1, 01->01 = 0, 01->10 = impossible, 01->11 = -1
 *   ligne prev=10 : 10->00 = -1, 10->01 = impossible, 10->10 = 0, 10->11 = +1
 *   ligne prev=11 : 11->00 = impossible, 11->01 = +1, 11->10 = -1, 11->11 = 0
 *
 * La diagonale est nulle (pas de changement) et la table est antisymétrique :
 * un cycle complet dans un sens puis dans l'autre ramène le compteur à zéro,
 * ce que test_quadrature_round_trip_is_neutral vérifie. */
static const int8_t QUAD[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

int8_t mouse_wheel_step(uint8_t prev_ab, uint8_t cur_ab)
{
    return QUAD[((prev_ab & 0x3) << 2) | (cur_ab & 0x3)];
}
