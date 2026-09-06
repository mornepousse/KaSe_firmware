/* Contrat de brochage — souris Conchodytes (U6).
 *
 * Relevé à la netlist de ~/Documents/GitHub/Conchodytes/hardware/pcb/ au
 * `kicad-cli export netlist` le 2026-08-25, PAS recopié du README : celui-ci
 * inversait gauche et droite sur les contacts NC (corrigé depuis, mais la
 * netlist reste la source).
 *
 * Ce fichier existe pour une raison précise : sur cette carte, une inversion de
 * brochage ne se voit ni à la compilation, ni au démarrage, ni même à l'usage
 * naïf. Les six contacts de clic vont par paires NO+NC du MÊME bouton, et c'est
 * cet appariement qui supprime le rebond ; croiser deux paires donne un firmware
 * qui semble marcher et qui produit des clics fantômes. Si le contrat change,
 * c'est CE fichier qu'on met à jour en premier, puis board.h.
 *
 * Les valeurs ci-dessous ont été vérifiées sur carte réelle le 2026-08-25 :
 * appui sur le seul clic gauche → 26 fronts sur G, 0 sur D.
 */
#include "test_framework.h"

/* ESP-IDF définit GPIO_NUM_* comme un enum ; sur host on les stube. */
#ifndef GPIO_NUM_0
#define GPIO_NUM_0  0
#define GPIO_NUM_1  1
#define GPIO_NUM_2  2
#define GPIO_NUM_4  4
#define GPIO_NUM_5  5
#define GPIO_NUM_6  6
#define GPIO_NUM_7  7
#define GPIO_NUM_8  8
#define GPIO_NUM_9  9
#define GPIO_NUM_10 10
#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15
#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_21 21
#define GPIO_NUM_38 38
#define GPIO_NUM_39 39
#define GPIO_NUM_40 40
#define GPIO_NUM_41 41
#define GPIO_NUM_42 42
#define GPIO_NUM_47 47
#define GPIO_NUM_48 48
#define GPIO_NUM_NC (-1)
#define SPI2_HOST   1
#endif

#include "../boards/conchodytes/board.h"
#include "../main/comm/rf/rf_slot.h"

/* Gardes de compilation : la souris n'a ni matrice, ni écran, ni trackpad.
 * Une macro égarée ici décrirait un périphérique qui n'existe pas sur cette
 * carte — on fait planter la compilation plutôt que de laisser passer. */
#ifdef BOARD_HAS_TRACKPAD_LOCAL
#error "la souris n'a pas de trackpad : BOARD_HAS_TRACKPAD_LOCAL n'a rien a faire dans boards/conchodytes/board.h"
#endif
/* MATRIX_ROWS/COLS viennent de test_framework.h et sont donc toujours definis
 * ici : on garde sur ROWS0/COLS0, qui n'existent que dans un board.h de
 * clavier. */
#ifdef ROWS0
#error "la souris n'a aucun interrupteur en matrice : ROWS0 n'a rien a faire dans boards/conchodytes/board.h"
#endif
#ifdef COLS0
#error "la souris n'a aucun interrupteur en matrice : COLS0 n'a rien a faire dans boards/conchodytes/board.h"
#endif
#ifdef BOARD_DISPLAY_BACKEND_ROUND
#error "la souris n'a pas d'ecran : BOARD_DISPLAY_BACKEND_ROUND n'a rien a faire dans boards/conchodytes/board.h"
#endif

/* GPIO non câblés sur cette carte : strapping et PSRAM octale. */
static int conch_is_forbidden(int gpio)
{
    return gpio == 3 || gpio == 45 || gpio == 46 ||
           gpio == 35 || gpio == 36 || gpio == 37;
}

/* GPIO engagés ailleurs : USB natif (19/20, broches 13/14 du module) et le
 * connecteur de programmation J1 (0, 43, 44). */
static int conch_is_reserved(int gpio)
{
    return gpio == 19 || gpio == 20 ||
           gpio == 0 || gpio == 43 || gpio == 44;
}

