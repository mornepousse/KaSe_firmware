#ifndef BOARD_H
#define BOARD_H

/* Niphargus — moitié GAUCHE (U6), le MAÎTRE.
 *
 * Brochage : docs/NIPHARGUS_V2_HARDWARE.md, vérifié à la netlist le 2026-08-06.
 * ⚠ La table de la moitié DROITE est différente (permutations de routage) —
 * ne jamais recopier l'une depuis l'autre.
 * Verrouillé par test/test_niphar_left_pins.c.
 */

#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#endif

/* ── Identité produit ── */
#define GATTS_TAG           "Niphargus_L"
#define MANUFACTURER_NAME   "Mae"
#define PRODUCT_NAME        "Niphargus Left"
#define SERIAL_NUMBER       "N/A"
#define MODULE_ID           0x10

/* ── Matrice : COL → switch → diode → ROW ──────────────────────
 * Le scan PILOTE les colonnes et LIT les rows (BOARD_MATRIX_COL2ROW).
 * Réveil de sommeil profond : EXT1 sur les ROWS (tous en domaine RTC). */
#define ROWS0  GPIO_NUM_1
#define ROWS1  GPIO_NUM_2
#define ROWS2  GPIO_NUM_8
#define ROWS3  GPIO_NUM_6

#define COLS0  GPIO_NUM_4
#define COLS1  GPIO_NUM_5
#define COLS2  GPIO_NUM_7
#define COLS3  GPIO_NUM_9
#define COLS4  GPIO_NUM_10
#define COLS5  GPIO_NUM_11
#define COLS6  GPIO_NUM_12

/* main/input/matrix_scan.c hardcode un initialiseur fixe COLS0..COLS12 /
 * ROWS0..ROWS4 (forme KaSe 5×13) quelle que soit la géométrie réelle du
 * board : matrix_arm_key_wake()/matrix_disarm_key_wake()/matrix_setup()
 * bornent leurs boucles sur MATRIX_COLS/MATRIX_ROWS (7/4 ici, corrects), mais
 * l'initialiseur C lui-même doit nommer 13 COLS et 5 ROWS pour compiler — les
 * valeurs en trop (COLS7..COLS12, ROWS4) sont des "excess elements"
 * silencieusement ignorés par le compilateur, jamais lues à l'exécution.
 * GPIO_NUM_NC : aucun pin réel n'existe au-delà de COLS6/ROWS3 sur cette
 * moitié, donc pas de numéro à inventer. Généraliser matrix_scan.c à une
 * géométrie arbitraire est hors scope de la tâche 1 (board.h uniquement). */
#define COLS7   GPIO_NUM_NC
#define COLS8   GPIO_NUM_NC
#define COLS9   GPIO_NUM_NC
#define COLS10  GPIO_NUM_NC
#define COLS11  GPIO_NUM_NC
#define COLS12  GPIO_NUM_NC
#define ROWS4   GPIO_NUM_NC

/* 26 touches, rangées de 7/7/6/6 : deux positions de la grille sont vides. */
#define MATRIX_ROWS  4
#define MATRIX_COLS  7

/* ── Radio nRF24L01+ (SPI2, partagé avec l'écran côté droit) ── */
#define BOARD_NRF_SPI_HOST   SPI2_HOST
#define BOARD_NRF_SCK        GPIO_NUM_38
#define BOARD_NRF_MISO       GPIO_NUM_39
#define BOARD_NRF_MOSI       GPIO_NUM_40
#define BOARD_NRF_CE         GPIO_NUM_15
#define BOARD_NRF_CSN        GPIO_NUM_16
#define BOARD_NRF_IRQ        GPIO_NUM_41

/* ── Lien inter-moitiés (TRRS, UART1) ──────────────────────────
 * Câble droit : TX arrive sur TX. UNE moitié doit échanger TXD/RXD via la
 * matrice GPIO — c'est la gauche. Ne jamais driver les deux TX sans ce swap. */
