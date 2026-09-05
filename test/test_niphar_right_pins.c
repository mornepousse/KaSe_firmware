/* Contrat de brochage — Niphargus moitié DROITE (U5).
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

#include "../boards/niphar_right/board.h"

/* Garde de compilation : le trackpad est GAUCHE uniquement (Azoteq TPS43,
 * I2C + RDY, cf. docs/NIPHARGUS_V2_HARDWARE.md). Une macro
 * BOARD_HAS_TRACKPAD_LOCAL égarée ici décrirait un périphérique qui n'existe
 * pas sur cette moitié — on fait planter la compilation plutôt que de
 * laisser passer silencieusement. */
#ifdef BOARD_HAS_TRACKPAD_LOCAL
#error "le trackpad est sur la gauche uniquement : BOARD_HAS_TRACKPAD_LOCAL n'a rien à faire dans boards/niphar_right/board.h"
#endif

/* GPIO non câblés : strapping et PSRAM octale. Aucun pin du board ne doit
 * tomber dedans. */
static int is_forbidden(int gpio)
{
    return gpio == 3 || gpio == 45 || gpio == 46 ||
           gpio == 35 || gpio == 36 || gpio == 37;
}

/* GPIO déjà engagés à autre chose que la matrice/périphériques du board :
 * USB D-/D+ (19/20, NIPHARGUS_V2_HARDWARE.md:41) et le connecteur de prog
 * (0, 43, 44, :47). COLS6 = GPIO1 et ROWS0 = GPIO2 étant déjà à un chiffre
 * des numéros de radio, un second contrôle ici couvre les pins engagés que
 * test_right_no_forbidden_gpio ne voit pas (pas dans la liste des non
 * câblés). */
static int is_reserved(int gpio)
{
    return gpio == 19 || gpio == 20 ||
           gpio == 0 || gpio == 43 || gpio == 44;
}

static void test_right_matrix_table(void)
{
    /* Table DROITE du contrat — différente de la gauche, ce n'est PAS une
     * symétrie : sur les 11 pins de la matrice, une seule coïncide entre
     * les deux moitiés (col3 = GPIO9) ; les 10 autres diffèrent. */
    TEST_ASSERT_EQ(ROWS0, 2,  "droite row0 = GPIO2");
    TEST_ASSERT_EQ(ROWS1, 12, "droite row1 = GPIO12");
    TEST_ASSERT_EQ(ROWS2, 4,  "droite row2 = GPIO4");
    TEST_ASSERT_EQ(ROWS3, 5,  "droite row3 = GPIO5");

    TEST_ASSERT_EQ(COLS0, 6,  "droite col0 = GPIO6");
    TEST_ASSERT_EQ(COLS1, 7,  "droite col1 = GPIO7");
    TEST_ASSERT_EQ(COLS2, 8,  "droite col2 = GPIO8");
    TEST_ASSERT_EQ(COLS3, 9,  "droite col3 = GPIO9");
    TEST_ASSERT_EQ(COLS4, 11, "droite col4 = GPIO11");
    TEST_ASSERT_EQ(COLS5, 10, "droite col5 = GPIO10");
    TEST_ASSERT_EQ(COLS6, 1,  "droite col6 = GPIO1");
}

static void test_right_matrix_geometry(void)
{
    TEST_ASSERT_EQ(MATRIX_ROWS, 4, "4 rangées");
    TEST_ASSERT_EQ(MATRIX_COLS, 7, "7 colonnes");
}

static void test_right_peripheral_pins(void)
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
}

static void test_right_display_pins(void)
{
    /* Sharp LS011B7DH03 : CS ACTIF HAUT, write-only, LSB-first. */
    TEST_ASSERT_EQ(BOARD_LCD_CS_GPIO, 14, "CS de l'écran Sharp");
    TEST_ASSERT_EQ(BOARD_LCD_CS_ACTIVE_HIGH, 1, "CS actif HAUT, pas bas");
    /* L'écran partage le SPI de la nRF24. */
    TEST_ASSERT_EQ(BOARD_NRF_SCK,  38, "SPI SCK partagé écran + radio");
    TEST_ASSERT_EQ(BOARD_NRF_MOSI, 40, "SPI MOSI partagé écran + radio");
}

static void test_right_no_forbidden_gpio(void)
{
    const int pins[] = {
        ROWS0, ROWS1, ROWS2, ROWS3,
        COLS0, COLS1, COLS2, COLS3, COLS4, COLS5, COLS6,
        BOARD_NRF_SCK, BOARD_NRF_MISO, BOARD_NRF_MOSI,
        BOARD_NRF_CE, BOARD_NRF_CSN, BOARD_NRF_IRQ,
        BOARD_LINK_TX, BOARD_LINK_RX, BOARD_LINK_5V_EN,
        BOARD_VBAT_SENSE_GPIO,
        BOARD_LCD_CS_GPIO,
    };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        TEST_ASSERT(!is_forbidden(pins[i]), "aucun pin sur un GPIO non câblé");
}

