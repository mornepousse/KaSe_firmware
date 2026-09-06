/* Contrat de la keymap combinee du Niphargus (logique pure).
 *
 * La moitie gauche est le seul moteur keymap du clavier : la spec la decrit
 * comme « la seule a produire un rapport HID », la droite n'etant qu'un scanner
 * qui remonte sa matrice brute. La gauche doit donc porter des keycodes pour
 * les 52 touches, alors qu'elle n'en SCANNE que 26.
 *
 * D'ou deux notions a ne pas confondre, et que keyboard_config.h avait deja
 * nommees sans jamais les cabler — KEYMAP_COLS y valait MATRIX_COLS et n'etait
 * utilise nulle part :
 *
 *   MATRIX_COLS  colonnes que CETTE carte balaie physiquement       (7)
 *   KEYMAP_COLS  colonnes que la keymap couvre, les deux moities   (14)
 *
 * Convention retenue : colonnes 0..6 = moitie gauche, 7..13 = moitie droite.
 * La coordonnee recue de la droite se traduit par un decalage de MATRIX_COLS.
 */
#include "test_framework.h"

#ifndef GPIO_NUM_0
#define GPIO_NUM_0 0
#endif
#include "../boards/niphar_left/board.h"
#include "../main/input/keyboard_config.h"

static void test_scan_reste_a_sept_colonnes(void)
{
    /* Elargir la keymap ne doit PAS elargir le balayage : la gauche n'a que
     * sept colonnes cablees, et les piloter au-dela toucherait d'autres
     * fonctions (GPIO13 = jauge batterie, 14 = CS ecran, 15/16 = nRF24). */
    TEST_ASSERT_EQ(MATRIX_ROWS, 4, "la gauche balaie 4 rangees");
    TEST_ASSERT_EQ(MATRIX_COLS, 7, "la gauche balaie 7 colonnes, inchange");
}

static void test_keymap_couvre_les_deux_moities(void)
{
    TEST_ASSERT_EQ(KEYMAP_COLS, 2 * MATRIX_COLS, "la keymap couvre les deux moities");
    TEST_ASSERT_EQ(KEYMAP_COLS, 14, "soit 14 colonnes");
    /* 4 rangees x 14 colonnes = 56 positions pour 52 touches reelles : les
     * quatre manquantes sont les trous des deux dernieres rangees (7/7/6/6). */
    TEST_ASSERT_EQ(MATRIX_ROWS * KEYMAP_COLS, 56, "56 positions de keymap");
}

static void test_les_tailles_derivees_suivent(void)
{
    /* ⚠ Ces deux macros ne sont utilisees NULLE PART aujourd'hui (verifie au
     * grep) : elles sont mortes. On les verrouille tout de meme sur la keymap,
     * parce que leur nom promet de dimensionner un rapport de touches — et le
     * jour ou quelqu'un s'en servira, il vaut mieux qu'elles couvrent les 52
     * touches que les 26 balayees.
     *
     * Les statistiques de frappe, elles, suivent la matrice BALAYEE :
     * key_stats est dimensionne en MATRIX_COLS, et boucler dessus en
     * KEYMAP_COLS a produit un depassement de tableau que le compilateur a
     * attrape (cdc_binary_cmds.c, « iteration 7 invokes undefined behavior »). */
    TEST_ASSERT_EQ(REPORT_COUNT_BYTES, MATRIX_ROWS * KEYMAP_COLS,
                   "les stats couvrent la keymap entiere");
    TEST_ASSERT_EQ(REPORT_LEN, MOD_LED_BYTES + MATRIX_ROWS * KEYMAP_COLS,
                   "le rapport couvre la keymap entiere");
}

void test_niphar_keymap_span(void)
{
    printf("\n-- keymap combinee Niphargus (gauche = moteur des 52 touches) --\n");
    test_scan_reste_a_sept_colonnes();
    test_keymap_couvre_les_deux_moities();
    test_les_tailles_derivees_suivent();
}