#define BOARD_LINK_UART_NUM    1
#define BOARD_LINK_TX          GPIO_NUM_17
#define BOARD_LINK_RX          GPIO_NUM_18
#define BOARD_LINK_SWAP_TX_RX  1
/* ON du SiP32431, pull-down 100 k : le 5 V est MORT par défaut. Ne lever
 * qu'après une poignée de main aboutie (link_handshake.h). */
#define BOARD_LINK_5V_EN       GPIO_NUM_21

/* ── Jauge batterie ────────────────────────────────────────────
 * ADC2_CH2, diviseur 1M/1M + 100 nF. ADC2 est utilisable parce qu'il n'y a
 * pas de WiFi ; batterie pleine ≈ 4,15 V ÷ 2. */
#define BOARD_VBAT_SENSE_GPIO  GPIO_NUM_13

/* ── Trackpad Azoteq TPS43 (IQS572) — gauche uniquement ────────
 * NRST du trackpad = RC matériel, aucun GPIO. RDY obligatoire (handshake). */
#define BOARD_HAS_TRACKPAD_LOCAL  1
#define BOARD_TRACK_SDA_GPIO      GPIO_NUM_47
#define BOARD_TRACK_SCL_GPIO      GPIO_NUM_48
#define BOARD_TRACK_RDY_GPIO      GPIO_NUM_42

/* ── Pas d'écran sur la gauche (connecteur J12 non peuplé) ──────
 * docs/NIPHARGUS_V2_HARDWARE.md : l'écran Sharp memory-LCD (J4, SPI,
 * write-only, CS=GPIO14) n'est peuplé que côté DROITE ; le connecteur miroir
 * J12 côté gauche est vide. Le rôle KASE_DEVICE_ROLE_KEYBOARD compile
 * inconditionnellement le backend display (main/CMakeLists.txt tranche entre
 * round/oled sur la seule présence de BOARD_DISPLAY_BACKEND_ROUND, sinon
 * oled par défaut) — il n'existe aujourd'hui aucun interrupteur board.h pour
 * dire "pas d'écran du tout". Les macros ci-dessous ne décrivent AUCUN pin
 * réel : GPIO_NUM_NC partout où le contrat n'a rien à offrir, pour ne jamais
 * inventer un numéro. Elles satisfont uniquement la compilation du backend
 * OLED mort (jamais flashé côté gauche) ; désactiver proprement la
 * compilation display pour ce rôle est un chantier à part (voir rapport de
 * tâche 1). */
#define BOARD_DISPLAY_BUS           DISPLAY_BUS_I2C
#define BOARD_DISPLAY_WIDTH         0
#define BOARD_DISPLAY_HEIGHT        0
#define BOARD_DISPLAY_CLK_HZ        (400 * 1000)
#define BOARD_DISPLAY_RESET         GPIO_NUM_NC
#define BOARD_DISPLAY_I2C_HOST      I2C_NUM_0
#define BOARD_DISPLAY_I2C_SDA       GPIO_NUM_NC
#define BOARD_DISPLAY_I2C_SCL       GPIO_NUM_NC
#define BOARD_DISPLAY_I2C_ADDR      0x00
#define BOARD_DISPLAY_I2C_PULLUPS   false
#define UI_FONT                     &lv_font_montserrat_14
#define BOARD_DISPLAY_SLEEP_MS      0

#define BOARD_HAS_LED_STRIP  0

/* ── Scan matrice ──────────────────────────────────────────────
 * SETTLING/RECOVERY à 0 reprend le réglage des boards KaSe. Suspecté de
 * participer aux frappes ratées (docs/DONGLE_ARCHI_ET_HALF_TYPING_2026-07-13.md,
 * bug #2) — à mesurer au banc, ne pas régler à l'aveugle. */
#define BOARD_MATRIX_COL2ROW
#define BOARD_MATRIX_SCAN_INTERVAL_US  1000
#define BOARD_MATRIX_SETTLING_US       0
#define BOARD_MATRIX_RECOVERY_US       0
#define BOARD_DEBOUNCE_TICKS           3

/* ── USB ── */
#define BOARD_USB_VID  0xCafe
#define BOARD_USB_PID  0x4003

#endif /* BOARD_H */
