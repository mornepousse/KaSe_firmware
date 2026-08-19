# Firmware Niphargus — plan d'implémentation, phase 1

> **Pour les agents :** SOUS-COMPÉTENCE REQUISE — utiliser
> `superpowers:subagent-driven-development` (recommandé) ou
> `superpowers:executing-plans` pour dérouler ce plan tâche par tâche. Les étapes
> sont en cases à cocher (`- [ ]`).

**But :** poser les deux boards Niphargus et la logique pure du lien
inter-moitiés, en n'écrivant que ce qu'un oracle peut valider sans matériel.

**Architecture :** la moitié gauche est le maître (seul moteur keymap, trackpad
local, USB HID) ; la droite est un scanner qui remonte sa matrice. En phase 1 on
construit les définitions de board, leur test de contrat de brochage, et les
protocoles purs du lien filaire. Aucune radio, aucune UART réelle, aucun
périphérique.

**Pile :** ESP-IDF 5.5, C11, tests host CMake dans `test/`.

**Spec :** `docs/superpowers/specs/2026-08-19-niphargus-firmware-design.md`
**Contrat matériel :** `docs/NIPHARGUS_V2_HARDWARE.md` (source de vérité du brochage)

## Contraintes globales

Elles s'appliquent implicitement à **toutes** les tâches.

- **Les cartes ne sont pas arrivées.** La compilation et les tests host sont les
  seuls oracles. Toute logique dont le comportement ne serait vérifiable qu'au
  banc est hors phase 1 — la repousser, ne pas l'écrire à l'aveugle.
- **Le brochage vient de `docs/NIPHARGUS_V2_HARDWARE.md`**, pas d'une mémoire ni
  d'une déduction. Les deux tables diffèrent par des permutations de routage.
- **GPIO interdits : 3, 45, 46, 35, 36, 37** (strapping / PSRAM octale, non câblés).
- **Chaîne matrice : COL → switch → anode-diode-cathode → ROW.** Le scan pilote
  les colonnes et lit les rows. 26 touches par moitié, rangées de 7/7/6/6, donc
  `MATRIX_ROWS 4` / `MATRIX_COLS 7` avec deux positions inutilisées.
- **Jamais de WiFi ni de BLE.** Aucune tâche ne doit activer `KASE_HAS_BLE` ni
  tirer `esp_wifi`.
- **Protocole RF KaSe inchangé** : le bitmap reste `RF_HALF_ROWS 5` ×
  `RF_HALF_COLS 7` (`rf_packet.h`). Niphargus n'utilise que les rows 0..3 ; les
  7 bits de queue restent à zéro. Ne pas redimensionner le bitmap.
- **Le swap TX/RX du lien TRRS ne se fait que d'UN côté.** Piloter les deux TX
  sans swap met deux sorties en conflit sur le même fil. Le contrat matériel
  classe ça en exigence non négociable.
- **`LINK_5V_EN` a un pull-down 100 k : le 5 V est mort par défaut.** Aucun code
  ne doit le lever avant que la poignée de main ait abouti.
- Chaque tâche se termine sur `./scripts/check.sh` vert et un commit.

### Décision de câblage Kconfig

La moitié **gauche réutilise le rôle `KASE_DEVICE_ROLE_KEYBOARD`** existant, plus
un booléen `KASE_NIPHAR_MASTER` pour ce qui est propre au split. Elle hérite ainsi
sans effort de la matrice locale, du moteur, du HID et de tout le CDC binaire —
c'est ce qui rend la brick B8 (config USB) vide en phase 1 : elle est satisfaite
par construction.

La moitié **droite reçoit un nouveau rôle `KASE_DEVICE_ROLE_NIPHAR_SLAVE`**, qui
ne compile ni display ni BLE et a donc besoin de symboles bouchons, comme le
dongle en a (`cdc_dongle_stubs.c`).

---

### Tâche 1 : Moitié gauche — brochage verrouillé, board, build

**Fichiers :**
- Créer : `test/test_niphar_left_pins.c`
- Créer : `boards/niphar_left/board.h`
- Créer : `boards/niphar_left/board_keymap.c`
- Créer : `boards/niphar_left/board_layout.c`
- Créer : `sdkconfig.defaults.niphar_left`
- Modifier : `test/CMakeLists.txt`, `test/test_main.c`
- Modifier : `main/Kconfig.projbuild`
- Modifier : `scripts/check.sh:31`, `scripts/perf-size.sh:15`, `.esp-dev.yml:3`

**Interfaces :**
- Consomme : rien (première tâche)
- Produit : les macros `ROWS0..ROWS3`, `COLS0..COLS6`, `MATRIX_ROWS`,
  `MATRIX_COLS`, `BOARD_NRF_*`, `BOARD_LINK_TX`, `BOARD_LINK_RX`,
  `BOARD_LINK_5V_EN`, `BOARD_LINK_SWAP_TX_RX`, `BOARD_VBAT_SENSE_GPIO`,
  `BOARD_TRACK_SDA_GPIO`, `BOARD_TRACK_SCL_GPIO`, `BOARD_TRACK_RDY_GPIO` ;
  le symbole Kconfig `CONFIG_KASE_NIPHAR_MASTER` ; la suite de tests
  `test_niphar_left_pins()`.

- [ ] **Étape 1 : écrire le test de contrat (il doit être rouge)**

Créer `test/test_niphar_left_pins.c` :

```c
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
```