static void test_right_no_reserved_gpio(void)
{
    /* USB D-/D+ et connecteur de prog : engagés ailleurs, pas dans la liste
     * des non-câblés donc invisibles à test_right_no_forbidden_gpio. */
    const int pins[] = {
        ROWS0, ROWS1, ROWS2, ROWS3,
        COLS0, COLS1, COLS2, COLS3, COLS4, COLS5, COLS6,
        BOARD_NRF_SCK, BOARD_NRF_MISO, BOARD_NRF_MOSI,
        BOARD_NRF_CE, BOARD_NRF_CSN, BOARD_NRF_IRQ,
        BOARD_LINK_TX, BOARD_LINK_RX, BOARD_LINK_5V_EN,
        BOARD_VBAT_SENSE_GPIO,
        BOARD_LCD_CS_GPIO,
    };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); i++)
        TEST_ASSERT(!is_reserved(pins[i]), "aucun pin sur l'USB natif ou le connecteur de prog");
}

static void test_right_no_pin_used_twice(void)
{
    /* Une permutation ratée produit typiquement un doublon — sur TOUS les
     * pins du board (matrice + SPI + nRF + lien + jauge + écran), pas
     * seulement la matrice : COLS6 = GPIO1 et ROWS0 = GPIO2 sont à un
     * chiffre des numéros de radio (CE=15/CSN=16), une faute de frappe qui
     * poserait BOARD_NRF_CE sur GPIO12 ne serait pas vue si on ne regardait
     * que la matrice. */
    const int pins[] = {
        ROWS0, ROWS1, ROWS2, ROWS3,
        COLS0, COLS1, COLS2, COLS3, COLS4, COLS5, COLS6,
        BOARD_NRF_SCK, BOARD_NRF_MISO, BOARD_NRF_MOSI,
        BOARD_NRF_CE, BOARD_NRF_CSN, BOARD_NRF_IRQ,
        BOARD_LINK_TX, BOARD_LINK_RX, BOARD_LINK_5V_EN,
        BOARD_VBAT_SENSE_GPIO,
        BOARD_LCD_CS_GPIO,
    };
    const unsigned n = sizeof(pins) / sizeof(pins[0]);
    for (unsigned i = 0; i < n; i++)
        for (unsigned j = i + 1; j < n; j++)
            TEST_ASSERT(pins[i] != pins[j], "aucun GPIO en double sur tout le board");
}

static void test_right_does_not_swap_the_link_uart(void)
{
    /* C'est la GAUCHE qui swappe. Si les deux swappent, ou aucune, deux TX se
     * retrouvent en conflit sur le même fil. */
    TEST_ASSERT_EQ(BOARD_LINK_SWAP_TX_RX, 0, "la droite ne swappe pas");
}

/* Les consommateurs de la pile RF (comm/rf/kbd_relay_tx.c) construisent leur
 * config depuis BOARD_NRF_SPI_SCK, BOARD_NRF_CSN_GPIO... Ces noms different de
 * ceux du contrat materiel (BOARD_NRF_SCK, BOARD_NRF_CSN), et kbd_relay_tx.c
 * fournit un bloc de repli — GPIO 35/36/37, interdits ici — sous
 * `#ifndef BOARD_NRF_SPI_HOST`.
 *
 * Les board.h Niphargus DEFINISSENT BOARD_NRF_SPI_HOST : le garde est donc
 * faux, le bloc de repli entier est saute, et les alias ne sont definis nulle
 * part. Le mode de defaillance n'est pas un mauvais brochage silencieux mais
 * une erreur de compilation, le jour ou un consommateur de la pile RF sera
 * compile pour une moitie. C'est moins grave, ca reste a fermer.
 *
 * boards/conchodytes/board.h a du ajouter ce bloc d'alias pour la meme raison,
 * en documentant le piege. Les deux moities l'avaient oublie. Ce test
 * verrouille l'equivalence : un alias absent ne compile pas, un alias qui
 * derive echoue ici — pas au banc, six mois plus tard.
 *
 * On n'asserte volontairement NI canal NI suffixe d'adresse : contrairement a
 * la souris qui n'a qu'un lien, une moitie en a deux (PRX vers l'autre moitie,
 * PTX vers le dongle). Le choix des canaux appartient a B3/B4. */
static void test_right_radio_pin_aliases(void)
{
    TEST_ASSERT_EQ(BOARD_NRF_SPI_SCK,  BOARD_NRF_SCK,  "alias SCK  == pin brute");
    TEST_ASSERT_EQ(BOARD_NRF_SPI_MISO, BOARD_NRF_MISO, "alias MISO == pin brute");
    TEST_ASSERT_EQ(BOARD_NRF_SPI_MOSI, BOARD_NRF_MOSI, "alias MOSI == pin brute");
    TEST_ASSERT_EQ(BOARD_NRF_CSN_GPIO, BOARD_NRF_CSN,  "alias CSN  == pin brute");
    TEST_ASSERT_EQ(BOARD_NRF_CE_GPIO,  BOARD_NRF_CE,   "alias CE   == pin brute");
    TEST_ASSERT_EQ(BOARD_NRF_IRQ_GPIO, BOARD_NRF_IRQ,  "alias IRQ  == pin brute");
}

void test_niphar_right_pins(void)
{
    printf("\n-- brochage Niphargus DROITE (contrat netlist 2026-08-06) --\n");
    test_right_matrix_table();
    test_right_matrix_geometry();
    test_right_peripheral_pins();
    test_right_display_pins();
    test_right_no_forbidden_gpio();
    test_right_no_reserved_gpio();
    test_right_no_pin_used_twice();
    test_right_does_not_swap_the_link_uart();
    test_right_radio_pin_aliases();
}
