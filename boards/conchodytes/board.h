#ifndef BOARD_H
#define BOARD_H

/* Conchodytes — la souris du set KaSe.
 *
 * PCB de remplacement pour coque Logitech M100 : ESP32-S3-WROOM-1U, capteur
 * PMW3389DM-T3QU, nRF24L01+ vers le slot 2 du dongle.
 * Matériel : ~/Documents/GitHub/Conchodytes
 * Design   : docs/superpowers/specs/2026-08-25-conchodytes-firmware-design.md
 *
 * Brochage relevé à la netlist au `kicad-cli export netlist` le 2026-08-25, et
 * vérifié sur carte le même jour. NE PAS le recopier du README du dépôt
 * matériel : celui-ci inversait gauche et droite sur les contacts NC.
 * Verrouillé par test/test_conchodytes_pins.c.
 */

#ifdef ESP_PLATFORM
#include "driver/gpio.h"
#include "driver/spi_master.h"
#endif

/* ── Identité produit ── */
#define GATTS_TAG           "Conchodytes"
#define MANUFACTURER_NAME   "Mae"
#define PRODUCT_NAME        "Conchodytes"
#define SERIAL_NUMBER       "N/A"
#define MODULE_ID           0xC0   /* souris — distinct du dongle 0xD0 et des moitiés 0x01/0x02 */

/* ── Pas de matrice, pas d'écran, pas de trackpad ───────────────
 *
 * Une souris n'a pas de touches en matrice : ses trois clics sont lus
 * individuellement, par paires NO+NC (voir plus bas). Aucun MATRIX_ROWS,
 * MATRIX_COLS, ROWSn ni COLSn ici — le moteur keymap n'est pas compilé pour ce
 * rôle (main/CMakeLists.txt), donc rien n'en a besoin, et un board.h qui
 * déclarerait un clavier imaginaire finirait par tromper quelqu'un.
 *
 * L'absence de BOARD_DISPLAY_BACKEND_* est également volontaire : le
 * CMakeLists racine détecte cette absence et saute les sources d'affichage.
 */

/* ── Bus SPI, PARTAGÉ entre le capteur et la radio ──────────────
 *
 * SCK/MOSI/MISO sont communs au PMW3389 et au nRF24, chacun derrière 100 Ω en
 * série (R30/R18/R16 côté carte — sans effet à ces fréquences).
 *
 * Les deux appareils n'ont PAS le même mode SPI : mode 0 pour le nRF24,
 * mode 3 pour le PMW3389. ESP-IDF reconfigure le mode par appareil, mais
 * l'exclusion mutuelle reste à notre charge : un seul CS bas à la fois, les
 * deux tirés haut au démarrage avant toute initialisation, et le bus acquis
 * pendant le téléversement du SROM (~61 ms) sous peine de le corrompre.
 */
#define BOARD_NRF_SPI_HOST   SPI2_HOST
#define BOARD_NRF_SCK        GPIO_NUM_38
#define BOARD_NRF_MISO       GPIO_NUM_39
#define BOARD_NRF_MOSI       GPIO_NUM_40

/* ── Radio nRF24L01+ ────────────────────────────────────────────
 * Slot 2 du dongle. boards/kase_dongle/board.h annote BOARD_NRF2_* comme
 * « slot souris (Conchodytes), canal 0x52 par défaut » : les deux côtés
 * doivent s'accorder ou le lien ne s'établit jamais. */
#define BOARD_NRF_CSN        GPIO_NUM_2
#define BOARD_NRF_CE         GPIO_NUM_1
#define BOARD_NRF_IRQ        GPIO_NUM_41
#define BOARD_NRF_CLOCK_HZ   (8 * 1000 * 1000)
#define BOARD_NRF_CHANNEL    0x52
#define BOARD_NRF_ADDR_SUFFIX 0x02   /* 0x01 = clavier, 0x02 = souris (rf_slot.h) */

/* ── Alias de brochage : DEUX conventions coexistent dans ce dépôt ──────────
 *
 * Les board.h Niphargus nomment les lignes `BOARD_NRF_SCK`, `BOARD_NRF_CSN`…
 * tandis que comm/rf/kbd_relay_tx.c attend `BOARD_NRF_SPI_SCK`,
 * `BOARD_NRF_CSN_GPIO`… et retombe sur des valeurs de repli quand il ne les
 * trouve pas.
 *
 * ⚠ Ces valeurs de repli sont GPIO35/37/36/34/33/38 — choisies pour le PCB du
 * KaSe V2, sans aucun rapport avec cette carte. Un board.h qui ne déclare que
 * la première convention compile donc parfaitement et pilote les mauvaises
 * broches, en silence. D'où ces alias : ils ne dupliquent rien, ils traduisent.
 */