Enregistrer la suite. Dans `test/CMakeLists.txt`, ajouter `test_niphar_left_pins.c`
juste avant `test_hid_dedup.c`. Dans `test/test_main.c`, ajouter
`extern void test_niphar_left_pins(void);` près des autres `extern`, et
`test_niphar_left_pins();` dans le corps, avant `test_hid_dedup();`.

- [ ] **Étape 2 : lancer le test, vérifier qu'il échoue**

```bash
./scripts/check.sh --host-only --force
```

Attendu : ROUGE. `.git/tripwire/last-fail.log` doit contenir
`fatal error: ../boards/niphar_left/board.h: No such file or directory`.

- [ ] **Étape 3 : créer le board de la moitié gauche**

Créer `boards/niphar_left/board.h` :

```c
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

/* ── Pas d'écran sur la gauche (connecteur J12 non peuplé) ── */
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
```

Créer `boards/niphar_left/board_keymap.c` — QWERTY minimal sur 4×7, les deux
positions vides de chaque rangée basse en `K_NO` :

```c
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
        {K_LCTRL, K_LWIN, K_LALT, MO_L1, K_SPACE, XXXXXXX, XXXXXXX},
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
```

Créer `boards/niphar_left/board_layout.c` sur le modèle de
`boards/kase_v2/board_layout.c` :

```c
#include "board.h"

#define _STR(x) #x
#define STR(x)  _STR(x)

#include "../niphar_layout.inc"
```

Créer `boards/niphar_layout.inc` en copiant la structure de
`boards/kase_layout.inc` et en l'adaptant à une grille 4×7 (26 touches). Lire
`boards/kase_layout.inc` d'abord : il définit le JSON de layout renvoyé par
`KS_CMD_LAYOUT_GET`, et sa forme exacte doit être respectée sous peine de casser
le contrôleur côté hôte.

- [ ] **Étape 4 : lancer le test, vérifier qu'il passe**

```bash
./scripts/check.sh --host-only --force
```

Attendu : VERT, et la sortie contient
`-- brochage Niphargus GAUCHE (contrat netlist 2026-08-06) --`.

- [ ] **Étape 5 : déclarer le rôle Kconfig**

Dans `main/Kconfig.projbuild`, après le bloc `KASE_KBD_WIRELESS`, ajouter :

```
config KASE_NIPHAR_MASTER
    bool "Niphargus: this board is the LEFT half (master)"
    default n
    depends on KASE_DEVICE_ROLE_KEYBOARD
    help
      Moitié gauche du Niphargus. Elle réutilise le rôle KEYBOARD (matrice
      locale, moteur keymap, USB HID, CDC binaire) et y ajoute ce qui est propre
      au split : réception de la matrice de la moitié droite (lien TRRS en
      filaire, nRF24 en sans-fil) et émission du rapport HID final vers le
      dongle. C'est la seule moitié qui porte un moteur keymap.
```

- [ ] **Étape 6 : créer le sdkconfig du board**

Créer `sdkconfig.defaults.niphar_left` :

```
CONFIG_KASE_DEVICE_ROLE_KEYBOARD=y
CONFIG_KASE_NIPHAR_MASTER=y

# Pas de BLE : le budget d'alimentation l'interdit (HT7833, pics radio hors
# budget). La config et les mises à jour passent par USB.
CONFIG_KASE_HAS_BLE=n

CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

Vérifier la valeur de flash contre le module réel : le contrat matériel indique
un ESP32-S3-WROOM-1-N8R2, donc **8 MB** de flash — pas 16 comme les KaSe. Si
`partitions.csv` (qui vise 16 MB) ne rentre pas, créer
`partitions_niphar.csv` en réduisant `ota_0` et `storage`, et le référencer ici.

- [ ] **Étape 7 : ajouter le board aux listes**

- `scripts/check.sh:31` → `ALL_VARIANTS=(kase_v1 kase_v2 kase_v2_debug kase_dongle niphar_left)`
- `scripts/perf-size.sh:15` → même liste
- `.esp-dev.yml:3` → `boards: [kase_v1, kase_v2, kase_v2_debug, kase_dongle, niphar_left]`
  et corriger le commentaire de tête (« 4 boards » → « 5 boards »)

- [ ] **Étape 8 : compiler le board**

```bash
source ~/esp/esp-idf/export.sh
./scripts/check.sh --board niphar_left --force
```

Attendu : VERT. Si le compilateur réclame une macro `BOARD_*` absente de
`board.h` (les rôles KEYBOARD tirent du code display et sommeil), l'ajouter avec
la valeur du contrat matériel — et si le contrat ne la couvre pas, la désactiver
plutôt que d'inventer une valeur (par exemple `BOARD_HAS_LED_STRIP 0`). Ne jamais
inventer un numéro de GPIO.

- [ ] **Étape 9 : commit**

```bash
git add test/test_niphar_left_pins.c test/CMakeLists.txt test/test_main.c \
        boards/niphar_left boards/niphar_layout.inc \
        sdkconfig.defaults.niphar_left main/Kconfig.projbuild \
        scripts/check.sh scripts/perf-size.sh .esp-dev.yml
git commit -m "feat(niphar): moitié gauche — brochage verrouillé par test de contrat