static void test_conch_sensor_pins(void)
{
    /* PMW3389 : SPI partagé avec le nRF24, plus deux lignes propres. */
    TEST_ASSERT_EQ(BOARD_SNS_NCS_GPIO,    17, "capteur NCS = GPIO17 (broche 10)");
    TEST_ASSERT_EQ(BOARD_SNS_MOTION_GPIO, 18, "capteur MOTION = GPIO18 (broche 11)");

    /* fSCLK max = 2,0 MHz, Table 4 p. 15 de la datasheet PMW3389DM-T3QU.
     * Validé sur carte à cette fréquence le 2026-08-25. */
    TEST_ASSERT(BOARD_SNS_CLOCK_HZ <= 2000000, "fSCLK du PMW3389 plafonne a 2 MHz");
    TEST_ASSERT_EQ(BOARD_SNS_SPI_MODE, 3, "PMW3389 en mode 3 (CPOL=1, CPHA=1)");
}

static void test_conch_radio_pins(void)
{
    /* Le nRF24 partage SCK/MOSI/MISO avec le capteur, mais pas le mode SPI :
     * mode 0 ici, mode 3 pour le PMW3389. */
    TEST_ASSERT_EQ(BOARD_NRF_SCK,  38, "SPI SCK partage capteur + radio");
    TEST_ASSERT_EQ(BOARD_NRF_MISO, 39, "SPI MISO partage");
    TEST_ASSERT_EQ(BOARD_NRF_MOSI, 40, "SPI MOSI partage");
    TEST_ASSERT_EQ(BOARD_NRF_CSN,  2,  "nRF24 CSN = GPIO2 (broche 38)");
    TEST_ASSERT_EQ(BOARD_NRF_CE,   1,  "nRF24 CE = GPIO1 (broche 39)");
    TEST_ASSERT_EQ(BOARD_NRF_IRQ,  41, "nRF24 IRQ = GPIO41 (broche 34)");

    /* Slot 2 du dongle. boards/kase_dongle/board.h annote BOARD_NRF2_* comme
     * "slot souris (Conchodytes), canal 0x52" — les deux cotes doivent
     * s'accorder ou le lien ne s'etablit jamais. */
    TEST_ASSERT_EQ(BOARD_NRF_ADDR_SUFFIX, 0x02, "suffixe d'adresse = slot souris");
    TEST_ASSERT_EQ(BOARD_NRF_CHANNEL, RF_CH_MOUSE_DONGLE, "canal == plan de canaux");
    TEST_ASSERT_EQ(BOARD_NRF_CHANNEL,     0x52, "canal du slot 2, cf. board.h du dongle");
}

static void test_conch_click_pairs(void)
{
    /* LE test de ce fichier. Chaque bouton a son NO et son NC, et l'anti-rebond
     * lit LES DEUX. Croiser les paires donne un firmware qui produit des clics
     * fantomes sans qu'aucun outil ne s'en apercoive.
     *
     * ⚠ Le README annoncait LEFT_NC = GPIO4 et RIGHT_NC = GPIO5. La netlist dit
     * l'inverse : GPIO4 -> SW2.3 (clic DROIT, R109), GPIO5 -> SW1.3 (clic
     * GAUCHE, R108). Verifie sur carte : appui sur le seul clic gauche a produit
     * 26 fronts sur G et 0 sur D. */
    TEST_ASSERT_EQ(BOARD_SW_LEFT_GPIO,     10, "clic gauche NO = GPIO10");
    TEST_ASSERT_EQ(BOARD_SW_LEFT_NC_GPIO,   5, "clic gauche NC = GPIO5, PAS GPIO4");
    TEST_ASSERT_EQ(BOARD_SW_RIGHT_GPIO,    11, "clic droit NO = GPIO11");
    TEST_ASSERT_EQ(BOARD_SW_RIGHT_NC_GPIO,  4, "clic droit NC = GPIO4, PAS GPIO5");
    TEST_ASSERT_EQ(BOARD_SW_MID_GPIO,      12, "clic milieu NO = GPIO12");
    TEST_ASSERT_EQ(BOARD_SW_MID_NC_GPIO,    6, "clic milieu NC = GPIO6");

    /* Les trois NC sont sur 4/5/6 et les trois NO sur 10/11/12 : aucun NC ne
     * doit se retrouver dans la plage des NO, et reciproquement. Une faute de
     * frappe qui echangerait un NO et un NC produirait un bouton dont les deux
     * lignes sont du meme cote — invisible aux egalites ci-dessus si on les
     * mettait toutes a jour ensemble. */
    const int nc[] = { BOARD_SW_LEFT_NC_GPIO, BOARD_SW_RIGHT_NC_GPIO, BOARD_SW_MID_NC_GPIO };
    const int no[] = { BOARD_SW_LEFT_GPIO,    BOARD_SW_RIGHT_GPIO,    BOARD_SW_MID_GPIO };
    for (unsigned i = 0; i < 3; i++) {
        TEST_ASSERT(nc[i] >= 4  && nc[i] <= 6,  "les trois NC sont sur GPIO4-6");
        TEST_ASSERT(no[i] >= 10 && no[i] <= 12, "les trois NO sont sur GPIO10-12");
    }
}