#define BOARD_NRF_SPI_SCK       BOARD_NRF_SCK
#define BOARD_NRF_SPI_MISO      BOARD_NRF_MISO
#define BOARD_NRF_SPI_MOSI      BOARD_NRF_MOSI
#define BOARD_NRF_SPI_CLOCK_HZ  BOARD_NRF_CLOCK_HZ
#define BOARD_NRF_CSN_GPIO      BOARD_NRF_CSN
#define BOARD_NRF_CE_GPIO       BOARD_NRF_CE
#define BOARD_NRF_IRQ_GPIO      BOARD_NRF_IRQ

/* ── Capteur PMW3389DM-T3QU ─────────────────────────────────────
 *
 * Datasheet PixArt Version 1.0 | 07 sep 2017. Validé sur carte le 2026-08-25 :
 * Product_ID 0x47, Inverse 0xB8, Revision 0x01, SROM_ID 0xE8 après
 * téléversement, déplacement lu.
 *
 * ⚠ Ce n'est PAS un PMW3360, malgré ce que disent encore les noms de fichiers
 * du dépôt matériel. Blob SROM différent, plan de registres partiellement
 * différent, temporisations SPI plus longues (tSRAD 160 µs contre 35).
 */
#define BOARD_SNS_NCS_GPIO     GPIO_NUM_17
#define BOARD_SNS_MOTION_GPIO  GPIO_NUM_18
#define BOARD_SNS_SPI_MODE     3            /* CPOL=1, CPHA=1 */
#define BOARD_SNS_CLOCK_HZ     (2 * 1000 * 1000)  /* fSCLK max, Table 4 p. 15 */

/* Résolution du capteur, en cpi.
 *
 * 1000 cpi est la valeur de la M100 d'origine, dont cette carte reprend la
 * coque : garder la même évite que le remplacement se sente sous la main.
 * Le PMW3389 monte à 16000 (datasheet p. 1) et c'est près de ce plafond qu'il
 * démarre si personne ne lui dit rien — le firmware ne le réglait pas, d'où un
 * curseur bien trop rapide au banc le 2026-08-26. */
#define BOARD_SNS_CPI          1000

/* Le capteur est-il monté tourné de 180° par rapport à l'axe de la souris ?
 *
 * ⚠ SUR LA v1, OUI — ET ÇA CHANGE À LA v2. Mesuré le 2026-08-25 (NOTES-V2 §1,
 * dépôt Conchodytes) : geste vers la droite → X négatif, biais 100,0 % sur 76
 * échantillons ; geste vers l'avant → Y positif, biais 99,8 % sur 71. Les DEUX
 * axes retournés et chacun resté propre — signature d'une rotation de 180° et
 * non d'un miroir, qui n'en inverserait qu'un.
 *
 * ⚠ METTRE À 0 QUAND LE LAYOUT v2 TOURNERA U2 DE 180°. La rotation de
 * l'empreinte et cette constante corrigent LE MÊME défaut : les laisser toutes
 * deux actives le réintroduirait à l'envers. La v2 est conditionnée à ce que la
 * lentille LM19-LSI accepte la rotation dans la coque M100 — tant que ce n'est
 * pas tranché, la correction vit ici. */
#define BOARD_SNS_ROT_180      1

/* Lissage adaptatif du déplacement, en Q8 (256 = 1,0). À 0, aucun lissage.
 *
 * ⚠ PALLIATIF D'UN DÉFAUT OPTIQUE, pas une correction. Le capteur tremble parce
 * que son SQUAL est à 30-55 pour ~80 attendu — voir NOTES-V2 §8 (dépôt
 * Conchodytes). Si l'optique est réparée, REMETTRE `ALPHA_MIN` à 256 (soit
 * aucun lissage) plutôt que de laisser un retard qu'on ne paie plus pour rien.
 *
 * ALPHA_MIN gouverne le lissage À L'ARRÊT ET AUX GESTES LENTS : plus il est bas,
 * plus c'est lissé et plus le retard est long. 38/256 ≈ 0,15, soit une constante
 * de temps de 4 ms / 0,15 ≈ 27 ms à 250 Hz.
 *
 * VITESSE_MAX est le seuil, en comptes par échantillon, au-delà duquel on ne
 * lisse PLUS DU TOUT. 8 comptes à 250 Hz = 2000 comptes/s ≈ 5 cm/s : tout geste
 * franc passe donc sans le moindre retard, seul le bruit de faible amplitude est
 * atténué. */
#define BOARD_SNS_LISSAGE_ALPHA_MIN   38
#define BOARD_SNS_LISSAGE_VITESSE_MAX 8