Les deux tables de pins du contrat matériel diffèrent par des permutations de
routage et aucune compilation ne détecte une inversion. Comme les cartes ne sont
pas arrivées, test_niphar_left_pins.c encode le contrat et le compare à board.h
valeur par valeur : c'est la seule barrière avant le banc. Il vérifie aussi
qu'aucun pin ne tombe sur un GPIO non câblé et qu'aucun GPIO matrice n'est
utilisé deux fois.

La gauche réutilise le rôle KEYBOARD (matrice locale, moteur, USB HID, CDC) et
n'ajoute que KASE_NIPHAR_MASTER."
```

---

### Tâche 2 : Moitié droite — brochage verrouillé, rôle esclave, build

**Fichiers :**
- Créer : `test/test_niphar_right_pins.c`
- Créer : `boards/niphar_right/board.h`, `board_keymap.c`, `board_layout.c`
- Créer : `sdkconfig.defaults.niphar_right`
- Créer : `main/comm/cdc/cdc_niphar_slave_stubs.c`
- Modifier : `test/CMakeLists.txt`, `test/test_main.c`
- Modifier : `main/Kconfig.projbuild`, `main/CMakeLists.txt`
- Modifier : `scripts/check.sh:31`, `scripts/perf-size.sh:15`, `.esp-dev.yml:3`

**Interfaces :**
- Consomme : le motif de test et de board de la tâche 1
- Produit : `CONFIG_KASE_DEVICE_ROLE_NIPHAR_SLAVE`, la suite
  `test_niphar_right_pins()`, et les bouchons CDC du rôle esclave

- [ ] **Étape 1 : écrire le test de contrat de la droite**

Créer `test/test_niphar_right_pins.c`. Reprendre **intégralement** la structure de
`test_niphar_left_pins.c` (stubs GPIO compris — le fichier doit se lire seul),
en changeant l'include pour `../boards/niphar_right/board.h` et les valeurs pour
la table DROITE :

```c
static void test_right_matrix_table(void)
{
    /* Table DROITE du contrat — différente de la gauche, ce n'est PAS une
     * symétrie : row1/row2/row3 et col4/col5/col6 sont permutés. */
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
```

Le test de la droite remplace en outre le bloc trackpad par l'écran, et inverse
l'assertion de swap :

```c
static void test_right_display_pins(void)
{
    /* Sharp LS011B7DH03 : CS ACTIF HAUT, write-only, LSB-first. */
    TEST_ASSERT_EQ(BOARD_LCD_CS_GPIO, 14, "CS de l'écran Sharp");
    TEST_ASSERT_EQ(BOARD_LCD_CS_ACTIVE_HIGH, 1, "CS actif HAUT, pas bas");
    /* L'écran partage le SPI de la nRF24. */
    TEST_ASSERT_EQ(BOARD_NRF_SCK,  38, "SPI SCK partagé écran + radio");
    TEST_ASSERT_EQ(BOARD_NRF_MOSI, 40, "SPI MOSI partagé écran + radio");
}

static void test_right_does_not_swap_the_link_uart(void)
{
    /* C'est la GAUCHE qui swappe. Si les deux swappent, ou aucune, deux TX se
     * retrouvent en conflit sur le même fil. */
    TEST_ASSERT_EQ(BOARD_LINK_SWAP_TX_RX, 0, "la droite ne swappe pas");
}
```

Enregistrer la suite dans `test/CMakeLists.txt` et `test/test_main.c` comme en
tâche 1.

- [ ] **Étape 2 : lancer le test, vérifier qu'il échoue**

```bash
./scripts/check.sh --host-only --force
```

Attendu : ROUGE, `../boards/niphar_right/board.h: No such file or directory`.

- [ ] **Étape 3 : créer le board de la moitié droite**

Créer `boards/niphar_right/board.h` en repartant du fichier de la gauche, avec
**la table droite**, sans le bloc trackpad, avec le bloc écran, et
`BOARD_LINK_SWAP_TX_RX 0` :

```c
/* ── Matrice — table DROITE (U5). Différente de la gauche. ── */
#define ROWS0  GPIO_NUM_2
#define ROWS1  GPIO_NUM_12
#define ROWS2  GPIO_NUM_4
#define ROWS3  GPIO_NUM_5

#define COLS0  GPIO_NUM_6
#define COLS1  GPIO_NUM_7
#define COLS2  GPIO_NUM_8
#define COLS3  GPIO_NUM_9
#define COLS4  GPIO_NUM_11
#define COLS5  GPIO_NUM_10
#define COLS6  GPIO_NUM_1

/* ── Écran Sharp LS011B7DH03 (module type nice!view, J4) ──────
 * CS ACTIF HAUT, write-only, LSB-first, VCOM logiciel à basculer (EXTCOMIN
 * géré par le module). Partage le SPI de la nRF24. */
#define BOARD_LCD_CS_GPIO         GPIO_NUM_14
#define BOARD_LCD_CS_ACTIVE_HIGH  1
#define BOARD_LCD_WIDTH           160
#define BOARD_LCD_HEIGHT          68

/* ── Lien : la DROITE ne swappe pas (c'est la gauche qui le fait) ── */
#define BOARD_LINK_SWAP_TX_RX  0
```

Vérifier les dimensions de l'écran contre la fiche du module avant de les
figer ; si elles ne sont pas connues au moment de l'écriture, ne pas inventer —
retirer les deux `#define` de taille et les poser en brick B6, quand le driver
les utilisera vraiment.

`board_keymap.c` et `board_layout.c` : mêmes fichiers que la gauche, avec la
moitié droite du clavier (colonnes miroir).

- [ ] **Étape 4 : lancer le test, vérifier qu'il passe**

```bash
./scripts/check.sh --host-only --force
```

Attendu : VERT, avec les deux suites de brochage dans la sortie.

- [ ] **Étape 5 : déclarer le rôle esclave**

Dans `main/Kconfig.projbuild`, ajouter dans le `choice KASE_DEVICE_ROLE` :

```
config KASE_DEVICE_ROLE_NIPHAR_SLAVE
    bool "Niphargus right half (slave: matrix + Sharp LCD, no keymap engine)"
```

et compléter l'aide du `choice` avec la ligne correspondante.

- [ ] **Étape 6 : écrire les bouchons CDC du rôle esclave**

La droite ne compile ni display ni BLE, mais `cdc_binary_cmds.c` référence leurs
symboles. Créer `main/comm/cdc/cdc_niphar_slave_stubs.c` en reprenant le motif de
`main/comm/cdc/cdc_dongle_stubs.c` — le lire d'abord et ne fournir que les
symboles que le linker réclame effectivement, un par un, en compilant entre
chaque. Ne pas copier en masse des bouchons dont on n'a pas besoin.

Dans `main/CMakeLists.txt`, après le bloc du rôle dongle :

```cmake
# Bouchons CDC du rôle esclave Niphargus (pas de display, pas de BLE)
if(CONFIG_KASE_DEVICE_ROLE_NIPHAR_SLAVE)
    list(APPEND srcs "comm/cdc/cdc_niphar_slave_stubs.c")
endif()
```

- [ ] **Étape 7 : sdkconfig et listes de boards**

Créer `sdkconfig.defaults.niphar_right` sur le modèle de celui de la gauche, avec
`CONFIG_KASE_DEVICE_ROLE_NIPHAR_SLAVE=y` à la place des deux lignes du maître, et
la console UART activée (la droite n'a pas d'USB utile en production, une console
au connecteur de prog est précieuse au bring-up) :

```
CONFIG_KASE_DEVICE_ROLE_NIPHAR_SLAVE=y
CONFIG_KASE_HAS_BLE=n
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
```

Ajouter `niphar_right` aux trois listes (`scripts/check.sh:31`,
`scripts/perf-size.sh:15`, `.esp-dev.yml:3`), et l'ajouter à
`console_none_boards` **non** — la droite a une console.

- [ ] **Étape 8 : compiler les 6 boards**

```bash
source ~/esp/esp-idf/export.sh
./scripts/check.sh --force
```

Attendu : VERT sur `kase_v1`, `kase_v2`, `kase_v2_debug`, `kase_dongle`,
`niphar_left`, `niphar_right`.

- [ ] **Étape 9 : commit**

```bash
git add test/test_niphar_right_pins.c test/CMakeLists.txt test/test_main.c \
        boards/niphar_right sdkconfig.defaults.niphar_right \
        main/comm/cdc/cdc_niphar_slave_stubs.c main/Kconfig.projbuild \
        main/CMakeLists.txt scripts/check.sh scripts/perf-size.sh .esp-dev.yml
git commit -m "feat(niphar): moitié droite — rôle esclave et brochage verrouillé

La table de pins de la droite n'est pas le miroir de celle de la gauche : row1,
row2, row3 et col4, col5, col6 sont permutés par le routage. test_niphar_right_pins.c
encode la table droite indépendamment et vérifie en plus que la droite NE swappe
PAS TX/RX — un swap des deux côtés, ou d'aucun, met deux sorties en conflit sur
le fil du TRRS.

Nouveau rôle NIPHAR_SLAVE : ni display ni BLE, d'où les bouchons CDC."
```

---

### Tâche 3 : Trame du lien inter-moitiés

**Fichiers :**
- Créer : `main/comm/link/link_frame.h`
- Créer : `test/test_link_frame.c`
- Modifier : `test/CMakeLists.txt`, `test/test_main.c`, `main/CMakeLists.txt`

**Interfaces :**
- Consomme : `RF_HALF_ROWS`, `RF_HALF_COLS`, `RF_HALF_BITMAP_BYTES` de
  `main/comm/rf/rf_packet.h`
- Produit : `link_frame_t`, `link_encode_matrix(uint8_t *buf, const uint8_t bitmap[5], uint8_t seq)`
  → `uint16_t`, `link_decode(const uint8_t *buf, uint16_t len, link_frame_t *out)`
  → `bool`, `link_crc8(const uint8_t *data, uint16_t len)` → `uint8_t`

La trame voyage sur une UART physique exposée au connecteur : elle doit être
resynchronisable après une coupure à chaud et détecter la corruption. D'où un
octet de début, une longueur, et un CRC-8 — le même polynôme que le protocole CDC
(`main/comm/cdc/cdc_binary_protocol.c`), pour n'avoir qu'une implémentation de
CRC à relire dans le dépôt.

- [ ] **Étape 1 : écrire les tests (ils doivent être rouges)**

Créer `test/test_link_frame.c` :

```c
/* Trame du lien inter-moitiés (TRRS / UART1).
 *
 * Le lien est exposé au connecteur : il est débranché à chaud, il prend de
 * l'ESD, et l'octet suivant peut arriver au milieu d'une trame. Le décodeur doit
 * donc rejeter proprement, jamais lire hors des bornes, et se resynchroniser.
 */
#include "test_framework.h"
#include "../main/comm/link/link_frame.h"

static void test_roundtrip_matrix(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {0xA5, 0x00, 0x3C, 0xFF, 0x01};
    uint8_t buf[LINK_FRAME_MAX];

    uint16_t n = link_encode_matrix(buf, bitmap, 42);
    TEST_ASSERT(n > 0, "l'encodage réussit");
    TEST_ASSERT(n <= LINK_FRAME_MAX, "la trame tient dans le buffer annoncé");

    link_frame_t out;
    TEST_ASSERT(link_decode(buf, n, &out), "le décodage réussit");
    TEST_ASSERT_EQ(out.type, LINK_TYPE_MATRIX, "type conservé");
    TEST_ASSERT_EQ(out.seq, 42, "seq conservé");
    for (int i = 0; i < RF_HALF_BITMAP_BYTES; i++)
        TEST_ASSERT_EQ(out.bitmap[i], bitmap[i], "bitmap conservé octet par octet");
}

static void test_rejects_bad_crc(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {1, 2, 3, 4, 5};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 7);

    buf[n - 1] ^= 0xFF;             /* corrompt le CRC */
    link_frame_t out;
    TEST_ASSERT(!link_decode(buf, n, &out), "CRC faux → rejet");

    uint16_t m = link_encode_matrix(buf, bitmap, 7);
    buf[3] ^= 0x01;                 /* corrompt la charge utile */
    TEST_ASSERT(!link_decode(buf, m, &out), "charge utile corrompue → rejet");
}

static void test_rejects_bad_sof(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {0};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 0);
    buf[0] = 0x00;
    link_frame_t out;
    TEST_ASSERT(!link_decode(buf, n, &out), "octet de début faux → rejet");
}

static void test_rejects_truncated(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {9, 9, 9, 9, 9};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 3);
    link_frame_t out;
    /* Toute troncature doit être rejetée, jamais lue au-delà du tampon. */
    for (uint16_t cut = 0; cut < n; cut++)
        TEST_ASSERT(!link_decode(buf, cut, &out), "trame tronquée → rejet");
    TEST_ASSERT(link_decode(buf, n, &out), "trame complète → acceptée");
}

static void test_rejects_length_lie(void)
{
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {1, 1, 1, 1, 1};
    uint8_t buf[LINK_FRAME_MAX];
    uint16_t n = link_encode_matrix(buf, bitmap, 1);
    buf[1] = 0xFF;                  /* longueur annoncée absurde */
    link_frame_t out;
    TEST_ASSERT(!link_decode(buf, n, &out), "longueur mensongère → rejet");
}

static void test_null_arguments(void)
{
    uint8_t buf[LINK_FRAME_MAX];
    const uint8_t bitmap[RF_HALF_BITMAP_BYTES] = {0};
    link_frame_t out;
    TEST_ASSERT_EQ(link_encode_matrix(NULL, bitmap, 0), 0, "buf NULL → 0");
    TEST_ASSERT_EQ(link_encode_matrix(buf, NULL, 0), 0, "bitmap NULL → 0");
    TEST_ASSERT(!link_decode(NULL, 8, &out), "buf NULL → rejet");
    TEST_ASSERT(!link_decode(buf, 8, NULL), "sortie NULL → rejet");
}

void test_link_frame(void)
{
    printf("\n-- trame du lien inter-moitiés --\n");
    test_roundtrip_matrix();
    test_rejects_bad_crc();
    test_rejects_bad_sof();
    test_rejects_truncated();
    test_rejects_length_lie();
    test_null_arguments();
}
```

Enregistrer dans `test/CMakeLists.txt` (`test_link_frame.c`) et `test/test_main.c`.

- [ ] **Étape 2 : lancer, vérifier l'échec**

```bash
./scripts/check.sh --host-only --force
```

Attendu : ROUGE, `link_frame.h: No such file or directory`.

- [ ] **Étape 3 : écrire l'implémentation minimale**

Créer `main/comm/link/link_frame.h` :

```c
/* Trame du lien inter-moitiés Niphargus (TRRS, UART1).
 *
 * Logique pure, entièrement en inline : aucune UART, aucun FreeRTOS. La couche
 * transport (B2b) appellera link_encode_matrix() pour émettre et link_decode()
 * sur ce qu'elle a resynchronisé.
 *
 * Format :
 *   [0] SOF 0x4E ('N')
 *   [1] longueur de la charge utile (type + seq + bitmap)
 *   [2] type
 *   [3] seq
 *   [4..8] bitmap 5 octets (format rf_packet, rows 0..3 utilisées)
 *   [9] CRC-8 sur les octets 1..8
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "rf_packet.h"   /* RF_HALF_BITMAP_BYTES — même bitmap qu'en RF */

#define LINK_SOF          0x4Eu
#define LINK_TYPE_MATRIX  0x01u

/* SOF + len + type + seq + bitmap + crc */
#define LINK_FRAME_MAX  (4 + RF_HALF_BITMAP_BYTES + 1)
#define LINK_PAYLOAD_MATRIX  (2 + RF_HALF_BITMAP_BYTES)   /* type + seq + bitmap */

typedef struct {
    uint8_t type;
    uint8_t seq;
    uint8_t bitmap[RF_HALF_BITMAP_BYTES];
} link_frame_t;

/* CRC-8, polynôme 0x07, init 0x00 — identique au protocole CDC binaire, pour
 * n'avoir qu'une seule implémentation de CRC à relire dans le dépôt. */
static inline uint8_t link_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

static inline uint16_t link_encode_matrix(uint8_t *buf,
                                          const uint8_t bitmap[RF_HALF_BITMAP_BYTES],
                                          uint8_t seq)
{
    if (buf == NULL || bitmap == NULL) return 0;
    buf[0] = LINK_SOF;
    buf[1] = LINK_PAYLOAD_MATRIX;
    buf[2] = LINK_TYPE_MATRIX;
    buf[3] = seq;
    memcpy(&buf[4], bitmap, RF_HALF_BITMAP_BYTES);
    buf[4 + RF_HALF_BITMAP_BYTES] = link_crc8(&buf[1], 3 + RF_HALF_BITMAP_BYTES);
    return LINK_FRAME_MAX;
}

static inline bool link_decode(const uint8_t *buf, uint16_t len, link_frame_t *out)
{
    if (buf == NULL || out == NULL) return false;
    if (len < 4 + 1) return false;                 /* plus court que le minimum */
    if (buf[0] != LINK_SOF) return false;

    uint8_t payload_len = buf[1];
    if (payload_len != LINK_PAYLOAD_MATRIX) return false;   /* seul type connu */
    uint16_t total = (uint16_t)(2 + payload_len + 1);
    if (len < total) return false;                 /* tronquée */

    if (link_crc8(&buf[1], (uint16_t)(1 + payload_len)) != buf[total - 1])
        return false;

    if (buf[2] != LINK_TYPE_MATRIX) return false;
    out->type = buf[2];
    out->seq  = buf[3];
    memcpy(out->bitmap, &buf[4], RF_HALF_BITMAP_BYTES);
    return true;
}
```

Ajouter `${CMAKE_CURRENT_SOURCE_DIR}/../main/comm/link` aux
`target_include_directories` de `test/CMakeLists.txt`, et `"comm/link"` aux
`INCLUDE_DIRS` de `main/CMakeLists.txt` (aucun `.c` à ajouter : le module est
entièrement en inline).

- [ ] **Étape 4 : lancer, vérifier que tout passe**

```bash
./scripts/check.sh --host-only --force
```

Attendu : VERT, avec `-- trame du lien inter-moitiés --` dans la sortie.

- [ ] **Étape 5 : prouver que le test mord**

Casser volontairement le décodeur — remplacer le contrôle de CRC par `if (0)` —
relancer `./scripts/check.sh --host-only --force`, constater le ROUGE sur
`test_rejects_bad_crc`, puis rétablir et revérifier le VERT. Un test qui ne mord
pas ne vaut rien : cette étape n'est pas optionnelle.

- [ ] **Étape 6 : commit**

```bash
git add main/comm/link/link_frame.h test/test_link_frame.c \
        test/CMakeLists.txt test/test_main.c main/CMakeLists.txt
git commit -m "feat(niphar): trame du lien inter-moitiés (logique pure)

Le lien TRRS est exposé au connecteur : débranchement à chaud, ESD, et un octet
qui arrive au milieu d'une trame. Le décodeur rejette donc SOF faux, longueur
mensongère, troncature et CRC faux, et ne lit jamais au-delà du tampon annoncé.

Le bitmap reste au format rf_packet (5x7, rows 0..3 utilisées) pour que le lien
filaire et le lien radio transportent exactement la même chose. CRC-8 polynôme
0x07 comme le CDC binaire : une seule implémentation de CRC à relire.

Mordant vérifié (contrôle de CRC neutralisé -> rouge -> revert)."
```

---

### Tâche 4 : Machine d'état de la poignée de main 5 V

**Fichiers :**
- Créer : `main/comm/link/link_handshake.h`
- Créer : `test/test_link_handshake.c`
- Modifier : `test/CMakeLists.txt`, `test/test_main.c`

**Interfaces :**
- Consomme : rien (logique pure autonome)
- Produit : `link_hs_state_t`, `link_hs_t`, `link_hs_init(link_hs_t *)`,
  `link_hs_step(link_hs_t *, link_hs_event_t, uint32_t now_ms)` →
  `link_hs_action_t`

Le contrat matériel est sans ambiguïté : `LINK_5V_EN` a un pull-down 100 k, le
5 V est mort par défaut, et **l'émetteur comme le récepteur doivent fermer leur
switch** pour qu'un courant passe. Une moitié à batterie morte n'est pas
réveillable par le TRRS — c'est assumé. La machine d'état encode exactement ça :
on ne ferme le switch qu'après avoir reconnu la moitié d'en face sur l'UART.

- [ ] **Étape 1 : écrire les tests**

Créer `test/test_link_handshake.c` :

```c
/* Poignée de main 5 V du lien inter-moitiés.
 *
 * Enjeu matériel : fermer le load switch avant d'avoir reconnu la moitié d'en
 * face, c'est mettre du 5 V sur un connecteur exposé — exactement le tueur
 * historique des splits que ce design cherche à éliminer. La règle testée ici
 * est donc : LINK_5V_EN ne se lève JAMAIS sans une reconnaissance aboutie, et il
 * retombe dès que la moitié d'en face se tait.
 */
#include "test_framework.h"
#include "../main/comm/link/link_handshake.h"

static void test_starts_dead(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "au repos au départ");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort au départ");
}

static void test_probe_then_ack_enables_5v(void)
{
    link_hs_t hs;
    link_hs_init(&hs);

    /* On a l'USB : on sonde. */
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 1000);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_SEND_PROBE, "présence USB → sonder");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V toujours mort pendant la sonde");

    /* La moitié d'en face répond. */
    a = link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 1050);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_ENABLE_5V, "pair reconnu → fermer le switch");
    TEST_ASSERT(link_hs_5v_enabled(&hs), "5 V vivant après reconnaissance");
    TEST_ASSERT_EQ(hs.state, LINK_HS_UP, "lien établi");
}

static void test_never_enables_without_ack(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);

    /* Du bruit sur la ligne, pas un ACK : rien ne doit se lever. */
    for (uint32_t t = 1; t < LINK_HS_PROBE_TIMEOUT_MS; t += 10) {
        link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, t);
        TEST_ASSERT(a != LINK_HS_ACT_ENABLE_5V, "aucun 5 V sans reconnaissance");
        TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V reste mort");
    }
}

static void test_probe_timeout_returns_to_idle(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);

    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, LINK_HS_PROBE_TIMEOUT_MS);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "pas de réponse → on abandonne");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "retour au repos");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort après abandon");
}

static void test_peer_silence_drops_5v(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 10);
    TEST_ASSERT(link_hs_5v_enabled(&hs), "5 V vivant");

    /* Le pair se tait : câble arraché. Le switch doit rouvrir. */
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, 10 + LINK_HS_PEER_TIMEOUT_MS);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_DISABLE_5V, "silence du pair → rouvrir");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort après arrachage");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "retour au repos");
}

static void test_peer_traffic_keeps_link_up(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 10);

    /* Trafic régulier : le lien tient indéfiniment. */
    for (uint32_t t = 10; t < 10 * LINK_HS_PEER_TIMEOUT_MS; t += LINK_HS_PEER_TIMEOUT_MS / 2) {
        link_hs_step(&hs, LINK_HS_EV_PEER_FRAME, t);
        TEST_ASSERT(link_hs_5v_enabled(&hs), "le trafic maintient le lien");
    }
}

static void test_usb_lost_drops_5v(void)
{
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_step(&hs, LINK_HS_EV_USB_PRESENT, 0);
    link_hs_step(&hs, LINK_HS_EV_PEER_ACK, 10);

    /* Débranché : on ne nourrit plus personne. */
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_USB_GONE, 20);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_DISABLE_5V, "USB parti → rouvrir");
    TEST_ASSERT(!link_hs_5v_enabled(&hs), "5 V mort sans USB");
}

static void test_no_probe_without_usb(void)
{
    /* Sur batterie seule, on ne sonde pas : on n'a rien à donner. */
    link_hs_t hs;
    link_hs_init(&hs);
    link_hs_action_t a = link_hs_step(&hs, LINK_HS_EV_TICK, 500);
    TEST_ASSERT_EQ(a, LINK_HS_ACT_NONE, "pas d'USB → pas de sonde");
    TEST_ASSERT_EQ(hs.state, LINK_HS_IDLE, "on reste au repos");
}

void test_link_handshake(void)
{
    printf("\n-- poignée de main 5 V du lien --\n");
    test_starts_dead();
    test_probe_then_ack_enables_5v();
    test_never_enables_without_ack();
    test_probe_timeout_returns_to_idle();
    test_peer_silence_drops_5v();
    test_peer_traffic_keeps_link_up();
    test_usb_lost_drops_5v();
    test_no_probe_without_usb();
}
```

- [ ] **Étape 2 : lancer, vérifier l'échec**

```bash
./scripts/check.sh --host-only --force
```

Attendu : ROUGE, `link_handshake.h: No such file or directory`.

- [ ] **Étape 3 : écrire la machine d'état**

Créer `main/comm/link/link_handshake.h` :

```c
/* Poignée de main du lien inter-moitiés Niphargus.
 *
 * LINK_5V_EN pilote un load switch SiP32431 avec un pull-down 100 k : le 5 V est
 * MORT par défaut. Émetteur et récepteur doivent tous deux fermer leur switch
 * pour qu'un courant passe, donc un branchement à chaud ne peut pas produire
 * d'étincelle. Une moitié à batterie vide n'est pas réveillable par le TRRS —
 * assumé au design matériel.
 *
 * Logique pure : l'horloge et les événements sont passés en argument, aucune
 * GPIO n'est touchée ici. L'appelant traduit les actions en gpio_set_level().
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LINK_HS_PROBE_TIMEOUT_MS  200   /* attente d'un ACK après la sonde */
#define LINK_HS_PEER_TIMEOUT_MS   500   /* silence du pair toléré, lien établi */

typedef enum {
    LINK_HS_IDLE = 0,   /* 5 V mort, rien en cours */
    LINK_HS_PROBING,    /* sonde envoyée, on attend l'ACK */
    LINK_HS_UP,         /* pair reconnu, 5 V fermé */
} link_hs_state_t;

typedef enum {
    LINK_HS_EV_TICK = 0,     /* passage du temps, rien d'autre */
    LINK_HS_EV_USB_PRESENT,  /* le câble hôte vient d'apparaître */
    LINK_HS_EV_USB_GONE,     /* le câble hôte a disparu */
    LINK_HS_EV_PEER_ACK,     /* la moitié d'en face a répondu à la sonde */
    LINK_HS_EV_PEER_FRAME,   /* trame valide reçue du pair (garde le lien vivant) */
} link_hs_event_t;

typedef enum {
    LINK_HS_ACT_NONE = 0,
    LINK_HS_ACT_SEND_PROBE,   /* émettre la sonde « t'es bien ma moitié ? » */
    LINK_HS_ACT_ENABLE_5V,    /* fermer le load switch */
    LINK_HS_ACT_DISABLE_5V,   /* rouvrir le load switch */
} link_hs_action_t;

typedef struct {
    link_hs_state_t state;
    uint32_t        since_ms;    /* entrée dans l'état courant */
    uint32_t        last_peer_ms;/* dernier signe de vie du pair */
    bool            usb;         /* câble hôte présent */
    bool            en_5v;       /* état commandé du load switch */
} link_hs_t;

static inline void link_hs_init(link_hs_t *h)
{
    h->state = LINK_HS_IDLE;
    h->since_ms = 0;
    h->last_peer_ms = 0;
    h->usb = false;
    h->en_5v = false;
}

static inline bool link_hs_5v_enabled(const link_hs_t *h) { return h->en_5v; }

static inline link_hs_action_t link_hs_step(link_hs_t *h, link_hs_event_t ev,
                                            uint32_t now_ms)
{
    /* Perte de l'USB : on coupe partout, immédiatement. */
    if (ev == LINK_HS_EV_USB_GONE) {
        h->usb = false;
        if (h->en_5v) {
            h->en_5v = false;
            h->state = LINK_HS_IDLE;
            h->since_ms = now_ms;
            return LINK_HS_ACT_DISABLE_5V;
        }
        h->state = LINK_HS_IDLE;
        h->since_ms = now_ms;
        return LINK_HS_ACT_NONE;
    }

    if (ev == LINK_HS_EV_USB_PRESENT) h->usb = true;
    if (ev == LINK_HS_EV_PEER_ACK || ev == LINK_HS_EV_PEER_FRAME)
        h->last_peer_ms = now_ms;

    switch (h->state) {
    case LINK_HS_IDLE:
        /* On ne sonde que si on a du courant à donner. */
        if (ev == LINK_HS_EV_USB_PRESENT) {
            h->state = LINK_HS_PROBING;
            h->since_ms = now_ms;
            return LINK_HS_ACT_SEND_PROBE;
        }
        return LINK_HS_ACT_NONE;

    case LINK_HS_PROBING:
        if (ev == LINK_HS_EV_PEER_ACK) {
            h->state = LINK_HS_UP;
            h->since_ms = now_ms;
            h->en_5v = true;
            return LINK_HS_ACT_ENABLE_5V;
        }
        if ((uint32_t)(now_ms - h->since_ms) >= LINK_HS_PROBE_TIMEOUT_MS) {
            h->state = LINK_HS_IDLE;
            h->since_ms = now_ms;
        }
        return LINK_HS_ACT_NONE;

    case LINK_HS_UP:
        if ((uint32_t)(now_ms - h->last_peer_ms) >= LINK_HS_PEER_TIMEOUT_MS) {
            h->en_5v = false;
            h->state = LINK_HS_IDLE;
            h->since_ms = now_ms;
            return LINK_HS_ACT_DISABLE_5V;
        }
        return LINK_HS_ACT_NONE;
    }
    return LINK_HS_ACT_NONE;
}
```

Enregistrer `test_link_handshake.c` dans `test/CMakeLists.txt` et
`test/test_main.c`.

- [ ] **Étape 4 : lancer, vérifier que tout passe**

```bash
./scripts/check.sh --host-only --force
```

Attendu : VERT, avec `-- poignée de main 5 V du lien --` dans la sortie.

- [ ] **Étape 5 : prouver que le test mord**

Remplacer dans `LINK_HS_IDLE` le passage par `LINK_HS_PROBING` par un passage
direct à `LINK_HS_UP` avec `h->en_5v = true` — c'est exactement la faute qu'on
cherche à rendre impossible : lever le 5 V sans reconnaissance. Relancer, vérifier
le ROUGE sur `test_never_enables_without_ack`, puis rétablir et revérifier le VERT.

- [ ] **Étape 6 : commit**

```bash
git add main/comm/link/link_handshake.h test/test_link_handshake.c \
        test/CMakeLists.txt test/test_main.c
git commit -m "feat(niphar): machine d'état de la poignée de main 5 V

LINK_5V_EN pilote un load switch avec pull-down 100 k : le 5 V est mort par
défaut, et les deux moitiés doivent fermer leur switch pour qu'un courant passe.
La règle que cette machine d'état rend structurelle : on ne lève JAMAIS le 5 V
sans avoir reconnu la moitié d'en face sur l'UART, et on le retombe dès que le
pair se tait ou que l'USB disparaît. C'est la réponse au tueur historique des
splits — le court-circuit de branchement à chaud.

Logique pure : horloge et événements en argument, aucune GPIO touchée.
Mordant vérifié (levée du 5 V sans ACK -> rouge -> revert)."
```

---

## Après la phase 1

Ce qui reste, et pourquoi ça n'est pas ici : `B1b` (bring-up du scan et du HID),
`B3` (radio esclave → maître), `B4` (bascule PRX/PTX — **à traiter en spike
d'abord**, son résultat commande la répartition), `B5` (trackpad), `B6` (écran),
`B7` (énergie). Toutes demandent du matériel pour être validées autrement qu'à
l'aveugle.

`B8` (config USB) est satisfaite par construction : la moitié gauche réutilise le
rôle `KEYBOARD`, donc elle hérite du CDC binaire complet sans une ligne de code.
