/* Contrat de brochage — Niphargus moitié GAUCHE (U6).
 *
 * Recopié à la main de docs/NIPHARGUS_V2_HARDWARE.md (netlist vérifiée le
 * 2026-08-06). Les deux moitiés ont des tables DIFFÉRENTES : ce sont des
 * permutations de routage, pas une symétrie. Aucune compilation ne détecte une
 * inversion, et les cartes ne sont pas arrivées — ce test est la seule barrière
 * avant le banc. Si le contrat change, c'est CE fichier qu'on met à jour en
 * premier, puis board.h.
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

#include "../boards/niphar_left/board.h"

/* GPIO non câblés : strapping et PSRAM octale. Aucun pin du board ne doit
 * tomber dedans. */
static int is_forbidden(int gpio)
{
    return gpio == 3 || gpio == 45 || gpio == 46 ||
           gpio == 35 || gpio == 36 || gpio == 37;
}

static void test_left_matrix_table(void)
{
    /* Table GAUCHE du contrat — NE PAS recopier depuis la table droite. */
    TEST_ASSERT_EQ(ROWS0, 1,  "gauche row0 = GPIO1");
    TEST_ASSERT_EQ(ROWS1, 2,  "gauche row1 = GPIO2");
    TEST_ASSERT_EQ(ROWS2, 8,  "gauche row2 = GPIO8");
    TEST_ASSERT_EQ(ROWS3, 6,  "gauche row3 = GPIO6");

    TEST_ASSERT_EQ(COLS0, 4,  "gauche col0 = GPIO4");
    TEST_ASSERT_EQ(COLS1, 5,  "gauche col1 = GPIO5");
    TEST_ASSERT_EQ(COLS2, 7,  "gauche col2 = GPIO7");
    TEST_ASSERT_EQ(COLS3, 9,  "gauche col3 = GPIO9");
    TEST_ASSERT_EQ(COLS4, 10, "gauche col4 = GPIO10");
    TEST_ASSERT_EQ(COLS5, 11, "gauche col5 = GPIO11");
    TEST_ASSERT_EQ(COLS6, 12, "gauche col6 = GPIO12");
}

static void test_left_matrix_geometry(void)
{
    TEST_ASSERT_EQ(MATRIX_ROWS, 4, "4 rangées");
    TEST_ASSERT_EQ(MATRIX_COLS, 7, "7 colonnes");
}

static void test_left_peripheral_pins(void)
{
    TEST_ASSERT_EQ(BOARD_NRF_SCK,  38, "SPI SCK partagé");
    TEST_ASSERT_EQ(BOARD_NRF_MISO, 39, "SPI MISO partagé");
    TEST_ASSERT_EQ(BOARD_NRF_MOSI, 40, "SPI MOSI partagé");
    TEST_ASSERT_EQ(BOARD_NRF_CE,   15, "nRF24 CE");
    TEST_ASSERT_EQ(BOARD_NRF_CSN,  16, "nRF24 CSN");
    TEST_ASSERT_EQ(BOARD_NRF_IRQ,  41, "nRF24 IRQ");

    TEST_ASSERT_EQ(BOARD_LINK_TX,    17, "TRRS TX (UART1)");
    TEST_ASSERT_EQ(BOARD_LINK_RX,    18, "TRRS RX (UART1)");
    TEST_ASSERT_EQ(BOARD_LINK_5V_EN, 21, "ON du SiP32431");

    TEST_ASSERT_EQ(BOARD_VBAT_SENSE_GPIO, 13, "jauge ADC2_CH2");

    /* Trackpad : gauche uniquement. */
    TEST_ASSERT_EQ(BOARD_TRACK_SDA_GPIO, 47, "trackpad SDA");
    TEST_ASSERT_EQ(BOARD_TRACK_SCL_GPIO, 48, "trackpad SCL");
    TEST_ASSERT_EQ(BOARD_TRACK_RDY_GPIO, 42, "trackpad RDY");
    TEST_ASSERT_EQ(BOARD_HAS_TRACKPAD_LOCAL, 1, "le trackpad est sur la gauche");
}

static void test_left_no_forbidden_gpio(void)
{
    const int pins[] = {
        ROWS0, ROWS1, ROWS2, ROWS3,
        COLS0, COLS1, COLS2, COLS3, COLS4, COLS5, COLS6,
        BOARD_NRF_SCK, BOARD_NRF_MISO, BOARD_NRF_MOSI,
        BOARD_NRF_CE, BOARD_NRF_CSN, BOARD_NRF_IRQ,
        BOARD_LINK_TX, BOARD_LINK_RX, BOARD_LINK_5V_EN,
        BOARD_VBAT_SENSE_GPIO,
        BOARD_TRACK_SDA_GPIO, BOARD_TRACK_SCL_GPIO, BOARD_TRACK_RDY_GPIO,
    };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        TEST_ASSERT(!is_forbidden(pins[i]), "aucun pin sur un GPIO non câblé");
}

static void test_left_no_pin_used_twice(void)
{
    /* Une permutation ratée produit typiquement un doublon. */
    const int pins[] = {
        ROWS0, ROWS1, ROWS2, ROWS3,
        COLS0, COLS1, COLS2, COLS3, COLS4, COLS5, COLS6,
    };
    const unsigned n = sizeof(pins) / sizeof(pins[0]);
    for (unsigned i = 0; i < n; i++)
        for (unsigned j = i + 1; j < n; j++)
            TEST_ASSERT(pins[i] != pins[j], "aucun GPIO matrice en double");
}

static void test_left_swaps_the_link_uart(void)
{
    /* Câble droit : TX arrive sur TX. UNE moitié doit échanger TXD/RXD via la
     * matrice GPIO — on a choisi la gauche. La droite ne doit PAS swapper
     * (vérifié dans test_niphar_right_pins.c). */
    TEST_ASSERT_EQ(BOARD_LINK_SWAP_TX_RX, 1, "la gauche swappe TX/RX");
}

void test_niphar_left_pins(void)
{
    printf("\n-- brochage Niphargus GAUCHE (contrat netlist 2026-08-06) --\n");
    test_left_matrix_table();
    test_left_matrix_geometry();
    test_left_peripheral_pins();
    test_left_no_forbidden_gpio();
    test_left_no_pin_used_twice();
    test_left_swaps_the_link_uart();
}