/* ── Clics : trois SPDT, anti-rebond par le contact NC ──────────
 *
 * COM à la masse, NO et NC tirés chacun au 3,3 V par 10 k (R105-R107 et
 * R108-R110). Le firmware lit LES DEUX contacts :
 *
 *   repos  : NC collé sur COM -> bas ; NO ouvert -> haut
 *   appuyé : NO collé sur COM -> bas ; NC ouvert -> haut
 *   rebond : les DEUX hauts — on garde l'état précédent
 *
 * Vérifié sur carte le 2026-08-25 : sur 24 transitions, AUCUN front parasite.
 * Pas de double-clic sans un seul filtre temporel ni une constante à régler —
 * mais cela n'est vrai QUE si NO et NC appartiennent au même bouton.
 *
 * ⚠ La DURÉE du rebond n'est pas mesurée : la campagne croyait scruter à 1 kHz
 * alors que CONFIG_FREERTOS_HZ vaut 100 par défaut, soit 10 ms par échantillon.
 * Voir main/input/mouse_buttons.h.
 *
 * ⚠ Le README du dépôt matériel annonçait LEFT_NC=GPIO4 et RIGHT_NC=GPIO5.
 * La netlist dit l'inverse : GPIO4 -> SW2.3 (clic DROIT, R109), GPIO5 -> SW1.3
 * (clic GAUCHE, R108). Vérifié sur carte : appui sur le seul clic gauche a
 * produit 26 fronts sur G et 0 sur D.
 */
#define BOARD_SW_LEFT_GPIO      GPIO_NUM_10
#define BOARD_SW_LEFT_NC_GPIO   GPIO_NUM_5
#define BOARD_SW_RIGHT_GPIO     GPIO_NUM_11
#define BOARD_SW_RIGHT_NC_GPIO  GPIO_NUM_4
#define BOARD_SW_MID_GPIO       GPIO_NUM_12
#define BOARD_SW_MID_NC_GPIO    GPIO_NUM_6

/* ── Molette : NON FONCTIONNELLE EN v1 — défaut de conception ────
 *
 * ⚠⚠ LQ1 EST CÂBLÉ À L'ENVERS SUR LA v1. Établi le 2026-08-26 en confrontant le
 * schéma d'origine de la M100 (`USB-Mouse-main/Mouse.sch`) au câblage réel.
 *
 * LQ1 n'est PAS un double phototransistor A/COM/B — c'est un capteur à trois
 * fils VCC/GND/DATA (marquage `H6Y07`) :
 *
 *   broche | M100 d'origine        | Conchodytes v1
 *   -------|-----------------------|------------------------
 *      1   | Pin_4 = sortie DATA   | ENC_A + tirage 10 k   ✅
 *      2   | Pin_2J = VCC          | la MASSE              ❌
 *      3   | GNDREF = GND          | ENC_B + tirage 10 k   ❌
 *
 * Le composant est donc alimenté à l'envers. Le courant passe par sa diode de
 * substrat interne et clampe les deux nets à 0,65 V, quelle que soit la
 * lumière. Vérifié sur DEUX composants distincts : c'est le câblage, pas eux.
 *
 * L'erreur vient du symbole `Optical_Mouse:LQ` hérité du fork, dont les broches
 * ne portent aucun nom de fonction — seulement « 1, 2, 3 ».
 *
 * ⚠ ET UNE SEULE SORTIE DATA = UN SEUL CANAL. La quadrature en exige deux. Le
 * principe même de lecture de la molette est à revoir en v2, pas seulement son
 * câblage. Question ouverte : voir Conchodytes/NOTES-V2.md §1bis.
 *
 * Conséquence pour ce firmware : le décodage de quadrature
 * (input/mouse_wheel.c) est vérifié sur l'hôte uniquement, et ne pourra pas
 * être exercé sur cette carte. Les macros ci-dessous décrivent le câblage
 * ACTUEL, qui est faux — elles changeront avec la v2.
 */
#define BOARD_ENC_A_GPIO     GPIO_NUM_7
#define BOARD_ENC_B_GPIO     GPIO_NUM_9

/* ── Jauge batterie ─────────────────────────────────────────────
 * Pont diviseur 1M/1M + 100 nF (R64/R67/C21), rapport ÷2 : 4,2 V pleine
 * charge -> 2,1 V à l'entrée. Les 1 MΩ sont un choix de consommation — 2,1 µA
 * de fuite permanente contre 21 µA avec du 100 k — viable parce que C21 fournit
 * la charge d'échantillonnage de l'ADC.
 *
 * ⚠ GPIO13 = ADC2_CH2, et sur ESP32-S3 l'ADC2 est partagé avec le driver WiFi :
 * tant que cette mesure est là, le WiFi est interdit sur cette carte. Sans
 * conséquence aujourd'hui — la radio est un nRF24 — mais c'est une hypothèque.
 * NOTES-V2.md point 7 prévoit GPIO8 / ADC1_CH7 en v2.
 */
#define BOARD_VBAT_SENSE_GPIO   GPIO_NUM_13

/* ── Pas d'écran, pas de bandeau LED ────────────────────────── */
#define BOARD_DISPLAY_SLEEP_MS    0
#define BOARD_SLEEP_MINS          0
#define BOARD_HAS_LED_STRIP       0

/* ── Identification USB ─────────────────────────────────────────
 * Même VID/PID de développement que le reste du set jusqu'à la première
 * publication. À migrer vers pid.codes (VID 0x1209) avant la v4.0. */
#define BOARD_USB_VID             0x303A
#define BOARD_USB_PID             0x4002

#endif /* BOARD_H */