static void test_conch_wheel_pins(void)
{
    TEST_ASSERT_EQ(BOARD_ENC_A_GPIO, 7, "encodeur voie A = GPIO7 (broche 7)");
    TEST_ASSERT_EQ(BOARD_ENC_B_GPIO, 9, "encodeur voie B = GPIO9 (broche 17)");
}

static void test_conch_battery_pin(void)
{
    /* ⚠ GPIO13 = ADC2_CH2, et sur ESP32-S3 l'ADC2 est partage avec le driver
     * WiFi : tant que la mesure batterie est la, le WiFi est interdit sur cette
     * carte. C'est sans consequence aujourd'hui (la radio est un nRF24 et le
     * firmware n'allume jamais le WiFi) mais c'est une hypotheque.
     * NOTES-V2.md point 7 prevoit de deplacer la mesure sur GPIO8 / ADC1_CH7
     * en v2. Quand ce sera fait, c'est cette valeur qui change en premier. */
    TEST_ASSERT_EQ(BOARD_VBAT_SENSE_GPIO, 13, "jauge sur ADC2_CH2 en v1");
}

/* Tous les pins du board, en un seul endroit — les trois tests suivants en
 * dependent et doivent voir exactement la meme liste. */
#define CONCH_ALL_PINS \
    BOARD_SNS_NCS_GPIO, BOARD_SNS_MOTION_GPIO, \
    BOARD_NRF_SCK, BOARD_NRF_MISO, BOARD_NRF_MOSI, \
    BOARD_NRF_CSN, BOARD_NRF_CE, BOARD_NRF_IRQ, \
    BOARD_SW_LEFT_GPIO,  BOARD_SW_LEFT_NC_GPIO, \
    BOARD_SW_RIGHT_GPIO, BOARD_SW_RIGHT_NC_GPIO, \
    BOARD_SW_MID_GPIO,   BOARD_SW_MID_NC_GPIO, \
    BOARD_ENC_A_GPIO, BOARD_ENC_B_GPIO, \
    BOARD_VBAT_SENSE_GPIO

static void test_conch_no_forbidden_gpio(void)
{
    const int pins[] = { CONCH_ALL_PINS };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        TEST_ASSERT(!conch_is_forbidden(pins[i]), "aucun pin sur un GPIO non cable");
}

static void test_conch_no_reserved_gpio(void)
{
    const int pins[] = { CONCH_ALL_PINS };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        TEST_ASSERT(!conch_is_reserved(pins[i]), "aucun pin sur l'USB natif ou J1");
}

static void test_conch_no_pin_used_twice(void)
{
    const int pins[] = { CONCH_ALL_PINS };
    const unsigned n = sizeof(pins) / sizeof(pins[0]);
    for (unsigned i = 0; i < n; i++)
        for (unsigned j = i + 1; j < n; j++)
            TEST_ASSERT(pins[i] != pins[j], "aucun GPIO en double sur tout le board");
}

void test_conchodytes_pins(void)
{
    printf("\n-- brochage Conchodytes (contrat netlist 2026-08-25) --\n");
    test_conch_sensor_pins();
    test_conch_radio_pins();
    test_conch_click_pairs();
    test_conch_wheel_pins();
    test_conch_battery_pin();
    test_conch_no_forbidden_gpio();
    test_conch_no_reserved_gpio();
    test_conch_no_pin_used_twice();
}
